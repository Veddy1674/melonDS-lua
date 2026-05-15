/*
    Copyright 2016-2026 melonDS team

    This file is part of melonDS.

    melonDS is free software: you can redistribute it and/or modify it under
    the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    melonDS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with melonDS. If not, see http://www.gnu.org/licenses/.
*/

#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <string.h>

#include <optional>
#include <string>

#include <QApplication>
#include <QStyle>
#include <QMessageBox>
#include <QMenuBar>
#include <QFileDialog>
#include <QInputDialog>
#include <QPainter>
#include <QKeyEvent>
#include <QMimeData>
#include <QVector>
#include <QCommandLineParser>
#include <QStandardPaths>
#ifndef _WIN32
#include <QGuiApplication>
#include <QSocketNotifier>
#include <unistd.h>
#include <sys/socket.h>
#include <signal.h>
#endif

#include <SDL2/SDL.h>

#include "OpenGLSupport.h"
#include "duckstation/gl/context.h"

#include "main.h"
#include "version.h"

#include "Config.h"

#include "EmuInstance.h"
#include "ArchiveUtil.h"
#include "CameraManager.h"
#include "MPInterface.h"
#include "Net.h"

#include "CLI.h"

#include "Net_PCap.h"
#include "Net_Slirp.h"

#include <sol/sol.hpp>
#include "NDS.h"
#include <InputConfig/InputConfigDialog.h>

using namespace melonDS;

QString* systemThemeName;


QString emuDirectory;

const int kMaxEmuInstances = 16;
EmuInstance* emuInstances[kMaxEmuInstances];

CameraManager* camManager[2];
bool camStarted[2];

std::optional<LibPCap> pcap;
Net net;

NDS* nds = nullptr; //! assigned in EmuInstance::loadRom() after updateConsole()
QElapsedTimer sysTimer;

void NetInit()
{
    Config::Table cfg = Config::GetGlobalTable();
    if (cfg.GetBool("LAN.DirectMode"))
    {
        if (!pcap)
            pcap = LibPCap::New();

        if (pcap)
        {
            std::string devicename = cfg.GetString("LAN.Device");
            std::unique_ptr<Net_PCap> netPcap = pcap->Open(devicename, [](const u8* data, int len) {
                net.RXEnqueue(data, len);
            });

            if (netPcap)
            {
                net.SetDriver(std::move(netPcap));
            }
        }
    }
    else
    {
        net.SetDriver(std::make_unique<Net_Slirp>([](const u8* data, int len) {
            net.RXEnqueue(data, len);
        }));
    }
}


bool createEmuInstance()
{
    int id = -1;
    for (int i = 0; i < kMaxEmuInstances; i++)
    {
        if (!emuInstances[i])
        {
            id = i;
            break;
        }
    }

    if (id == -1)
        return false;

    auto inst = new EmuInstance(id);

    emuInstances[id] = inst;

    return true;
}

void deleteEmuInstance(int id)
{
    auto inst = emuInstances[id];
    if (!inst) return;

    delete inst;
    emuInstances[id] = nullptr;
}

void deleteAllEmuInstances(int first)
{
    for (int i = first; i < kMaxEmuInstances; i++)
        deleteEmuInstance(i);
}

int numEmuInstances()
{
    int ret = 0;

    for (int i = 0; i < kMaxEmuInstances; i++)
    {
        if (emuInstances[i])
            ret++;
    }

    return ret;
}


void broadcastInstanceCommand(int cmd, QVariant& param, int sourceinst)
{
    for (int i = 0; i < kMaxEmuInstances; i++)
    {
        if (i == sourceinst) continue;
        if (!emuInstances[i]) continue;

        emuInstances[i]->handleCommand(cmd, param);
    }
}


void pathInit()
{
    // First, check for the portable directory next to the executable.
    QString appdirpath = QCoreApplication::applicationDirPath();
    QString portablepath = appdirpath + QDir::separator() + "portable";

#if defined(__APPLE__)
    // On Apple platforms we may need to navigate outside an app bundle.
    // The executable directory would be "melonDS.app/Contents/MacOS", so we need to go a total of three steps up.
    QDir bundledir(appdirpath);
    if (bundledir.cd("..") && bundledir.cd("..") && bundledir.dirName().endsWith(".app") && bundledir.cd(".."))
    {
        portablepath = bundledir.absolutePath() + QDir::separator() + "portable";
    }
#endif

    QDir portabledir(portablepath);
    if (portabledir.exists())
    {
        emuDirectory = portabledir.absolutePath();
    }
    else
    {
        // If no overrides are specified, use the default path.
#if defined(__WIN32__) && defined(WIN32_PORTABLE)
        emuDirectory = appdirpath;
#else
        QString confdir;
        QDir config(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation));
        config.mkdir("melonDS");
        confdir = config.absolutePath() + QDir::separator() + "melonDS";
        emuDirectory = confdir;
#endif
    }
}


void setMPInterface(MPInterfaceType type)
{
    // switch to the requested MP interface
    MPInterface::Set(type);

    // set receive timeout
    // TODO: different settings per interface?
    MPInterface::Get().SetRecvTimeout(Config::GetGlobalTable().GetInt("MP.RecvTimeout"));

    // update UI appropriately
    // TODO: decide how to deal with multi-window when it becomes a thing
    for (int i = 0; i < kMaxEmuInstances; i++)
    {
        EmuInstance* inst = emuInstances[i];
        if (!inst) continue;

        MainWindow* win = inst->getMainWindow();
        if (win) win->updateMPInterface(type);
    }
}



MelonApplication::MelonApplication(int& argc, char** argv)
    : QApplication(argc, argv)
{
#if !defined(Q_OS_APPLE)
    setWindowIcon(QIcon(":/melon-icon"));
    #if defined(Q_OS_UNIX)
        setDesktopFileName(QString("net.kuribo64.melonDS"));
    #endif
#endif
}

// TODO: ROM loading should be moved to EmuInstance
// especially so the preloading below and in main() can be done in a nicer fashion

bool MelonApplication::event(QEvent *event)
{
    if (event->type() == QEvent::FileOpen)
    {
        EmuInstance* inst = emuInstances[0];
        MainWindow* win = inst->getMainWindow();
        QFileOpenEvent *openEvent = static_cast<QFileOpenEvent*>(event);

        const QStringList file = win->splitArchivePath(openEvent->file(), true);
        win->preloadROMs(file, {}, true);
    }

    return QApplication::event(event);
}

sol::state LUA; // extern

sol::protected_function luaOnFrameCallback; // extern
sol::protected_function luaOnPauseCallback; // extern

// executes a lua protected function safely, manages exceptions: returns nil if an exception occurs, otherwise the function result
template<typename... Args>
sol::object safeExecuteCallback(sol::protected_function& callback, Args&&... args) {
    if (!callback.valid()) {
        return sol::nil;
    }
    
    try {
        // accept function args
        sol::protected_function_result result = callback(std::forward<Args>(args)...);
        
        if (result.valid()) {
            return result;
        } else {
            sol::error err = result;

            printf("[LUA] Callback error: %s\n", err.what());
            luaStopEverything();

            return sol::nil;
        }
        
    } catch (const std::exception& e) {
        printf("[LUA] Exception: %s\n", e.what());
        luaStopEverything();

        return sol::nil;
    } catch (...) {
        printf("[LUA] Unknown exception\n");
        luaStopEverything();

        return sol::nil;
    }
}

void luaOnFrameFunction() { // extern
    auto result = safeExecuteCallback(luaOnFrameCallback);

    // if is string and is "unregister"
    if (result.is<std::string>()) {
        auto str = result.as<std::string>();

        // TODO: add multi return support so that you can unregister anything
        // e.g: return "unregisterOnPause", "unregisterOnFrame";

        if (str == "unregister")
            luaOnFrameFunction_stop();

        else if (str == "unregisterAll")
            luaStopEverything();
    }
}

void luaOnPauseFunction(bool pausing) { // extern
    auto result = safeExecuteCallback(luaOnPauseCallback, pausing);

    if (result.is<std::string>()) {
        auto str = result.as<std::string>();

        if (str == "unregister")
            luaOnPauseFunction_stop();
            
        else if (str == "unregisterAll")
            luaStopEverything();
    }
}

void luaOnFrameFunction_stop() { // extern
    luaOnFrameCallback = sol::nil;
}

void luaOnPauseFunction_stop() { // extern
    luaOnPauseCallback = sol::nil;
}

void luaStopEverything() {
    luaOnFrameFunction_stop();
    luaOnPauseFunction_stop();

    auto instance = emuInstances[0];

    if (instance) {
        instance->luaInputMask = 0xFFF; // all bits to 1 = no button
        instance->luaInputActive = false;

        instance->luaPendingFrames = 0; // frame skip to run max speed
        instance->luaSyncMode = false;
    }
}

void setup_lua() { //!

    LUA.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table, sol::lib::package, sol::lib::io);

    // extern EmuThread* emuThread; // EmuInstance.cpp
    // LUA.set_function("pause", &emuThread->emuPause);
    
    sol::table native = LUA.create_table();
    // memory-related
    native.set_function("read_s8_le", &read_s8_le);
    native.set_function("read_s16_le", &read_s16_le);
    native.set_function("read_s32_le", &read_s32_le);
    native.set_function("write_s8_le", &write_s8_le);
    native.set_function("write_s16_le", &write_s16_le);
    native.set_function("write_s32_le", &write_s32_le);

    native.set_function("setInput", [](std::string input, bool downOrUp) {
        auto instance = emuInstances[0];
        if (!instance) return;

        QString lower = QString::fromStdString(input).toLower();

        // find input index in dskeylabels and corresponding bit in dskeyorder
        int bit = -1;
        for (int i = 0; i < 12; i++) {
            if (QString(dskeylabels[i]).toLower() == lower) {
                bit = dskeyorder[i];
                break;
            }
        }

        if (bit == -1) return; // NOTE: silent fail

        u32 mask = instance->luaInputMask;

        if (downOrUp)
            mask &= ~(1u << bit); // pressed
        else
            mask |= (1u << bit); // released

        instance->luaInputMask = mask;
        instance->luaInputActive = true; // enable if isn't

        nds->SetKeyMask(mask); // set input right away, before a possible frameSkip()
    });

    native.set_function("resetInput", []() {
        auto instance = emuInstances[0];
        if (!instance) return;

        instance->luaInputMask = 0xFFF; // disable al l
        instance->luaInputActive = true; // enable if isn't

        nds->SetKeyMask(0xFFF);
    });

    // emulator-related
    native.set_function("setPaused", [](bool pause) {
        auto instance = emuInstances[0];

        if (instance && instance->getEmuThread())
        {
            if (pause)
                instance->getEmuThread()->emuPause();
            else
                instance->getEmuThread()->emuUnpause();
        }
    });

    native.set_function("getPaused", []() -> bool {
        auto instance = emuInstances[0];

        if (instance && instance->getEmuThread())
            return !emuInstances[0]->getEmuThread()->emuIsRunning();
        
        return true;
    });

    native.set_function("onPause", [](sol::protected_function callback) {
        luaOnPauseCallback = callback;
    });


    native.set_function("onFrame", [](sol::protected_function callback) {
        luaOnFrameCallback = callback;
    });

    // for software renderer, NOT OPENGL
    native.set_function("get_screen", [](int screen_index) -> sol::object {
        auto& renderer = nds->GPU.GetRenderer();
        void* fb_top = nullptr;
        void* fb_bottom = nullptr;
        
        bool usesRamFramebuffers = renderer.GetFramebuffers(&fb_top, &fb_bottom);
        
        // if software renderer
        if (usesRamFramebuffers && fb_top && fb_bottom) {

            void* screen_buffer = (screen_index == 0) ? fb_top : fb_bottom;
            size_t buffer_size = 256 * 192 * sizeof(u32); // RGBA8, roughly 196,608 bytes (196kb)

            return sol::make_object(LUA, std::string(static_cast<const char*>(screen_buffer), buffer_size));
        }
        return sol::nil;
    });

    // sometimes causes visual glitches with OpenGL renderer
    // edit: perhaps fixed
    native.set_function("frame_skip", [](int frames, bool sync) {
        auto instance = emuInstances[0];

        if (instance) {

            // no sync = as fast as possible
            // sync = normal framerate (e.g: 60 fps with limited fps at default speed)

            instance->luaSyncMode = sync;
            instance->luaPendingFrames = frames;

            while (instance->luaPendingFrames > 0);
                SDL_Delay(1); // skip frames without pause
            
            instance->luaSyncMode = false;
        }
    });

    // TODO: implement blocking or non-blocking wait()

    native.set_function("reset", []() {
        if (emuInstances[0] && emuInstances[0]->getEmuThread())
        {
            auto thread = emuInstances[0]->getEmuThread();
            thread->emuReset();
        }
    });

    native.set_function("savestate_file", [](QString path) {
        if (emuInstances[0] && emuInstances[0]->getEmuThread())
        {
            auto thread = emuInstances[0]->getEmuThread();
            return thread->saveState(path);
        }
        return -1;
    });

    native.set_function("loadstate_file", [](QString path) {
        if (emuInstances[0] && emuInstances[0]->getEmuThread())
        {
            auto thread = emuInstances[0]->getEmuThread();
            return thread->loadState(path);
        }
        return -1;
    });

    // time in milliseconds
    native.set_function("time", []() -> int {
        return sysTimer.elapsed();
    });

    LUA["native"] = native;

    // so that files in 'core/' and nearby are found
    LUA.script(R"(
        package.path = package.path .. ";" .. "lua/?.lua"
    )");
}

// read memory
// Nelle tue funzioni di binding Lua
int read_s8_le(u32 address) {
    if (address == -1) return -1;
    return nds->ARM9Read8(address);
}

int read_s16_le(u32 address) {
    if (address == -1) return -1;
    return nds->ARM9Read16(address);
}

int read_s32_le(u32 address) {
    if (address == -1) return -1;
    return nds->ARM9Read32(address);
}

void write_s8_le(u32 address, u8 value) {
    if (address == -1) return;
    nds->ARM9Write8(address, value);
}

void write_s16_le(u32 address, u16 value) {
    if (address == -1) return;
    nds->ARM9Write16(address, value);
}

void write_s32_le(u32 address, u32 value) {
    if (address == -1) return;
    nds->ARM9Write32(address, value);
}

int main(int argc, char** argv)
{
    sysTimer.start();
    srand(time(nullptr));

    for (int i = 0; i < kMaxEmuInstances; i++)
        emuInstances[i] = nullptr;

    qputenv("QT_SCALE_FACTOR", "1");

#if QT_VERSION_MAJOR == 6 && defined(__WIN32__)
    // Allow using the system dark theme palette on Windows
    qputenv("QT_QPA_PLATFORM", "windows:darkmode=2");
#endif

    printf("melonDS " MELONDS_VERSION " - With LUA Scripting\n");
    printf(MELONDS_URL "\n");

    //! lua init
    setup_lua();

    // easter egg - not worth checking other cases for something so dumb
    if (argc != 0 && (!strcasecmp(argv[0], "derpDS") || !strcasecmp(argv[0], "./derpDS")))
        printf("did you just call me a derp???\n");

#ifdef _WIN32
    // argc and argv are passed as UTF8 by SDL's WinMain function
    // QT checks for the original value in local encoding though
    // to see whether it is unmodified to activate its hack that
    // retrieves the unicode value via CommandLineToArgvW.
    argc = __argc;
    argv = __argv;
#endif
    MelonApplication melon(argc, argv);
    pathInit();

    CLI::CommandLineOptions* options = CLI::ManageArgs(melon);

    // http://stackoverflow.com/questions/14543333/joystick-wont-work-using-sdl
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

    SDL_SetHint(SDL_HINT_APP_NAME, "melonDS");

    if (SDL_Init(SDL_INIT_HAPTIC) < 0)
    {
        printf("SDL couldn't init rumble\n");
    }
    if (SDL_Init(SDL_INIT_JOYSTICK) < 0)
    {
        printf("SDL couldn't init joystick\n");
    }
    if (SDL_Init(SDL_INIT_SENSOR) < 0)
    {
        printf("SDL couldn't init motion sensors\n");
    }
    if (SDL_Init(SDL_INIT_AUDIO) < 0)
    {
        const char* err = SDL_GetError();
        QString errorStr = "Failed to initialize SDL. This could indicate an issue with your audio driver.\n\nThe error was: ";
        errorStr += err;

        QMessageBox::critical(nullptr, "melonDS", errorStr);
        return 1;
    }

    SDL_JoystickEventState(SDL_ENABLE);

    SDL_InitSubSystem(SDL_INIT_VIDEO);
    SDL_EnableScreenSaver(); SDL_DisableScreenSaver();

    if (!Config::Load())
        QMessageBox::critical(nullptr,
                              "melonDS",
                              "Unable to write to config.\nPlease check the write permissions of the folder you placed melonDS in.");

    camStarted[0] = false;
    camStarted[1] = false;
    camManager[0] = new CameraManager(0, 640, 480, true);
    camManager[1] = new CameraManager(1, 640, 480, true);

    systemThemeName = new QString(QApplication::style()->objectName());

    {
        Config::Table cfg = Config::GetGlobalTable();
        QString uitheme = cfg.GetQString("UITheme");
        if (!uitheme.isEmpty())
        {
            QApplication::setStyle(uitheme);
        }
    }

    // fix for Wayland OpenGL glitches
    QGuiApplication::setAttribute(Qt::AA_NativeWindows, false);
    QGuiApplication::setAttribute(Qt::AA_DontCreateNativeWidgetSiblings, true);

    // default MP interface type is local MP
    // this will be changed if a LAN or netplay session is initiated
    setMPInterface(MPInterface_Local);

    NetInit();

    createEmuInstance();

    {
        MainWindow* win = emuInstances[0]->getMainWindow();
        bool memberSyntaxUsed = false;
        const auto prepareRomPath = [&](const std::optional<QString> &romPath,
                                        const std::optional<QString> &romArchivePath) -> QStringList
        {
            if (!romPath.has_value())
                return {};

            if (romArchivePath.has_value())
                return {*romPath, *romArchivePath};

            const QStringList path = win->splitArchivePath(*romPath, true);
            if (path.size() > 1) memberSyntaxUsed = true;
            return path;
        };

        const QStringList dsfile = prepareRomPath(options->dsRomPath, options->dsRomArchivePath);
        const QStringList gbafile = prepareRomPath(options->gbaRomPath, options->gbaRomArchivePath);

        if (memberSyntaxUsed) printf("Warning: use the a.zip|b.nds format at your own risk!\n");

        win->preloadROMs(dsfile, gbafile, options->boot);

        if (options->fullscreen)
            win->toggleFullscreen();
    }

    int ret = melon.exec();

    delete options;

    // if we get here, all the existing emu instances should have been deleted already
    // but with this we make extra sure they are all deleted
    deleteAllEmuInstances();

    delete camManager[0];
    delete camManager[1];

    Config::Save();

    SDL_Quit();
    return ret;
}

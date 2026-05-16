#include <InputConfig/InputConfigDialog.h>

#include "ScriptManager.h"

sol::state LUA;

ScriptManager::ScriptManager() : running(false) {}

#pragma region load, run, stop scripts

void ScriptManager::loadScript(const QString& path) {
    currentPath = path;
    printf("[LUA}] Script loaded: %s\n", currentPath.toUtf8().constData());
}

void ScriptManager::runScript() {

    if (currentPath.isEmpty()) {
        printf("[LUA] No script loaded!\n");
        return;
    }
    
    auto* instance = emuInstances[0];
    if (!instance) return;
    
    if (!instance->getEmuThread()->emuIsActive()) {
        printf("[LUA] No game is running!\n");
        return;
    }

    if (running) {
        // (no print)
        stopScript();
    }

    // printf("[LUA] Running script: %s\n", currentPath.toUtf8().constData());
    printf("[LUA] Running loaded script...\n");
    
    running = true;

    // run actual script with stacktrace if exception (no try-catch needed):

    // add a function to call when an error occurs (uses debug lib)
    LUA.script(R"(
    function __error_handler(err)

        local levels = {}
        local level = 1

        while true do
            local info = debug.getinfo(level, "Sl")
            if not info then break end

            if info.what ~= "C" and info.short_src ~= '[string "..."]' then

                local src = info.source
                if src:sub(1,1) == "@" then
                    src = src:sub(2)
                    
                    local i, start = nil, 1
                    while true do
                        local ni = src:find("lua/", start)
                        if not ni then break end
                        i = ni
                        start = ni + 1
                    end

                    if i then src = src:sub(i) end
                end
                table.insert(levels, {src = src, line = info.currentline})
            end
            level = level + 1
        end
        return {msg = tostring(err), levels = levels}
    end
    )");

    LUA.globals()["__script_path"] = currentPath.toUtf8().constData();

    // run script safely via xpcall, and run __error_handler if an exception occurs
    LUA.safe_script(R"(
        local ok, data = xpcall(function()
            dofile(__script_path)
        end, __error_handler)
        __lua_error = ok and nil or data
    )", sol::script_pass_on_error); // SYNC!

    sol::optional<sol::table> errData = LUA.globals()["__lua_error"];

    // if exception
    if (errData && errData.value().valid()) {
        sol::table data = errData.value();
        std::string msg = data["msg"].get_or<std::string>("<unknown error>");

        auto normalizePath = [](const std::string& s) -> std::string {
            size_t i = std::string::npos;
            size_t ni, start = 0;

            // make path more compact (till lua/) TODO: revise
            while ((ni = s.find("lua/", start)) != std::string::npos)
            {
                i = ni;
                start = ni + 1;
            }

            return i != std::string::npos ? s.substr(i) : s;
        };

        // clean up error message
        std::string cleanMsg = msg;
        size_t c1 = msg.find(':');

        if (c1 != std::string::npos) {
            size_t c2 = msg.find(':', c1 + 1);

            if (c2 != std::string::npos)
                cleanMsg = msg.substr(c2 + 2);
        }

        sol::table levels = data["levels"];
        int n = (int)levels.size();

        std::string errorLocation = "?";

        if (n > 0) {
            sol::table last = levels[n];
            errorLocation = last["src"].get_or<std::string>("?") + ":" + std::to_string(last["line"].get_or(-1));
        }

        printf("[LUA] Error: %s %s\n", errorLocation.c_str(), cleanMsg.c_str());
        printf("Stacktrace:\n");

        for (int i = 1; i <= n; i++) {
            sol::table entry = levels[i];
            std::string src = entry["src"].get_or<std::string>("?");

            int line = entry["line"].get_or(-1);

            // add "(main)" to most recent entry
            printf(i == n ? "- %s:%d (main)\n" : "- %s:%d\n", src.c_str(), line);
        }

        resetAll(); // running = false
    }
}

void ScriptManager::stopScript() {

    if (!isScriptLoaded()) {
        printf("[LUA] No script loaded!\n");
        return;
    }
    
    auto* instance = emuInstances[0];
    if (!instance) return;
    
    if (!instance->getEmuThread()->emuIsActive()) {
        printf("[LUA] No game is running!\n");
        return;
    }

    if (!running) {
        printf("[LUA] No script is running!\n");
        return;
    }

    printf("[LUA] Stopped loaded script.\n");
    resetAll(); // running = false
}

#pragma endregion

#pragma region memory-related functions (memory.lua)

// read memory
static int read_s8_le(u32 address) {
    if (address == -1) return -1;
    return nds->ARM9Read8(address);
}

static int read_s16_le(u32 address) {
    if (address == -1) return -1;
    return nds->ARM9Read16(address);
}

static int read_s32_le(u32 address) {
    if (address == -1) return -1;
    return nds->ARM9Read32(address);
}

// write memory
static void write_s8_le(u32 address, u8 value) {
    if (address == -1) return;
    nds->ARM9Write8(address, value);
}

static void write_s16_le(u32 address, u16 value) {
    if (address == -1) return;
    nds->ARM9Write16(address, value);
}

static void write_s32_le(u32 address, u32 value) {
    if (address == -1) return;
    nds->ARM9Write32(address, value);
}

#pragma endregion

#pragma region private utils

static EmuThread* getEmuThread() {
    auto* inst = emuInstances[0];
    if (!inst) return nullptr;

    auto* thread = inst->getEmuThread();
    if (!thread) return nullptr;

    return thread;
}

#pragma endregion

void ScriptManager::setupLua() {
    // set libraries (avoid os for security)
    LUA.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table, sol::lib::package, sol::lib::io, sol::lib::debug);

    // create a global table "native" that contains everything
    sol::table native = LUA.create_table();

    // memory:
    native.set_function("read_s8", &read_s8_le);
    native.set_function("read_s16", &read_s16_le);
    native.set_function("read_s32", &read_s32_le);
    native.set_function("write_s8", &write_s8_le);
    native.set_function("write_s16", &write_s16_le);
    native.set_function("write_s32", &write_s32_le);

    // emu:
    // time in milliseconds
    native.set_function("time", []() -> int
    {
        return sysTimer.elapsed();
    });

    // stop this script
    native.set_function("stop", [this]()
    {
        this->resetAll(); // running = false
    });

    // untested, to reset current game
    native.set_function("reset", []()
    {
        if (auto* thread = getEmuThread())
            thread->emuReset();
    });

    // emulate a number of frames in the game
    native.set_function("frame_skip", [this](int frames, bool sync)
    {
        // no sync = as fast as possible
        // sync = normal framerate (e.g: 60 fps with limited fps at default speed)

        this->luaSyncMode = sync;
        this->luaPendingFrames = frames;

        while (this->luaPendingFrames > 0);
            SDL_Delay(1); // skip frames without pausing unlike frame advance/step
        
        this->luaSyncMode = false;
    });

    native.set_function("set_input", [this](std::string input, bool downOrUp)
    {
        // case insensitive
        QString lower = QString::fromStdString(input).toLower();

        // find input index in dskeylabels and corresponding bit in dskeyorder
        int bit = -1;
        for (int i = 0; i < 12; i++) {
            if (QString(dskeylabels[i]).toLower() == lower) {
                bit = dskeyorder[i];
                break;
            }
        }

        if (bit == -1) return; // silent fail

        u32 mask = this->luaInputMask;

        if (downOrUp)
            mask &= ~(1u << bit); // pressed
        else
            mask |= (1u << bit); // released

        this->luaInputMask = mask;
        this->luaInputActive = true; // block external input (unblocked when stop)

        nds->SetKeyMask(mask); // set input right away, before a possible frameSkip()
    });

    native.set_function("reset_input", [this]()
    {
        this->luaInputMask = 0xFFF;
        this->luaInputActive = true;

        nds->SetKeyMask(0xFFF);
    });

    // emu.pause() -- returns whether emu is paused
    // emu.pause(true) -- pauses emu, returns true
    // emu.pause(false) -- unpauses emu, returns false
    native.set_function("pause", [](sol::optional<bool> state) -> bool {
        if (auto* thread = getEmuThread())
        {
            // setter
            if (state.has_value())
            {
                if (pause) {
                    thread->emuPause();
                    return true;
                }
                else {
                    thread->emuUnpause();
                    return false;
                }
            }

            // getter
            return !thread->emuIsRunning();
        }

        return false;
    });

    native.set_function("on_pause", [this](sol::protected_function callback) {
        this->onPauseCallback = callback;
    });


    native.set_function("on_frame", [this](sol::protected_function callback) {
        this->onFrameCallback = callback;
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

    // save to .sav, return success
    native.set_function("savestate_file", [](std::string path) -> bool
    {
        if (auto* thread = getEmuThread())
            return thread->saveState(QString::fromStdString(path));

        return false;
    });

    // load a .sav, return success
    native.set_function("loadstate_file", [](std::string path) -> bool
    {
        if (auto* thread = getEmuThread())
            return thread->loadState(QString::fromStdString(path));

        return false;
    });
    
    // set global
    LUA.set("native", native);

    // so that files in 'core/' and nearby are found
    LUA.script(R"(
        package.path = package.path .. ";" .. "lua/?.lua"
    )");
}

// TODO revise: callbacks:

#pragma region callbacks cpp to lua

void ScriptManager::onFrame() {
    if (!running || !onFrameCallback.valid()) return;
    
    try {
        auto result = onFrameCallback();
        if (result.valid() && result.get_type() == sol::type::string) {
            std::string str = result;
            if (str == "unregister")
                onFrameCallback = sol::nil;
            else if (str == "unregisterAll")
                resetAll();
        }
    }
    catch (const sol::error& e) {
        printf("[LUA] onFrame error: %s\n", e.what());
        resetAll();
    }
}

void ScriptManager::onPause(bool pausing) {
    if (!running || !onPauseCallback.valid()) return;
    
    try {
        auto result = onPauseCallback(pausing);
        if (result.valid() && result.get_type() == sol::type::string) {
            std::string str = result;
            if (str == "unregister")
                onPauseCallback = sol::nil;
            else if (str == "unregisterAll")
                resetAll();
        }
    }
    catch (const sol::error& e) {
        printf("[LUA] onPause error: %s\n", e.what());
        resetAll();
    }
}

#pragma endregion
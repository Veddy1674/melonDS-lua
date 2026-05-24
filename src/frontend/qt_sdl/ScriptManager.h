#ifndef SCRIPTMANAGER_H
#define SCRIPTMANAGER_H

#include <QApplication>
#include "EmuInstance.h"
#include "EmuThread.h"
#include "NDS.h"

#include <sol/sol.hpp>

using namespace melonDS;

extern sol::state LUA; // multiple LUA for each instance?
extern EmuInstance* emuInstances[]; // eventually one ScriptManager could handle multiple instances
extern NDS* nds; // ?

class ScriptManager {
public:
    ScriptManager();
    ~ScriptManager() { resetAll(); } // ?
    
    void setupLua();
    void loadScript(const QString& path);
    void setLuaArgs(const std::vector<std::string>& args);
    void runScript();
    void stopScript();

    bool isScriptRunning() const { return running; }
    bool isScriptLoaded() const { return !currentPath.isEmpty(); }

    // callbacks
    void onFrame();
    void onPause(bool pausing);

    std::atomic<int> luaPendingFrames{0}; // frames to emulate (1 = 0)
    bool externalInputsBlocked() const { return luaInputActive; }

    // block lua when pending frames
    std::mutex frameMutex;
    std::condition_variable frameCond;

    bool savePreferences; // whether to save preferences on emu.terminate(), like a manual window closure or forced closure
    
private:
    // reset variables, callbacks
    void resetAll() {
        onFrameCallback = sol::nil;
        onPauseCallback = sol::nil;

        luaInputMask = 0xFFF;
        luaInputActive = false;
        luaPendingFrames = 0;

        // reset modules cache (e.g: if emu.lua is changed it will be reloaded without having to restart emulator)
        LUA.script(R"(
            package.loaded = {}
        )");

        running = false; //!
    }

    QString currentPath; // of lua script
    bool running = false;

    // callbacks
    sol::protected_function onFrameCallback;
    sol::protected_function onPauseCallback;

    // accessed from main thread (lua) and emu thread
    std::atomic<melonDS::u32> luaInputMask{0xFFF}; // all bits to 1 = no button
    std::atomic<bool> luaInputActive{false}; // whether to apply input mask
};

#endif // SCRIPTMANAGER_H

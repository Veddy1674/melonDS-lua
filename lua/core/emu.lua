-- emu.lua - a wrapper of the "native" table for emulator-related functions and constants

local emu = {}

---@diagnostic disable-next-line: undefined-global
local native = assert(native, "'native' module not found")

-- Returns time in milliseconds since the emulator was started (never pauses)
---@return integer
function emu.time()
    return native.time()
end

-- Stops this script<br>
-- DO NOT USE INSIDE CALLBACKS! (e.g: onPause, onFrame)
function emu.stop()
    native.stop()
end

-- Stops the whole emulator process
function emu.terminate()
    native.terminate()
end

-- Resets current running game
function emu.reset()
    native.reset()
end

-- Advances the game by 'frames' amount of frames, as fast as possible (sync)<br>
-- If 'frames' is 1, nothing special will happen, but lua will pause for a frame<br>
-- NOTE: This is to synchronize LUA to EMULATOR, use <b>emu.frameStep()</b> for the other way around<br>
-- NOTE: onFrame callback will be called once per frame regardless of 'frames'
---@param frames number|nil
function emu.frameSkip(frames)
    return native.frame_skip(frames or 1)
end

-- Advances the game by 'frames' amount of frames, as fast as possible (sync)<br>
-- The emulator should be paused before this function is called<br>
-- NOTE: This is to synchronize EMULATOR to LUA, use <b>emu.frameSkip()</b> for the other way around<br>
-- NOTE: onFrame callback won't be called at all
---@param frames number|nil
function emu.frameStep(frames)
    return native.frame_step(frames or 1)
end

-- TODO add a method to enable/disable external inputs

-- Forces an input, ignoring external inputs until script is stopped
---@param input "A"|"B"|"X"|"Y"|"Left"|"Right"|"Up"|"Down"|"L"|"R"|"Select"|"Start"
---@param downOrUp boolean
function emu.setInput(input, downOrUp)
    return native.set_input(input, downOrUp)
end

-- Forces all inputs to false (up)
function emu.resetInput()
    return native.reset_input()
end

-- Sets emulator state to paused/unpaused if 'pause' is a boolean<br>
-- Returns current emulator pause state
---@param pause boolean|nil
---@return boolean
function emu.pause(pause)
    -- already paused/unpaused
    if pause ~= nil and native.pause() == pause then
        return pause
    end

    return native.pause(pause)
end

-- setting pause to true and then false causes issues if the game was paused externally at least once
-- kind of difficult to explain and i have no absolute clue why, just stating it here

-- Sets a function called every pause event of any kind (from emu.pause() as well)<br>
-- If 'func' returns "unregister", the function will be unregistered right away<br>
-- NOTE: DO NOT call emu.stop() in the callback, as the emulator will crash, use "unregisterAll" instead
---@param func fun(pausing: boolean): "unregister"|"unregisterAll"|nil
function emu.onPause(func)
    native.on_pause(func)
end

-- Sets a function called every game frame<br>
-- If 'func' returns "unregister", the function will be unregistered right away<br>
-- NOTE: DO NOT call emu.stop() in the callback, as the emulator will crash, use "unregisterAll" instead
---@param func fun(): "unregister"|"unregisterAll"|nil
function emu.onFrame(func)
    native.on_frame(func)
end

-- Returns current game screen pixel data as a raw string
---@param topOrBottom "top"|"bottom"
function emu.getScreen(topOrBottom)
    return native.get_screen(topOrBottom == "bottom" and 1 or 0)
end

-- Save current game state as a .sav file
---@param path string
function emu.savestate(path)
    return native.savestate_file(path)
end

-- Load game state from a .sav file
---@param path string
function emu.loadstate(path)
    return native.loadstate_file(path)
end

-- Other functions not from native lib:

-- Saves a screenshot .raw file of 196.608 bytes, RGB
---@param path string
---@param screen "top"|"bottom"
function emu.screenshot(path, screen)
    local data = emu.getScreen(screen) -- default top

    local file = assert(io.open(path, "wb"), "Unable to open file " .. path)
    file:write(data)
    file:close()
end

-- Utils not related to emulator:

-- print(string.format(<b>...</b>))
---@diagnostic disable-next-line: lowercase-global
function printf(...)
    print(string.format(...))
end

return emu
-- emu.lua - a wrapper of the "native" table for emulator-related functions and constants

local emu = {}

---@diagnostic disable-next-line: undefined-global
local native = assert(native, "'native' module not found")

-- Returns time in milliseconds
---@return integer
function emu.time()
    return native.time()
end

-- Stops this script
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

-- Advances the game by 'frames' number of frames<br>
-- If 'sync' is true, the frame advancing will synchronize with the emulator (slower)
---@param frames number|nil
---@param sync boolean|nil
function emu.frameSkip(frames, sync)
    return native.frame_skip(frames or 1, sync or false)
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

-- Sets emulator state to paused/unpaused if 'pause' is a boolean
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

-- Sets a function called every pause event of any kind (from emu.pause() too)<br>
-- If 'func' returns "unregister", the function will be unregistered right away
---@param func fun(pausing: boolean): "unregister"|"unregisterAll"|nil
function emu.onPause(func)
    native.on_pause(func)
end

-- Sets a function called every game frame<br>
-- If 'func' returns "unregister", the function will be unregistered right away
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
-- emu.lua - a wrapper of the "native" table for emulator-related functions and constants

local emu = {}

---@diagnostic disable-next-line: undefined-global
local native = assert(native, "native module not found")

---@param pause boolean
function emu.setPaused(pause)
    if emu.getPaused() == pause then return end -- avoid unnecessary calls, which often cause crashes
    return native.setPaused(pause)
end

function emu.getPaused()
    return native.getPaused()
end

-- Sets a function that will be called every pause of any kind (from lua included)<br>
-- If 'func' returns "unregister", the function will be unregistered right away
---@param func fun(pausing: boolean): "unregister"|"unregisterAll"|nil
function emu.onPause(func)
    native.onPause(func)
end

-- Resets current game
function emu.reset()
    return native.reset()
end

-- Advances the game by 'frames' number of frames<br>
-- If 'sync' is true, the method will synchronize with the emulator (e.g: wait for the frame limitator), otherwise will run as fast as possible (1ms delay between frame advance requests)
---@param frames number|nil
---@param sync boolean|nil
function emu.frameSkip(frames, sync)
    return native.frame_skip(frames or 1, sync or false)
end

---@param path string
function emu.savestate(path)
    return native.saveState_file(path)
end

---@param path string
function emu.loadstate(path)
    return native.loadState_file(path)
end

---@param input "A"|"B"|"X"|"Y"|"Left"|"Right"|"Up"|"Down"|"L"|"R"|"Select"|"Start"
---@param downOrUp boolean
function emu.setInput(input, downOrUp)
    return native.setInput(input, downOrUp)
end

-- Returns time in milliseconds
function emu.time()
    return native.time()
end

---@param topOrBottom "top"|"bottom"
function emu.getScreen(topOrBottom)
    return native.get_screen(topOrBottom == "bottom" and 1 or 0)
end

-- Sets a function that will be called every game frame<br>
-- If 'func' returns "unregister", the function will be unregistered right away
---@param func fun(): "unregister"|"unregisterAll"|nil
function emu.onFrame(func)
    native.onFrame(func)
end

-- Saves a screenshot as a .raw of 196.608 bytes, RGB
---@param path string
---@param screen "top"|"bottom"
function emu.screenshot(path, screen)
    local data = emu.getScreen(screen)
    local file = assert(io.open(path, "wb"), "Unable to open file " .. path)
    file:write(data)
    file:close()
end

return emu
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

return emu
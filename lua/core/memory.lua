-- memory.lua - a wrapper of the "native" table for memory-related functions and constants

local memory = {}

---@diagnostic disable-next-line: undefined-global
local native = assert(native, "native module not found")

---@param addr number Address
function memory.read_s8_le(addr)
    return native.read_s8_le(addr)
end

---@param addr number Address
function memory.read_s16_le(addr)
    return native.read_s16_le(addr)
end

---@param addr number Address
function memory.read_s32_le(addr)
    return native.read_s32_le(addr)
end

---@param addr number Address
---@param value number Value to write
function memory.write_s8_le(addr, value)
    native.write_s8_le(addr, value)
end

---@param addr number Address
---@param value number Value to write
function memory.write_s16_le(addr, value)
    native.write_s16_le(addr, value)
end

---@param addr number Address
---@param value number Value to write
function memory.write_s32_le(addr, value)
    native.write_s32_le(addr, value)
end

return memory
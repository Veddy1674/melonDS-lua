-- memory.lua - a wrapper of the "native" table for memory-related functions and constants

local memory = {}

---@diagnostic disable-next-line: undefined-global
local native = assert(native, "native module not found")

-- NOTE: all functions are little-endian based

---@param addr number Address
function memory.read_s8(addr)
    return native.read_s8(addr)
end

---@param addr number Address
function memory.read_s16(addr)
    return native.read_s16(addr)
end

---@param addr number Address
function memory.read_s32(addr)
    return native.read_s32(addr)
end

---@param addr number Address
---@param value number Value to write
function memory.write_s8(addr, value)
    native.write_s8(addr, value)
end

---@param addr number Address
---@param value number Value to write
function memory.write_s16(addr, value)
    native.write_s16(addr, value)
end

---@param addr number Address
---@param value number Value to write
function memory.write_s32(addr, value)
    native.write_s32(addr, value)
end

return memory
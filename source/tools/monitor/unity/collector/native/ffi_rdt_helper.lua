local rawffi = require "ffi"
local rdtffi = rawffi.load('rdt_helper')

rawffi.cdef [[
    int calculate(const char* now,const char* prev);
]]

return { rawffi = rawffi, rdtffi = rdtffi }

local ffi = require("ffi")
local awesome = ffi.load('metricSnappy')

ffi.cdef[[
typedef signed char GoInt8;
typedef unsigned char GoUint8;
typedef long long GoInt64;
typedef GoInt64 GoInt;
typedef double GoFloat64;
typedef struct { const char *p; GoInt n; } GoString;
typedef struct { void *data; GoInt len; GoInt cap; } GoSlice;
extern GoInt metricSnappy(GoString* prome_ptr, GoSlice* ret);
]]

return {ffi = ffi, awesome = awesome}
--
---- input prometheus string
--local prome = ffi.new("GoString")
---- an example of prometheus string
--local s = 'sysak_proc_cpu_total{mode="user",instance="i-wz9d3tqjhpb8esj8ps4z"} 0.8\nsysak_proc_cpu_total{mode="total",instance="i-wz9d3tqjhpb8esj8ps4z"} 3960.0'
--prome.p = s;
--prome.n = #s;
--local prome_ptr = ffi.cast("GoString*", prome)
--local byte = ffi.new("GoSlice")
--local byte_ptr = ffi.cast("GoSlice*", byte)
--
---- execute parse and snappy
--local data_len = awesome.metricSnappy(prome,byte_ptr)
--
---- read data in lua
--data_len = tonumber(data_len)
--local data = ffi.cast("GoUint8*", byte_ptr.data)
--
---- show data
--for i = 0, data_len do
--    print(data[i])
--end

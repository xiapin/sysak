local ffi = require("ffi")
local awesome = ffi.load('Cri')

ffi.cdef[[
    typedef signed char GoInt8;
    typedef unsigned char GoUint8;
    typedef long long GoInt64;
    typedef GoInt64 GoInt;
    typedef double GoFloat64;
    typedef struct { const char *p; GoInt n; } GoString;
    typedef struct { void *data; GoInt len; GoInt cap; } GoSlice;
    extern GoInt CheckRuntime(GoString* endpoint_ptr);
    extern char *CGetContainerInfosfunc(const char* endpoint);
    void free(void *p);
]]

return {ffi = ffi, awesome = awesome}
---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liaozhaoyan.
--- DateTime: 2022/12/16 10:12 PM
---

require("common.class")
local CvProto = require("collector.vproto")

local CvProc = class("collector.vproc", CvProto)

function CvProc:_init_(proto, pffi, mnt, pFile)
    CvProto._init_(self, proto)
    self._cffi = pffi["cffi"]
    self._ffi = pffi["ffi"]
    mnt = mnt or "/"
    pFile = pFile or ""
    self.pFile = mnt .. pFile
end

return CvProc
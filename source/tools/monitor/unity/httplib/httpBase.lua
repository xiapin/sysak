---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liaozhaoyan.
--- DateTime: 2022/12/24 9:52 AM
---

require("class")
local ChttpComm = require("httpComm")

local ChttpBase = class("ChttpBase", ChttpComm)

function ChttpBase:_init_(frame)
    ChttpComm._init_(self)
    self._urlCb = {}
end

function ChttpBase:_install(frame)
    for k, _ in pairs(self._urlCb) do
        frame:register(k, self)
    end
end

function ChttpBase:echo(tRet)
    error("ChttpBase:echo is a virtual function.")
end

function ChttpBase:call(tReq)
    local tRet = self._urlCb[tReq.path](tReq)
    return self:echo(tRet)
end

return ChttpBase

---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liaozhaoyan.
--- DateTime: 2022/12/22 12:14 PM
---

require("class")
local ChttpBase = require("httpBase")
local pystring = require("pystring")

local ChttpApp = class("ChttpApp", ChttpBase)

function ChttpApp:_init_(frame)
    ChttpBase._init_(self)
end

function ChttpApp:echo(tRet)
    local stat = self:packStat(200)
    local tHead = {
        ["Content-Type"] = "application/json",
    }
    local body = self:jencode(tRet)
    local headers = self:packHeaders(tHead, #body)
    local tHttp = {stat, headers, body}
    return pystring:join("\r\n", tHttp)
end

function ChttpApp:getJson(tReq)
    return self:jdecode(tReq.data)
end

return ChttpApp

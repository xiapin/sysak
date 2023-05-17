---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liaozhaoyan.
--- DateTime: 2022/12/22 12:14 PM
---

require("common.class")
local ChttpBase = require("httplib.httpBase")
local pystring = require("common.pystring")

local ChttpApp = class("ChttpApp", ChttpBase)

function ChttpApp:_init_(frame)
    ChttpBase._init_(self)
end

function ChttpApp:echo(tRet, keep, code)
    code = code or 200
    local stat = self:packStat(code)
    local tHead = {
        ["Content-Type"] = "application/json",
        ["Connection"] = (keep and "keep-alive") or "close"
    }
    local body = self:jencode(tRet)
    local headers = self:packServerHeaders(tHead, #body)
    local tHttp = {stat, headers, body}
    return pystring:join("\r\n", tHttp)
end

function ChttpApp:getJson(tReq)
    return self:jdecode(tReq.data)
end

return ChttpApp
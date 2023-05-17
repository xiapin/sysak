---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liaozhaoyan.
--- DateTime: 2022/12/24 12:32 AM
---

require("common.class")
local pystring = require("common.pystring")
local ChttpBase = require("httplib.httpBase")

local ChttpPlain = class("ChttpPlain", ChttpBase)

function ChttpPlain:_init_(frame)
    ChttpBase._init_(self)
end

function ChttpPlain:echo(tRet, keep, code)
    code = code or 200
    local stat = self:packStat(code)
    local tHead = {
        ["Content-Type"] = "text/plain",
        ["Connection"] = (keep and "keep-alive") or "close"
    }
    local body = tRet.text
    local headers = self:packServerHeaders(tHead, #body)
    local tHttp = {stat, headers, body}
    return pystring:join("\r\n", tHttp)
end

return ChttpPlain
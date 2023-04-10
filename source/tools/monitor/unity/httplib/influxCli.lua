---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liaozhaoyan.
--- DateTime: 2023/3/30 1:28 AM
---

require("common.class")
local ChttpCli = require("httplib.httpCli")

local CinfluxCli = class("influxCli", ChttpCli)

function CinfluxCli:_init_(url, db, user, pass, proxy)
    self._url = string.format("%swrite?u=%s&p=%s&db=%s", url, user, pass, db)
    ChttpCli._init_(self, proxy)
end

function CinfluxCli:puts(s)
    local headers = {
        ["Content-Type"] = "text/plain",
        ["Content-Length"] = #s,
    }
    return self:post(self._url, s, headers)
end

return CinfluxCli

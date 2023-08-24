---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liaozhaoyan.
--- DateTime: 2023/8/22 10:24 AM
---

require("common.class")
local CasyncHttp = require("httplib.asyncHttp")

local CasyncHttps = class("CasyncHttps", CasyncHttp)

function CasyncHttps:_init_()
    CasyncHttp._init_(self)
end

function CasyncHttp:procSSLStream(fd, handle, stream, toWake)
    local res, msg
    res = g_lb:ssl_write(fd, handle, stream)
    if res then
        local fread = g_lb:ssl_read(handle)
        local tReq = self:result(fread)
        res, msg = coroutine.resume(toWake, tReq.data)
        assert(res, msg)
    else
        res, msg = coroutine.resume(toWake, "write failed.")
        assert(res, msg)
    end

    g_lb:ssl_del(handle)
    g_lb:co_exit(fd)
end

function CasyncHttp:_get(fd)
    local res, msg
    local toWake, domain, uri, headers, body, connecting = coroutine.yield()

    if self:assertConnect(fd, connecting, toWake) < 0 then
        return
    end

    local handle = g_lb:ssl_handshake(fd)
    if not handle then
        res, msg = coroutine.resume(toWake, "tls handshake failed.")
        g_lb:co_exit(fd)
        return
    end

    local stream = self:pack('GET', domain, uri, headers, body)
    self:procSSLStream(fd, handle, stream, toWake)
end

return CasyncHttp
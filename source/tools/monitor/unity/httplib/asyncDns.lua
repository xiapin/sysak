---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liaozhaoyan.
--- DateTime: 2023/5/24 11:41
---

require("common.class")
local psocket = require("posix.sys.socket")
local unistd = require("posix.unistd")
local pystring = require("common.pystring")
local system = require("common.system")

local CasyncDns = class("coDns")

local function lookup_server()
    local f = io.open("/etc/resolv.conf")
    local server = ""
    for line in f:lines() do
        if pystring:startswith(line, "nameserver") then
            local res = pystring:split(line)
            server = res[2]
            break
        end
    end

    f:close()
    return server
end

function CasyncDns:_init_()
    self._server = lookup_server()
    assert(#self._server > 0)
end

local function packQuery(domain)
    local cnt = 0
    local queries = {}
    local head = string.char(
            0x12, 0x34, -- Query ID
            0x01, 0x00, -- Standard query
            0x00, 0x01, -- Number of questions
            0x00, 0x00, -- Number of answers
            0x00, 0x00, -- Number of authority records
            0x00, 0x00  -- Number of additional records
    )
    cnt = cnt + 1
    queries[cnt] = head

    local names = pystring:split(domain, ".")
    for _, name in ipairs(names) do
        cnt = cnt + 1
        queries[cnt] = string.char(string.len(name))
        cnt = cnt + 1
        queries[cnt] = name
    end
    cnt = cnt + 1
    local tail = string.char(
            0x00, -- End of domain name
            0x00, 0x01, -- Type A record
            0x00, 0x01 -- Class IN
    )
    queries[cnt] = tail

    local query = table.concat(queries)
    return query
end

function CasyncDns:waitReq(fd)
    local res, msg
    local toWake = coroutine.yield()
    local e = coroutine.yield()
    local ip
    if e.ev_close > 0 then
        print("force to close socket.")
    elseif e.ev_in > 0 then
        res = psocket.recvfrom(fd, 512)
        if res then
            ip = string.format("%d.%d.%d.%d", string.byte(res, -4, -1))
        end
    else
        print("bad socket.")
    end
    res, msg = coroutine.resume(toWake, ip)
    assert(res, msg)
    g_lb:co_exit(fd)
end

function CasyncDns:dns_lookup(domain)
    self._coWake = nil
    local res, msg
    if domain == "localhost" then
        return "127.0.0.1"
    end

    local fd, err, errno = psocket.socket(psocket.AF_INET, psocket.SOCK_DGRAM, 0)
    if not fd then
        system:posixError("new socket failed.", err, errno)
    end
    local tDist = {family=psocket.AF_INET, addr=self._server, port=53}

    local co = g_lb:co_add(fd, self.waitReq)
    local query = packQuery(domain)
    local len, err, errno = psocket.sendto(fd, query, tDist)
    if not len then
        system:posixError("socket send failed.", err, errno)
    end

    res, msg = coroutine.resume(co, self, fd)  -- send fd
    assert(res, msg)
    res, msg = coroutine.resume(co, coroutine.running())  -- send to wake
    assert(res, msg)
    return coroutine.yield()
end

return CasyncDns
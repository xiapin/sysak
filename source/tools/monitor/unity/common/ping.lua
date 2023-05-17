---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liaozhaoyan.
--- DateTime: 2023/4/10 00:50
---

require("common.class")

local psocket = require("posix.sys.socket")
local ptime = require("posix.sys.time")
local poll = require("posix.poll")
local unistd = require("posix.unistd")
local system = require("common.system")
local Cping = class("ping")

function Cping:_init_(ip, dev)
    assert(psocket.SOCK_RAW and psocket.SO_BINDTODEVICE, "not support raw socket yet.")
    self.ip = ip

    local fd, err = psocket.socket(psocket.AF_INET, psocket.SOCK_RAW, psocket.IPPROTO_ICMP)
    assert(fd, err)

    local ok, err = psocket.setsockopt(fd, psocket.SOL_SOCKET, psocket.SO_BINDTODEVICE, dev)
    assert(ok, err)

    system:fdNonBlocking(fd)

    self.fd = fd
end

function Cping:_del_()
    if self.fd then
        unistd.close(self.fd)
    end
end

local function diff_us(t1, t0)
    local sum1 = t1.tv_sec * 1e6 + t1.tv_usec
    local sum0 = t0.tv_sec * 1e6 + t0.tv_usec
    return sum1 - sum0
end

function Cping:ping()
    local fd = self.fd
    local data = string.char(0x08, 0x00, 0x89, 0x98, 0x6e, 0x63, 0x00, 0x04, 0x00)
    local t0 = ptime.gettimeofday()
    local ok, err = psocket.sendto(fd, data, {family=psocket.AF_INET, addr=self.ip, port=0})
    assert(ok, err)

    local r = poll.rpoll(fd, 300)
    if r == 0 then
        print("receive over time.")
        return -1
    elseif r == 1 then
        local data, sa = psocket.recvfrom(fd, 1024)
        local t1 = ptime.gettimeofday()
        assert(data, sa)
        return diff_us(t1, t0)
    else
        print("closed.")
        return -2
    end
end

return Cping
---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liaozhaoyan.
--- DateTime: 2023/4/24 22:09
---
require("common.class")

local unistd = require("posix.unistd")
local posix = require("posix")
local utsname = require("posix.sys.utsname")
local ChttpCli = require("httplib.httpCli")

local CbtfLoader = class("btfLoader")

local function regionId()
    local url = "http://100.100.100.200/latest/meta-data/region-id"
    local cli = ChttpCli.new()
    local res = cli:get(url)
    assert(#res.body > 0)
    return res.body
end

local function checkBtf(path)
    if unistd.access("/sys/kernel/btf/vmlinux") then
        return false
    end
    if unistd.access(path) then
        return false
    end
    return true
end

local function checkKo(path)
    if unistd.access(path) then
        return false
    end
    return true
end

local function downBtf(path, region, machine, release)
    local url = "https://sysom-".. region ..".oss-".. region .."-internal.aliyuncs.com/home/hive/btf/".. machine .."/vmlinux-" .. release
    if not unistd.access("/boot") then
        local res, err = posix.mkdir("/boot")
        assert(res == 0, err)
    end
    local cli = ChttpCli.new()
    local res = cli:get(url)
    assert(#res.body > 0)

    local file = io.open(path,"wb")
    file:write(res.body)
    file:close()
end

local function downKo(path, name, region, machine, release)
    local url = "https://sysom-".. region ..".oss-".. region .."-internal.aliyuncs.com/home/hive/sysak/modules/".. machine .."/sysak-" .. release .. ".ko"
    if not unistd.access(path) then
        local res, err, errno = posix.mkdir(path)
        print(res)
        assert(res ~= 0, err .. errno)
    end
    local cli = ChttpCli.new()
    local res = cli:get(url)
    assert(#res.body > 0)

    print(path..name)
    local file = io.open(path .. name,"wb")
    file:write(res.body)
    file:close()
end

function CbtfLoader:_init_(root)
    local distro = utsname.uname()
    if distro then
        local release, machine = distro.release, distro.machine
        local path = '/boot/vmlinux-' .. release
        local region = regionId()
        if checkBtf(path) then
            downBtf(path, region, machine, release)
        end
        local ko = root .. "/lib/" .. release .. "/sysak.ko"
        if checkKo(ko) then
            local path = root .. "/lib/" .. release .. "/"
            ko = downKo(path, "sysak.ko", region, machine, release)
        end
    end
end

return CbtfLoader

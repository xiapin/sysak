---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liaozhaoyan.
--- DateTime: 2022/12/26 11:26 PM
---
package.path = package.path .. ";../?.lua;"

local dirent = require("posix.dirent")
local unistd = require("posix.unistd")
local stat = require("posix.sys.stat")
local bit = require("bit")
local system = require("common.system")
local ptime = require("posix.time")

local srcPath = "../collector/native/"
local dstPath = "../collector/lib/"

local function listSrc(path)
    local res = {}
    local files = dirent.files(path)
    for f in files do
        if string.find(f, "%.so") then
            table.insert(res, f)
        end
    end
    return res
end

local function checkDst()
    if unistd.access(dstPath) then
        local pstat = stat.stat(dstPath)
        if stat.S_ISDIR(pstat.st_mode) == 0 then
            error(string.format("dst %s is no a dictionary", dstPath))
        end
    else
        print("mkdir " .. dstPath)
        local _, s, errno = stat.mkdir(dstPath)
        if errno then
            error(string.format("mkdir %s failed ,report %s. %d", dstPath), s, errno)
        end
    end
end

local function checkSo(fPath)
    local fSrc = srcPath .. fPath
    local fDst = dstPath .. fPath

    if unistd.access(fDst) then
        local sSrc = stat.stat(fSrc)
        local sDst = stat.stat(fDst)

        if sSrc.st_mtime > sDst.st_mtime then  -- modified
            return true
        else
            return false
        end
    else   -- exit
        return true
    end
end

local function copySo(fPath)
    local fSrc = srcPath .. fPath
    local fDst = dstPath .. fPath

    local sFile, err = io.open(fSrc,"rb")
    if err then
        error(string.format("open file %s report %s."), fSrc, err)
    end
    local stream = sFile:read("*a")
    sFile:close()

    local dFile, err = io.open(fDst,"wb")
    if err then
        error(string.format("open file %s report %s."), fDst, err)
    end
    dFile:write(stream)
    dFile:close()

    stat.chmod(fDst, bit.bor(stat.S_IRWXU, stat.S_IRGRP, stat.S_IROTH))
end

local function checkSos()
    print(unistd.getcwd())
    checkDst()
    local so_s = listSrc(srcPath)
    for _, so in ipairs(so_s) do
        if checkSo(so) then
            print("need copy " .. so)
            copySo(so)
        end
    end
end

local function setupFreq(fYaml)
    local conf = system:parseYaml(fYaml)
    if conf then
        local ret = tonumber(conf.config.freq)
        if ret > 5 then
            return conf.config.freq
        else
            return 5
        end
    else
        error("load yaml file failed.")
        return -1
    end
end

local function calcSleep(hope, now)
    if hope.tv_nsec >= now.tv_nsec then
        return {tv_sec  = hope.tv_sec - now.tv_sec,
                tv_nsec = hope.tv_nsec - now.tv_nsec}
    else
        return {tv_sec  = hope.tv_sec - now.tv_sec - 1,
                tv_nsec = 1e9 + hope.tv_nsec - now.tv_nsec}
    end
end

function work(que, proto_q, yaml)
    local fYaml = yaml or "../collector/plugin.yaml"
    checkSos()
    local Cloop = require("collector.loop")
    local w = Cloop.new(que, proto_q, fYaml)
    local unit = setupFreq(fYaml)
    local tStart = ptime.clock_gettime(ptime.CLOCK_MONOTONIC)
    while true do
        w:work(unit)
        local now = ptime.clock_gettime(ptime.CLOCK_MONOTONIC)
        local hope = {tv_sec = tStart.tv_sec + unit, tv_nsec = tStart.tv_sec}
        local diff = calcSleep(hope, now)
        assert(diff.tv_sec >= 0)
        local _, s, errno, _ = ptime.nanosleep(diff)
        if errno then   -- interrupt by signal
            print(string.format("new sleep stop. %d, %s", errno, s))
            return 0
        end
        tStart = hope
    end
end

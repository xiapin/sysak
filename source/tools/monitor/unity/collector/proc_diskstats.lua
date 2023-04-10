---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liaozhaoyan.
--- DateTime: 2022/12/16 11:49 PM
---

require("common.class")
local system = require("common.system")
local CvProc = require("collector.vproc")

local CprocDiskstats = class("proc_diskstats", CvProc)

function CprocDiskstats:_init_(proto, pffi, mnt, pFile)
    CvProc._init_(self, proto, pffi, mnt, pFile or "proc/diskstats")
    self._lastData = {}
    self._lastDisk = {}
    self._diskVNum = 11
end

function CprocDiskstats:_diskIndex()
    return {
        "reads", "rmerge", "rkb", "rmsec",
        "writes", "wmerge", "wkb", "wmsec",
        "inflight", "time", "backlog"
    }
end

function CprocDiskstats:_diffIndex()
    return {
        "reads", "rmerge", "rkb", "rmsec",
        "writes", "wmerge", "wkb", "wmsec",
        "backlog", "xfers"
    }
end

-- "reads", "rmerge", "rkb", "rmsec", "writes", "wmerge", "wkb", "wmsec",  "inflight", "time", "backlog"
function CprocDiskstats:_getNewValue(data)
    local now = {}
    local index = self:_diskIndex()
    for i = 1, self._diskVNum do
        local head = index[i]
        now[head] = tonumber(self._ffi.string(data.s[i + 2]))
    end

    now["rkb"] = now["rkb"] / 2  -- sectors = 512 bytes
    now['wkb'] = now['wkb'] / 2
    now['xfers'] = now['reads'] + now['writes']
    if now['xfers'] == 0 then
        now['bsize'] = 0
    else
        now['bsize'] = (now['rkb'] + now['wkb']) * 1024 / now['xfers']
    end

    now['time'] = now['time'] / 10.0
    return now
end

function CprocDiskstats:_calcDiff(disk_name, now, last, elapsed)
    local protoTable = {
        line = "disks",
        ls = {{name = "disk_name", index = disk_name}},
        vs = {}
    }
    local index = self:_diffIndex()

    for i = 1, #index do
        local value = (now[index[i]] - last[index[i]]) / elapsed
        local cell = {
            name = index[i],
            value = value
        }
        table.insert(protoTable.vs, cell)
    end

    local cell = {
        name = "inflight",
        value = now["inflight"]
    }
    table.insert(protoTable.vs, cell)

    cell = {
        name = "bsize",
        value = now["bsize"]
    }
    table.insert(protoTable.vs, cell)

    cell = {
        name = "busy",
        value = (now["time"] - last["time"]) / elapsed
    }
    table.insert(protoTable.vs, cell)
    self:appendLine(protoTable)
end

function CprocDiskstats:_calcDisk(disk_name, data, elapsed)
    local now = self:_getNewValue(data)
    local last = self._lastData[disk_name]

    if last then
        self:_calcDiff(disk_name, now, last, elapsed)
    end
    self._lastData[disk_name] = now
    self._lastDisk[disk_name] = 1
end

function CprocDiskstats:checkLastDisks()
    for k, _ in pairs(self._lastData) do
        if not self._lastDisk[k] then
            self._lastData[k] = nil
        end
    end
    self._lastDisk = {}
end

function CprocDiskstats:_proc(line, elapsed)
    local data = self._ffi.new("var_string_t")
    assert(self._cffi.var_input_string(self._ffi.string(line), data) == 0)
    assert(data.no >= 14)

    local disk_name = self._ffi.string(data.s[2])
    self:_calcDisk(disk_name, data, elapsed)
end

function CprocDiskstats:proc(elapsed, lines)
    CvProc.proc(self)
    for line in io.lines(self.pFile) do
        self:_proc(line, elapsed)
    end
    self:checkLastDisks()
    self:push(lines)
end

return CprocDiskstats

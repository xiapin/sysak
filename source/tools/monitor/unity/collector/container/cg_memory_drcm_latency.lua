---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liaozhaoyan.
--- DateTime: 2023/3/8 2:32 PM
---

require("common.class")
local pystring = require("common.pystring")
local CvProc = require("collector.vproc")
local root = "sys/fs/cgroup/memory/"
local dfile = "/memory.direct_reclaim_memcg_latency"

local CgMemDrcmLatency = class("cg_memDrcm_latency", CvProc)

--ls{}, (pod_name and docker_name
function CgMemDrcmLatency:_init_(proto, pffi, mnt, path, ls)
    CvProc._init_(self, proto, pffi, mnt, root .. path .. dfile)
	self.ls = ls
end

function CgMemDrcmLatency:global_proc(cPath)
    local c = 1
    local values = {}
    local gPath = cPath..""
    gPath = string.gsub(gPath, "direct_reclaim_memcg_latency", "direct_reclaim_global_latency", 1)
    for line in io.lines(gPath) do
        local cell = pystring:split(line)
        values[c] = tonumber(cell[2])
        c = c + 1
    end
    return values
end

function CgMemDrcmLatency:proc(elapsed, lines)
    -- if pFile not valid ,return -1
    local c = 1
    CvProc.proc(self)
    local values = {}
    local gvalues = {}
    gvalues = self:global_proc(self.pFile)
    for line in io.lines(self.pFile) do
        local cell = pystring:split(line)
        local tmp = cell[1]
        tmp = string.gsub(tmp, ":", "", 1)
        tmp = string.gsub(tmp, ">=", "", 1)
        tmp = string.gsub(tmp, "-", "to", 1)
        tmp = string.gsub(tmp, "%(.*%)", "", 1)
        values[c] = {
            name = "memDrcm_lat_"..tmp,
            value = tonumber(cell[2]) + gvalues[c]
        }
        c = c + 1
    end
    self:appendLine(self:_packProto("cg_memdrcm_latency", self.ls, values))
    self:push(lines)
end

return CgMemDrcmLatency

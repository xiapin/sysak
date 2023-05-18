---
--- Created by Hailong Liu.
--- DateTime: 2023/5/18
---

require("common.class")
local unistd = require("posix.unistd")
local pystring = require("common.pystring")
local CvProc = require("collector.vproc")
local root = "sys/fs/cgroup/cpu/"
local quotFile = "/cpu.cfs_quota_us"
local periodFile = "/cpu.cfs_period_us"

local CgCfsQuota = class("cg_cfs_quota", CvProc)

--ls{}, (pod_name and docker_name
function CgCfsQuota:_init_(proto, pffi, mnt, path, ls)
    CvProc._init_(self, proto, pffi, mnt, root .. path .. quotFile)
	self.ls = ls
	self.period = 0
	self.quota = 0
	self.nr_cpus = unistd.sysconf(84)
	self.periodFp = mnt..root..path..periodFile
end

function CgCfsQuota:proc(elapsed, lines)
	-- if pFile not valid ,return -1
    local c = 1
    CvProc.proc(self)
    local values = {}

    for line in io.lines(self.pFile) do
        local cell = pystring:split(line)
	self.quota = tonumber(cell[1]) 
        values[c] = {
            name = "quota_us",
            value = self.quota
        }
        c = c + 1
    end

    for line in io.lines(self.periodFp) do
        local cell = pystring:split(line)
	self.period = tonumber(cell[1]) 
        values[c] = {
            name = "period_us",
            value = self.period
        }
        c = c + 1
    end
    local ratio = self.nr_cpus*100
    if self.quota ~= -1 then
	ratio = self.quota*100.0/self.period
    end

    values[c] = {
        name = "quota_ratio",
        value = ratio
    }
    self:appendLine(self:_packProto("cgCpuQuota", self.ls, values))
    self:push(lines)
end

return CgCfsQuota

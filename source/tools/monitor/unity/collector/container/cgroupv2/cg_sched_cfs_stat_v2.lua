require("common.class")
local unistd = require("posix.unistd")
local pystring = require("common.pystring")
local CvProc = require("collector.vproc")
local root = "sys/fs/cgroup/"
local dfile = "/cpu.sched_cfs_statistics"

local cgCpuSchedCfsStatV2 = class("cg_sched_cfs_stat_v2", CvProc)

function cgCpuSchedCfsStatV2:_init_(proto, pffi, mnt, path, ls)
    CvProc._init_(self, proto, pffi, mnt, root .. path .. dfile)
    self.ls = ls
end

function cgCpuSchedCfsStatV2:getMetricNames()
    return { "serve", "oncpu", "queue_other", "queue_sibling", "queue_max", "force_idle" };
end

function cgCpuSchedCfsStatV2:proc(elapsed, lines)
    if not unistd.access(self.pFile) then
        return
    end
    CvProc.proc(self)
    local values = {}
    local metrics = self:getMetricNames()
    for line in io.lines(self.pFile) do
        local cell = pystring:split(line)
        for c, val in ipairs(cell) do
            values[c] = {
                name = metrics[c],
                value = tonumber(val)
            }
        end
        break
    end
    self:appendLine(self:_packProto("cg_sched_cfs_stat_v2", self.ls, values))
    self:push(lines)
end

return cgCpuSchedCfsStatV2

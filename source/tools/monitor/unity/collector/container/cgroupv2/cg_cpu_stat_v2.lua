require("common.class")

local unistd = require("posix.unistd")
local pystring = require("common.pystring")
local CvProc = require("collector.vproc")
local root = "sys/fs/cgroup/"
local dfile = "/cpu.stat"

local cgCpuStatV2 = class("cg_cpu_stat_v2", CvProc)

function cgCpuStatV2:_init_(proto, pffi, mnt, path, ls)
    CvProc._init_(self, proto, pffi, mnt, root .. path .. dfile)
    self.ls = ls
end

function cgCpuStatV2:proc(elapsed, lines)
    if not unistd.access(self.pFile) then
        return
    end

    CvProc.proc(self)
    local c = 1
    local values = {}

    for line in io.lines(self.pFile) do
        local cell = pystring:split(line)
        values[c] = {
            name = cell[1],
            value = tonumber(cell[2])
        }
        c = c + 1
    end
    self:appendLine(self:_packProto("cg_cpu_stat_v2", self.ls, values))
    self:push(lines)
end

return cgCpuStatV2

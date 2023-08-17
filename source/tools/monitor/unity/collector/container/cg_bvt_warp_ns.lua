require("common.class")
local pystring = require("common.pystring")
local CvProc = require("collector.vproc")
local root = "sys/fs/cgroup/cpu/"
local dfile = "/cpu.bvt_warp_ns"

local cgBvtWarpNs = class("cg_bvt_warp_ns", CvProc)

function cgBvtWarpNs:_init_(proto, pffi, mnt, path, ls)
    CvProc._init_(self, proto, pffi, mnt, root .. path .. dfile)
    self.ls = ls
end

function cgBvtWarpNs:proc(elapsed, lines)
    local c = 1
    CvProc.proc(self)
    local values = {}

    local f = io.open(self.pFile, "r")
    if f ~= nil then
        io.close(f)
    else
        return
    end

    for line in io.lines(self.pFile) do
        values = {
            name = 'bvt_warp_ns',
            value = tonumber(line)
        }
        break
    end
    self:appendLine(self:_packProto("cg_bvt_warp_ns", self.ls, values))
    self:push(lines)
end

return cgBvtWarpNs

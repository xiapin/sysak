---
--- Created by liaozhaoyan.
--- DateTime: 2023/5/18
---

require("common.class")
local pystring = require("common.pystring")
local CvProc = require("collector.vproc")
local root = "sys/fs/cgroup/memory/"
local dfile = "/memory.direct_reclaim_global_latency"

local CgMemGlobDrcmLat = class("cgGlbDrcmLatency", CvProc)

--ls{}, (pod_name and docker_name
function CgMemGlobDrcmLat:_init_(proto, pffi, mnt, path, ls)
    CvProc._init_(self, proto, pffi, mnt, root .. path .. dfile)
	self.ls = ls
end

function CgMemGlobDrcmLat:proc(elapsed, lines)
	-- if pFile not valid ,return -1
    local c = 1
    CvProc.proc(self)
    local values = {}

    for line in io.lines(self.pFile) do
        local cell = pystring:split(line)
	    local tmp = cell[1]
	    tmp = string.gsub(tmp, ":", "", 1)
	    tmp = string.gsub(tmp, ">=", "", 1)
	    tmp = string.gsub(tmp, "-", "to", 1)
	    tmp = string.gsub(tmp, "%(.*%)", "", 1)
        values[c] = {
            name = "memDrcm_glb_lat_"..tmp,
            value = tonumber(cell[2])
        }
        c = c + 1
    end
    self:appendLine(self:_packProto("cgGlbDrcmLatency", self.ls, values))
    self:push(lines)
end

return CgMemGlobDrcmLat

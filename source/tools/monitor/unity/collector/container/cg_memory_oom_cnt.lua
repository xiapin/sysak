require("common.class")
local pystring = require("common.pystring")
local CvProc = require("collector.vproc")
local root = "sys/fs/cgroup/memory/"
local dfile = "/memory.oom_control"

local CgMemOomCnt = class("cg_memoom_cnt", CvProc)

--ls{}, (pod_name and docker_name
function CgMemOomCnt:_init_(proto, pffi, mnt, path, ls)
    CvProc._init_(self, proto, pffi, mnt, root .. path .. dfile)
	self.ls = ls
end

function CgMemOomCnt:proc(elapsed, lines)
	-- if pFile not valid ,return -1
    local c = 1
    CvProc.proc(self)
    local values = {}

    for line in io.lines(self.pFile) do
        if string.find(line, "oom_kill") then
            local cell = pystring:split(line)
            values[c] = {
                name = "oom_kill",
                value = tonumber(cell[2])
            }
            c = c + 1
        end
    end
    self:appendLine(self:_packProto("cg_memoom_cnt", self.ls, values))
    self:push(lines)
end

return CgMemOomCnt
require("common.class")
local pystring = require("common.pystring")
local CvProc = require("collector.vproc")
local root = "sys/fs/cgroup/memory/"
local dfile = "/memory.stat"
local usage = "/memory.usage_in_bytes"
local limit = "/memory.limit_in_bytes"
local system = require("common.system")

local CgMemUtil = class("cg_memory_util", CvProc)

--ls{}, (pod_name and docker_name
function CgMemUtil:_init_(proto, pffi, mnt, path, ls)
	CvProc._init_(self, proto, pffi, mnt, root .. path .. dfile)
	self.ls = ls
	self.path = mnt..root..path..dfile
	self.limitpath = mnt..root..path..limit
	self.usagepath = mnt..root..path..usage
	self.limit = 0
	self.usage = 0
end

function CgMemUtil:_getLimit_()
	local pfile = io.open(self.limitpath, "r")
	local line = pfile:read()
	self.limit = tonumber(line)
	io.close(pfile)
end

function CgMemUtil:_getUsage_()
	local pfile = io.open(self.usagepath, "r")
	local line = pfile:read()
	self.usage = tonumber(line)
	io.close(pfile)
end

function CgMemUtil:proc(elapsed, lines)
    local c = 1
    local k = 1
    CvProc.proc(self)
    self:_getLimit_()
    self:_getUsage_()
    local values = {}
    for line in io.lines(self.pFile) do
	local name
	local cell = pystring:split(line)
	local num = #cell
	local val = tonumber(cell[num])
	--we assume that: memory.use_hierarchy is "1"
	if ("total_cache" == cell[1]) or ("total_rss" == cell[1]) then
		name = string.sub(cell[1], 7)
		values[k] = {
			name = name,
			value = val
		}
		k = k + 1
		local ratio = (100.00*val) / tonumber(self.usage)
		values[k] = {
			name = name.."_ratio",
			value = ratio
		}
		k = k + 1
        	c = c + 1
	end
	if c > 3 then
		break
	end
    end
    values[k] = {
 	 name = "usage",
	 value = self.usage
    }
    values[k+1] = {
         name = "mem_util",
         value = (tonumber(self.usage)*100.0)/ tonumber(self.limit)
    }
    self:appendLine(self:_packProto("cg_memory_util", self.ls, values))
    self:push(lines)
end

return CgMemUtil

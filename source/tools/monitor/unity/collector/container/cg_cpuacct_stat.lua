require("common.class")
local pystring = require("common.pystring")
local CvProc = require("collector.vproc")
local root = "sys/fs/cgroup/cpuacct/"
local dfile = "/cpuacct.stat"
local procstat = "proc/stat"
local system = require("common.system")

local CgCpuacctStat = class("cg_proc_stat", CvProc)

--ls{}, (pod_name and docker_name
function CgCpuacctStat:_init_(proto, pffi, mnt, path, ls)
	CvProc._init_(self, proto, pffi, mnt, root .. path .. dfile)
	self.ls = ls
	self.path = mnt..root..path..dfile
	self.statpath = mnt..procstat
	self.hostPrev = 0
	self.hostNow = 0
	self.hostCpuSum = 0
	self.conTotal = 0
	self.values = {0,0,0,0,0,0,0,0,0}
end

function CgCpuacctStat:_getTotal_()
	local pfile = io.open(self.statpath, "r")
	local line = pfile:read()
	local sum = 0
	local s = string.sub(line, 4)
	local data = self._ffi.new("var_long_t")
	assert(self._cffi.var_input_long(self._ffi.string(s), data) == 0)
	assert(data.no == 10)
        for i = 1, data.no do
            sum = sum + data.value[i]
	end
	self.hostPrev = self.hostNow
	self.hostNow = sum
	self.hostCpuSum = sum - self.hostPrev
	io.close(pfile)
end

function CgCpuacctStat:proc(elapsed, lines)
    local c = 1
    CvProc.proc(self)
    self:_getTotal_()
    local values = {}
    self.conTotal = 0
    for line in io.lines(self.pFile) do
	local name
	local cell = pystring:split(line)
	local num = #cell
	if c < 3 then
		name = cell[1]
		local prev = self.values[c]
		local now = tonumber(cell[num])
		local rate = tonumber(((now - prev)*100.0) / self.hostCpuSum)
		values[c] = {
			name = name,
			value = tonumber(rate)
		}
		self.values[c] = now
		self.conTotal = self.conTotal + (now - prev)
	end
        c = c + 1
    end
    values[c] = {
	name = "total",
	value = tonumber((self.conTotal*100.0)/self.hostCpuSum)
	}
    self:appendLine(self:_packProto("cg_cpuacct_stat", self.ls, values))
    self:push(lines)
end

return CgCpuacctStat

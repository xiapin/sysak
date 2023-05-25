require("common.class")
local pystring = require("common.pystring")
local CvProc = require("collector.vproc")
local unistd = require("posix.unistd")
local system = require("common.system")
local cgpmuffi = require("collector.native.cgpmuffi")

local root = "sys/fs/cgroup/perf_event/"
local cgPmu = class("cg_pmu_events", CvProc)

function cgPmu:_init_(proto, pffi, mnt, path, ls)
	CvProc._init_(self, proto, pffi, mnt, root .. path)
	self.ls = ls
	local ffi = cgpmuffi["rawffi"]
	self._cgpmuffi = cgpmuffi["cgpmuffi"]
 	self._rawffi = cgpmuffi["rawffi"]
	self.all_cpus_t = ffi.typeof("pcpu_hwi_t[?]")
	self.double_arry_t = ffi.typeof("double[?]")
	self.c_str_t = ffi.typeof("const char*")
	self.nr_events = 7
	self.nr_cpus = unistd.sysconf(84)
	--print("nr_cpus="..tonumber(self.nr_cpus))
	self.allCpuInfo = ffi.new(self.all_cpus_t, self.nr_cpus)
	self.summary = ffi.new(self.double_arry_t, self.nr_events)

	--self._cgpmuffi.my_sleep(1)
	self.cpath = mnt..root..path
	--print("cg_pmu_events: _init_:create_hw_events")
	self._cgpmuffi.create_hw_events(self.allCpuInfo, self.nr_cpus, self.cpath)
	self.stoped = 0
end

function cgPmu:_drcName()
	return {"cycles", "ins", "refCyc", "llcLoad",
		"llcLoadMis", "llcStore", "llcStoreMis"}
end

function cgPmu:_compName()
	return {"CPI", "IPC", "MPI", "l3LoadMisRate", 
		"l3StoreMisRate", "l3MisRate"}
end

function cgPmu:fillCompValue(sum, values, index)
	local csum = {}
	local compNames = self:_compName()
	local cpi, ipc, mpi, l3loadmisr, l3storemisr, l3misr
	local CYC, INS, REFCYC, L3LOAD, L3LOADMS, L3STO, L3STOMS
	CYC = 0
	INS = 1
	REFCYC = 2
	L3LOAD = 3
	L3LOADMS = 4
	L3STO = 5
	L3STOMS = 6
	if sum[INS] ~= 0 then
		table.insert(csum, tonumber(sum[CYC]/sum[INS]))
		table.insert(csum, tonumber((sum[L3LOADMS]+sum[L3STOMS])/sum[INS]))
	else
		table.insert(csum, 0)
		table.insert(csum, 0)
	end

	if sum[CYC] ~= 0 then
		table.insert(csum, sum[INS]/sum[CYC])
	else
		table.insert(csum, 0)
	end

	if sum[L3LOAD] ~= 0 then
		table.insert(csum, sum[L3LOADMS]/sum[L3LOAD])
	else
		table.insert(csum, 0)
	end

	if sum[L3STO] ~= 0 then
		table.insert(csum, sum[L3STOMS]/sum[L3STO])
	else
		table.insert(csum, 0)
	end

	if (sum[L3LOAD]+sum[L3STO]) ~= 0 then
		table.insert(csum, (sum[L3LOADMS]+sum[L3STOMS])/(sum[L3LOAD]+sum[L3STO]))
	else
		table.insert(csum, 0)
	end
	local c = index
	--for i = 1, #compNames do
	for k,v in ipairs(csum) do
		values[c] = {
			name = compNames[k],
			value = v
		}
		c = c + 1
	end
end

function cgPmu:proc(elapsed, lines)
	CvProc.proc(self)
	local c = 1
	local values = {}
	local direcNames = self:_drcName()
	self._cgpmuffi.collect_events(self.allCpuInfo, self.nr_cpus, self.summary)
	local sum = self.summary
	for i = 1, #direcNames do
		values[c] = {
			name = direcNames[i],
			value = tonumber(sum[i-1])
		}
		c = c + 1
	end

	local compNames = self:_compName()
	self:fillCompValue(sum, values, c)

	self:appendLine(self:_packProto("pmu_cg_events", self.ls, values))
	self:push(lines)
end

function cgPmu:releaseEvents()
	local ret = self._cgpmuffi.stop_events(self.allCpuInfo, self.nr_cpus)
	if ret == 0 then
		self.stoped = 1
	end
end

function cgPmu:_del_()
	if self.stoped ~= 1 then
		self._cgpmuffi.stop_events(self.allCpuInfo, self.nr_cpus)
	end
end

return cgPmu

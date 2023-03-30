local rawffi = require "ffi"
local cgpmuffi = rawffi.load('cgpmuffi')
local unistd = require("posix.unistd")

rawffi.cdef[[
	struct event_info {
		int fd;
		unsigned long long prev, cnt, delta;
	}event_info_t;

	typedef struct pcpu_hwi {
		struct event_info ei[7];
	} pcpu_hwi_t;

	void my_sleep(int x);
	int create_hw_events(pcpu_hwi_t *hwi, int nr_cpus, const char *path);
	int collect_events(pcpu_hwi_t *hwi, int nr_cpus, double *summary);
	int stop_events(pcpu_hwi_t *hwi, int nr_cpus);
]]
--[[
local all_cpus_t = rawffi.typeof("pcpu_hwi_t[?]")
local double_arry_t = rawffi.typeof("double[?]")
local c_str_t = rawffi.typeof("const char*")

--step 1: init--
--post to me 
local nr_cpu = unistd.sysconf(84)
print("nr_cpu="..nr_cpu)
local nr_events = 7
local parent_path = "/sys/fs/cgroup/perf_event"
local child_path = "/docker/"
--local mnt="/mnt/host"
local mnt="/"

local real_path = mnt..parent_path..child_path

local cpath = rawffi.cast(c_str_t, real_path)
local allCpuInfo = rawffi.new(all_cpus_t, nr_cpu)
local ret = cgpmuffi.create_hw_events(allCpuInfo, nr_cpu, cpath)


--step 2: collect and fill line --
local eventStrs = {"cycles", "ins", "refCyc",
		"llcLoad", "llcLoadMis",
		"llcStore", "llcStoreMis"}
local summary = rawffi.new(double_arry_t, nr_events)
ret = cgpmuffi.collect_events(allCpuInfo, nr_cpu, summary)
cgpmuffi.my_sleep(3)
ret = cgpmuffi.collect_events(allCpuInfo, nr_cpu, summary)
--]]
--[[ for debug
for i = 0, nr_cpu-1 do
	local pcpues = allCpuInfo[i]
	for j = 0, nr_events-1 do
		local ei = pcpues.ei[j]
		print("cpu"..i.." "..eventStrs[j+1].."="..tonumber(ei.delta))
	end
end
]]--
--[[
for j = 0, nr_events-1 do
	print("summary "..eventStrs[j+1].."="..tonumber(summary[j]))
end

local cpi, ipc, mpi, ref, miss, loadmis, storemis, missrate
local CYC, INS, REFCYC, L3LOAD, L3LOADMS, L3STO, L3STOMS
CYC = 0
INS = 1
REFCYC = 2
L3LOAD = 3
L3LOADMS = 4
L3STO = 5
L3STOMS = 6

ref = summary[L3LOAD] + summary[L3STO]
miss = summary[L3LOADMS] + summary[L3STOMS]
if summary[INS] ~= 0 then
	cpi = tonumber(summary[CYC]/summary[INS])
	mpi = tonumber(summary[miss]/summary[INS])
else
	cpi = 0.0
	mpi = 0.0
end

if summary[CYC] ~= 0 then
	ipc = tonumber(summary[INS]/summary[CYC])
else
	ipc = 0.0
end
print("summary ".."ipc="..ipc..",cpi="..cpi..",mpi="..mpi)

if summary[L3LOAD] ~= 0 then
	loadmis = tonumber(summary[L3LOADMS]/summary[L3LOAD])
else
	loadmis = 0.0
end

if summary[L3STO] ~= 0 then
	storemis = tonumber(summary[L3STOMS]/summary[L3STO])
else
	storemis = 0.0
end

if ref ~= 0 then
	missrate = tonumber(miss/ref)
else
	missrate = 0.0
end
print("LLC ref="..ref..",miss="..miss)
print("LLC loadmiss="..loadmis..",storemiss="..storemis..",misrate="..missrate)

print("-----------------")
--todo: how to fiil allCpuInfo to lines ??--


--step 3: deinit --
ret = cgpmuffi.stop_events(allCpuInfo, nr_cpu)
]]--
return {rawffi = rawffi, cgpmuffi=cgpmuffi}

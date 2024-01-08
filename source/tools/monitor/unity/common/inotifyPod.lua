require("common.class")

local Cinotifies = require("common.inotifies")
local system = require("common.system")
local dirent = require("posix.dirent")
local pstat = require("posix.sys.stat")
local inotify = require('inotify')

local CinotifyPod = class("inotifyPod", Cinotifies)

function CinotifyPod:_init_()
	Cinotifies._init_(self, nil)
	self.kube_pod_paths = {
		"sys/fs/cgroup/cpu/kubepods.slice",
		"sys/fs/cgroup/cpu/kubepods.slice/kubepods-besteffort.slice",
		"sys/fs/cgroup/cpu/kubepods.slice/kubepods-burstable.slice",
		"sys/fs/cgroup/cpu/kubepods",
		"sys/fs/cgroup/cpu/kubepods/besteffort",
		"sys/fs/cgroup/cpu/kubepods/burstable"
	}
	self._pod_ws = {} -- record ["pod_path"] = pod_ws(return value of addwatch)
	self._kpp_map = {} -- record [kube_pod_path_ws] = "kube_pod_paths"
end

-- watch kubepod dirs (to watch pods' changes)
function CinotifyPod:watchKubePod(mnt)
	for _, path in ipairs(self.kube_pod_paths) do
		local watch_path = mnt .. path
		local w = self._handle:addwatch(watch_path, inotify.IN_CREATE, inotify.IN_MOVE, inotify.IN_DELETE)
		if w ~= nil then 
			if w > 0 then
				--print("watchKubePod: " .. watch_path)
				table.insert(self._ws, w)
				self._kpp_map[w] = watch_path
			else
				error("add" .. watch_path .. "to watch failed.")
			end
		end
	end
end

-- watch pod's dir (to watch container's changes)
function CinotifyPod:addPodWatch(pod_path)
	if self._pod_ws[pod_path] ~= nil then 	
		return
	end

	local pws = self._handle:addwatch(pod_path, inotify.IN_CREATE, inotify.IN_MOVE, inotify.IN_DELETE)
	if pws ~= nil then 
		if pws > 0 then
			--print("add " .. pod_path .. "to watch")
			self._pod_ws[pod_path] = pws
		else
			error("add " .. pod_path .. "to watch")
		end
	end 
end

-- check if the pod has been delete, if so, remove the watch
function CinotifyPod:RemoveDeletePodWatch(events)
	local dp_paths = {}
	for _, event in ipairs(events) do
		-- todo: now pod creations can also pass this event.mask check
		-- add another check in line 73 to filter creation temporary
		if bit.band(event.mask, inotify.IN_DELETE) then 
			for wd, kube_path in pairs(self._kpp_map) do
				if event.wd == wd then
					local d_path = kube_path .. "/" .. event.name .. "/"
					--print("delete_pod_path: " .. d_path)
					table.insert(dp_paths, d_path)
				end
			end
		end
	end
	
	for _, d_path in ipairs(dp_paths) do
		if self._pod_ws[d_path] == nil then goto continue end
		self._handle:rmwatch(self._pod_ws[d_path])
		self._pod_ws[d_path] = nil
		::continue::
	end
end

function CinotifyPod:_del_()
	for _, w in ipairs(self._ws) do
        self._handle:rmwatch(w)
    end
	for _, w in pairs(self._pod_ws) do
		self._handle:rmwatch(w)
	end
    self._handle:close()
end

function CinotifyPod:isChange()
	local events = self._handle:read()
    if events ~=nil then
        if #events > 0 then
			return true, events
		end
    end
    return false, nil
end

return CinotifyPod 

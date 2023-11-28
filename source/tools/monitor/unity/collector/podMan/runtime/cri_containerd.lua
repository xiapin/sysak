require("common.class")

local unistd = require("posix.unistd")
local CinotifyPod = require("common.inotifyPod")
local cjson = require("cjson")

local CcriContainerd = class("criContainerd")

local containerdEnpoint = {"/run/containerd/containerd.sock",
                           "/var/run/containerd/containerd.sock",
                           "/var/run/containerd/containerd.sock"}

function CcriContainerd:_init_(resYaml, mnt)
    self._name = "cri-containerd"
    self._mnt = mnt
    self._ino = {}
    local ffi = require("collector.podMan.runtime.cri.ffi_lua")
    self._ffi = ffi.ffi
    self._awsome = ffi.awesome
    self._valid_endpoint = ""
    self._ns_blacklist = {}

    local ns_blacklist = resYaml.container.nsBlacklist
    for _, ns in ipairs(ns_blacklist) do
        self._ns_blacklist[ns] = 1
    end
end

function CcriContainerd:checkRuntime()
    for _, ep in ipairs(containerdEnpoint) do
        local endpoint = "unix://"..self._mnt..ep
        local endpoint_ffi = self._ffi.new("GoString")
        endpoint_ffi.p = endpoint
        endpoint_ffi.n = #endpoint
        local endpoint_ptr = self._ffi.cast("GoString*", endpoint_ffi)
        local is_enable = self._awsome.CheckRuntime(endpoint_ffi)
        if is_enable == 1 then
            self._valid_endpoint = endpoint
            print("Using cri-containerd api.")
            return 1
        end
    end
    return 0
end

function CcriContainerd:cgroupChanged()
    local is_change, events = self._ino:isChange()
    if is_change then
        print("cri-containerd: cgroup changed.")
        if events ~= nil then
            -- reomeve inotify watch on deleted pod dirs
			self._ino:RemoveDeletePodWatch(events)
        end
    end
    return is_change
end

function CcriContainerd:initInotify()
    --[[
        in k8s, need to watch 
        "sys/fs/cgroup/cpu/kubepods.slice",
		"sys/fs/cgroup/cpu/kubepods.slice/kubepods-besteffort.slice",
		"sys/fs/cgroup/cpu/kubepods.slice/kubepods-burstable.slice"
    --]]
    self._ino = CinotifyPod.new()
	self._ino:watchKubePod(self._mnt)
end

function CcriContainerd:queryPodsInfo()
    local resptr = self._awsome.CGetContainerInfosfunc(self._valid_endpoint)
    if not resptr then return nil end
    local infoString = self._ffi.string(resptr)
    self._ffi.C.free(resptr)
    return cjson.decode(infoString)
end

function CcriContainerd:setupCons()
    local mnt = self._mnt
    local cons = {}
    local c = 0

    local consInfo = self:queryPodsInfo()
    if not consInfo then
        print("cri-containerd: Can't get pods info!")
        return nil
    end

    for _, cs in ipairs(consInfo) do
        local namespace = cs.Namespace
        if self._ns_blacklist[namespace] then
            goto continue
        end

		-- watch pod dirs(containers' changes in pod)
        local pod_path = cs.PodCgroup
		local full_pod_path = mnt .. "sys/fs/cgroup/cpu" .. pod_path
		if unistd.access(full_pod_path) == 0 then
			self._ino:addPodWatch(full_pod_path)
		end

        local lpod = {
            name = cs.PodName,
            namespace = cs.Namespace,
        }

        local cell = {
            pod = lpod,
            name = cs.ContainerName,
            id = cs.Id
        }
          
        cell.path = cs.ContainerCgroup
        if unistd.access(mnt .. "sys/fs/cgroup/cpu/" .. cell.path) == 0 then
            c = c + 1
            cons[c] = cell
        end

        ::continue::
    end

    return cons
end

return CcriContainerd
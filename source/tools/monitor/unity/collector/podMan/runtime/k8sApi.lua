require("common.class")
local unistd = require("posix.unistd")
local json = require("cjson")
local https = require("ssl.https")
local ltn12 = require("ltn12")
local ChttpCli = require("httplib.httpCli")

local pystring = require("common.pystring")
local CinotifyPod = require("common.inotifyPod")

local Ck8sApi = class("podsApi")
local default_token_path = "/var/run/secrets/kubernetes.io/serviceaccount/token"
local default_ca_path = "/var/run/secrets/kubernetes.io/serviceaccount/ca.crt"

local function spiltConId(conId)
    local res = pystring:split(conId, "//", 1)
    return res[2]
end

local function getRuntime(mnt)
    if unistd.access(mnt .. "var/run/docker/runtime-runc/moby/") == 0 then
        return "docker"
    end
    return "cri-containerd"
end

local function podPath(lpod)
	local paths
	if lpod.qos == "guaranteed" then
		paths = {
			"/kubepods.slice/kubepods-pod",
			lpod.uid,
			".slice/"
		}
	else
		paths = {
			"/kubepods.slice/kubepods-",
			lpod.qos,
			".slice/kubepods-",
			lpod.qos,
			"-pod",
			lpod.uid,
			".slice/"
		}
	end
	return pystring:join("", paths)
end 

local function joinContainerPath(pod_path, cell, runtime)
    if pod_path == nil then 
        pod_path = podPath(cell.pod)
    end

    local paths = {
        pod_path,
        runtime,
        "-",
        cell.id,
        ".scope"
    }
    return pystring:join("", paths)
end

local function getQosStr()
        return {"BE", "NOR", "LS", "LS", "OT"}
end

local function get_bvt(bvt_path)
    -- local bvt_path = conpath.."cpu.bvt_warp_ns"
    local value = 5
    if unistd.access(bvt_path) == 0 then
        for line in io.lines(bvt_path) do
            value = tonumber(line) + 2
            break
        end
    end
    names = getQosStr()
    return names[value]
end

function Ck8sApi:queryPodsInfo()
    if self._token == "" then
        local f = io.open(self._token_path, "r")
        if not f then
            print("service token not exist!")
            return nil
        end

        self._token = f:read("*a")
        f:close()
    end

    local header = {
        ["Authorization"] = "Bearer "..self._token
    }

    local host_ip = os.getenv("HOST_IP")
    if not host_ip then
        host_ip = "127.0.0.1"
    end

    local resp = {}
    local params = {
        url = "https://" .. host_ip .. ":10250/pods",
        method = "GET",
        verify = "none",
        protocol = "any",
        headers = header,
        cafile = self._ca_path,
        sink = ltn12.sink.table(resp)
    }

    local worked, code, _, _ = https.request(params)
    if not worked then
        print("failed to query k8s api: ", code)
        return nil
    end

    return table.concat(resp)
end

function Ck8sApi:setupCons()
    local mnt = self._resYaml.config.proc_path
    local runtime = getRuntime(mnt)
    local cli = ChttpCli.new()
    local cons = {}
    local c = 0
    local content = cli:get("http://127.0.0.1:10255/pods")
    local obj = cli:jdecode(content.body)
    if not obj then
        local podsinfo = self:queryPodsInfo()
        if not podsinfo then
            print("Can't get pods info from kube api!")
            return nil
        end
        obj = json.decode(podsinfo)
    end

    for _, pod in ipairs(obj.items) do
        local metadata = pod.metadata
        if self._ns_blacklist[metadata.namespace] then
            goto continue
        end

		if not pod.status.qosClass then goto continue end
        local lpod = {
            name = metadata.name,
            namespace = metadata.namespace,
            uid = pystring:replace(metadata.uid, "-", "_"),
            qos = pystring:lower(pod.status.qosClass),
            volume = pod.spec.volumes
        }

		-- watch pod dirs(containers' changes in pod)
        local pod_path = podPath(lpod)
		local full_pod_path = mnt .. "sys/fs/cgroup/cpu" .. pod_path
		if unistd.access(full_pod_path) == 0 then
			self._ino:addPodWatch(full_pod_path)
		end

        local containerStatuses = pod.status.containerStatuses
        local containers = pod.spec.containers
        local containerResources = {}
        -- record container resources for pod_storage_stat
        for _, con in ipairs(containers) do
            containerResources[con.name] = con.resources
        end

        for _, con in ipairs(containerStatuses) do
            if not con.containerID then goto cs_continue end
            local cell = {
                pod = lpod,
                name = con.name,
                id = spiltConId(con.containerID),
                resources = containerResources[con.name]
            }

            cell.path = joinContainerPath(pod_path, cell, runtime)
            if unistd.access(mnt .. "sys/fs/cgroup/cpu/" .. cell.path) == 0 then
		cell.bvt = get_bvt(mnt .. "sys/fs/cgroup/cpu/" .. cell.path .. "/cpu.bvt_warp_ns")
                c = c + 1
                cons[c] = cell
            end
            ::cs_continue::
        end

        ::continue::
    end

    return cons
end

-- for now, just check if 10255 and 10250 ports has pods' info
function Ck8sApi:checkRuntime()
    local cli = ChttpCli.new()
    local content = cli:get("http://127.0.0.1:10255/pods")
    local obj = cli:jdecode(content.body)
    if not obj then 
        local podsinfo = self:queryPodsInfo()
        -- both 10250 and 10255 are unavaliable, k8s api not supported
        if not podsinfo then
            return 0
        end
    end
    print("Using kubernetes api.")
    return 1
end

function Ck8sApi:cgroupChanged()
    local is_change, events = self._ino:isChange()
    if is_change then
        print("k8s: cgroup changed.")
        if events ~= nil then
            -- reomeve inotify watch on deleted pod dirs
			self._ino:RemoveDeletePodWatch(events)
        end
    end
    return is_change
end

function Ck8sApi:initInotify()
    --[[
        in k8s, need to watch 
        "sys/fs/cgroup/cpu/kubepods.slice",
		"sys/fs/cgroup/cpu/kubepods.slice/kubepods-besteffort.slice",
		"sys/fs/cgroup/cpu/kubepods.slice/kubepods-burstable.slice"
    --]]
    self._ino = CinotifyPod.new()
	self._ino:watchKubePod(self._mnt)

end

function Ck8sApi:_init_(resYaml, mnt)
    self._resYaml = resYaml
    self._mnt = mnt
    self._token_path = default_token_path
    self._ca_path = default_ca_path
    self._token = ""
    self._ino = {}
    self._ns_blacklist = {}

    local ns_blacklist = resYaml.container.nsBlacklist
    for _, ns in ipairs(ns_blacklist) do
        self._ns_blacklist[ns] = 1
    end
end

return Ck8sApi

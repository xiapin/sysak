require("common.class")

local unistd = require("posix.unistd")
local Cinotifies = require("common.inotifies")
local docker_api = require("httplib.dockerApi")

local Cdocker = class("dockerApi")
local defualt_endpoint = "/var/run/docker.sock"
local docker = "/docker/"

function Cdocker:_init_(resYaml, mnt)
    self._name = "Docker"
    self._cgTopDir = mnt .. "sys/fs/cgroup/cpu"
    self._endpoint = defualt_endpoint
    self._mnt = mnt
    self._api = docker_api.new('localhost', defualt_endpoint)
    self._ino = {}
    self._cgroupDriver = "cgroupfs"
end

function Cdocker:getCgroupDriver()
    local qresp, err = self._api:get_system_info()
    if err or not qresp then
        print("Get docker cgroup driver failed!", err)
        return "cgroupfs" -- default is "cgroupfs"
    end

    if not qresp["body"] then
        return "cgroupfs"
    end

    return qresp["body"]["CgroupDriver"]
end

function Cdocker:initInotify()
    local cgroupDriver = self:getCgroupDriver()
    -- in docker, default watch /sys/fs/cgroup/cpu/docker (cgroupfs driver)
    local cg_path = self._cgTopDir .. docker
    if cgroupDriver == "systemd" then
        -- systemd cgroup driver, watch /sys/fs/cgroup/system.slice/docker
        cg_path = self._cgTopDir .. "/system.slice"
        self._cgroupDriver = "systemd"
    end
    self._ino = Cinotifies.new()
    self._ino:add(cg_path)
end

function Cdocker:checkRuntime()
    local qresp, err = self._api:get_version()
    if err then
        print("Get docker version failed!", err)
        return 0
    end
    if not qresp then
        return 0
    end
    print("Using docker runtime version:", qresp["body"].Version)
    return 1
end

function Cdocker:queryPodInfo()
    return self._api:list_containers()
end

function Cdocker:setupCons()
    local cons = {}
    local c = 0
    --local url = "http://127.0.0.1/containers/json"
    local obj, err = self:queryPodInfo()
    if err then
        print("Docker: query container info failed!", err)
        return nil
    end

    if not obj then
        return nil
    end

    local obj_body = obj["body"]
    if not obj_body then
        return nil
    end

    for _, container in ipairs(obj_body) do
        local path = ""
        local cg_path = ""
        local id = container.Id
        -- 返回的容器名会多一个"/"前缀（i.e /con_name)
        local name = string.sub(container.Names[1], 2)

        -- i.e /sys/fs/cgroup/cpu/docker/container_id/, path="docker/con_id"
        if self._cgroupDriver == "systemd" then
            path = "/system.slice" .. "/docker-" .. id .. ".scope"
            cg_path = self._cgTopDir .. path
        else
            path = docker .. id
            cg_path = self._cgTopDir .. path
        end

        local cell = {
            pod = nil,
            name = name,
            id = id,
            path =path
        }
        if unistd.access(cg_path) == 0 then
            c = c + 1
            cons[c] = cell
        end
    end

    return cons
end

function Cdocker:cgroupChanged()
    local is_change = self._ino:isChange()
    if is_change then
        print("Docker: cgroup changed.")
    end
    return is_change
end

return Cdocker
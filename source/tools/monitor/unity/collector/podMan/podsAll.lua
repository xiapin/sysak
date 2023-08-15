---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liaozhaoyan.
--- DateTime: 2023/3/15 6:00 PM
---

require("common.class")

local ChttpCli = require("httplib.httpCli")
local system = require("common.system")
local pystring = require("common.pystring")
local Cinotifies = require("common.inotifies")
local unistd = require("posix.unistd")
local json = require("cjson")

local CpodsAll = class("podsApi")

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

local function joinNPath(cell, runtime)
    -- "/sys/fs/cgroup/cpu/kubepods.slice/kubepods-${qos}.slice/kubepods-${qos}-pod${podid}.slice/${runtime}-${cid}.scope"
    local paths = {
        "/kubepods.slice/kubepods-",
        cell.pod.qos,
        ".slice/kubepods-",
        cell.pod.qos,
        "-pod",
        cell.pod.uid,
        ".slice/",
        runtime,
        "-",
        cell.id,
        ".scope"
    }
    return pystring:join("", paths)
end

local function joinGPath(cell, runtime)
    -- "/sys/fs/cgroup/cpu/kubepods.slice/kubepods-pod${podid}.slice/${runtime}-${cid}.scope"
    local paths = {
        "/kubepods.slice/kubepods-pod",
        cell.pod.uid,
        ".slice/",
        runtime,
        "-",
        cell.id,
        ".scope"
    }
    return pystring:join("", paths)
end

local function joinPath(cell, runtime)
    if cell.pod.qos == "guaranteed" then
        return joinGPath(cell, runtime)
    else
        return joinNPath(cell, runtime)
    end
end

local function setupCons(res)
    local mnt = res.config.proc_path
    local runtime = getRuntime(mnt)
    local cli = ChttpCli.new()
    local cons = {}
    local c = 0
    local blacklist = {["arms-prom"] = 1, ["kube-system"] = 1, ["kube-public"] = 1, ["kube-node-lease"] = 1}
    local content = cli:get("http://127.0.0.1:10255/pods")
    local obj = cli:jdecode(content.body)
    if not obj then 
        local cmd = ' curl -s -k -XGET https://127.0.0.1:10250/pods --cacert /var/run/secrets/kubernetes.io/serviceaccount/ca.crt --header "Authorization: Bearer $(cat /var/run/secrets/kubernetes.io/serviceaccount/token) "'
        local f = io.popen(cmd,"r")
        local podsinfo = f:read("*a")
        f:close()
        obj = json.decode(podsinfo)
    end

    for _, pod in ipairs(obj.items) do
        local metadata = pod.metadata
        if blacklist[metadata.namespace] then
            goto continue
        end
        local lpod = {name = metadata.name,
                      namespace = metadata.namespace,
                      uid = pystring:replace(metadata.uid, "-", "_"),
                      qos = pystring:lower(pod.status.qosClass),
        }

        local containerStatuses = pod.status.containerStatuses
        for _, con in ipairs(containerStatuses) do
            local cell = {
                pod = lpod,
                name = con.name,
                id = spiltConId(con.containerID)
            }
            cell.path = joinPath(cell, runtime)
            if unistd.access(mnt .. "sys/fs/cgroup/cpu/" .. cell.path) == 0 then
                c = c + 1
                cons[c] = cell
            end
        end
        ::continue::
    end

    return cons
end

function CpodsAll:getAllcons(procfs)
    local mnt = procfs
    local runtime = getRuntime(mnt)
    local cli = ChttpCli.new()
    local cons = {}
    local c = 0
    local content = cli:get("http://127.0.0.1:10255/pods")
    local obj = cli:jdecode(content.body)
    if not obj then 
        local cmd = ' curl -s -k -XGET https://127.0.0.1:10250/pods --cacert /var/run/secrets/kubernetes.io/serviceaccount/ca.crt --header "Authorization: Bearer $(cat /var/run/secrets/kubernetes.io/serviceaccount/token) "'
        local f = io.popen(cmd,"r")
        local podsinfo = f:read("*a")
        f:close()
        obj = json.decode(podsinfo) 
    end

    for _, pod in ipairs(obj.items) do
        local metadata = pod.metadata
        --print(string.format("podns :%s, pod:%s",metadata.namespace, metadata.name))
        local lpod = {name = metadata.name,
                      namespace = metadata.namespace,
                      uid = pystring:replace(metadata.uid, "-", "_"),
                      qos = pystring:lower(pod.status.qosClass),
        }
        local containerStatuses = pod.status.containerStatuses
        for _, con in ipairs(containerStatuses) do
            local cell = {
                pod = lpod,
                name = con.name,
                id = spiltConId(con.containerID)
            }
            cell.path = joinPath(cell, runtime)
            if unistd.access(mnt .. "/sys/fs/cgroup/memory/" .. cell.path) == 0 then
                c = c + 1
                cons[c] = cell
            end
        end
    end
    return cons
end

local function setupPlugins(res, proto, pffi, mnt, ino)
    local c = 0
    local cons = setupCons(res)
    local plugins = {}

    for _, con in ipairs(cons) do
        local ls = {
            {
                name = "pod",
                index = con.pod.name,
            },
            {
                name = "container",
                index = con.name.."-"..string.sub(con.id,0,4),
            },
            {
                name = "namespace",
                index = con.pod.namespace,
            },
        }

        for _, plugin in ipairs(res.container.luaPlugin) do
            local CProcs = require("collector.container." .. plugin)
            c = c + 1
            plugins[c] = CProcs.new(proto, pffi, mnt, con.path, ls)
            ino:add(plugins[c].pFile)
        end
    end

    return plugins
end

function CpodsAll:_init_(resYaml, proto, pffi, mnt)
    self._monDir = mnt .. "sys/fs/cgroup/"
    self._resYaml = resYaml
    self._proto = proto
    self._pffi = pffi
    self._mnt = mnt

    self._ino = Cinotifies.new()
    self._plugins = setupPlugins(self._resYaml, self._proto, self._pffi, self._mnt, self._ino)
    
    self._ino:add(mnt .. "sys/fs/cgroup/memory/kubepods.slice")
    self._ino:add(mnt .. "sys/fs/cgroup/memory/kubepods.slice/kubepods-besteffort.slice")
    self._ino:add(mnt .. "sys/fs/cgroup/memory/kubepods.slice/kubepods-burstable.slice")
 
    print( "pods plugin add " .. #self._plugins)
end

function CpodsAll:proc(elapsed, lines)
    local rec = {}
    if self._ino:isChange() or #self._plugins == 0 then
        print("cgroup changed.")
        self._ino = Cinotifies.new()
        self._plugins = setupPlugins(self._resYaml, self._proto, self._pffi, self._mnt, self._ino)
        self._ino:add(self._mnt .. "sys/fs/cgroup/memory/kubepods.slice")
        self._ino:add(self._mnt .. "sys/fs/cgroup/memory/kubepods.slice/kubepods-besteffort.slice")
        self._ino:add(self._mnt .. "sys/fs/cgroup/memory/kubepods.slice/kubepods-burstable.slice")
    end
    for i, plugin in ipairs(self._plugins) do
        --local res = plugin:proc(elapsed, lines)
        local stat, res = pcall(plugin.proc, plugin, elapsed, lines)
        if not stat or res == -1 then
            table.insert(rec, i)
        end
    end

    for _, i in ipairs(rec) do  -- del bad plugin
        self._plugins[i] = nil
    end
end

return CpodsAll

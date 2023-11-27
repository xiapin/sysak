require("common.class")
local json = require("cjson")
local https = require("ssl.https")
local ltn12 = require("ltn12")
local ChttpCli = require("httplib.httpCli")
local CkvProc = require("collector.kvProc")
local CvProc = require("collector.vproc")

local PodStorageStat = class("con_storage_stat", CkvProc)

local default_token_path = "/var/run/secrets/kubernetes.io/serviceaccount/token"
local default_ca_path = "/var/run/secrets/kubernetes.io/serviceaccount/ca.crt"
local summary_api = "/stats/summary"
local blacklist = {
    ["arms-prom"] = 1, ["kube-system"] = 1,
    ["kube-public"] = 1, ["kube-node-lease"] = 1
}

function PodStorageStat:_init_(proto, pffi, mnt)
    CkvProc._init_(self, proto, pffi, mnt, nil, "PodStorageStat")
    self._token = ""
    self.pods = {}
    self.cons = {}
end

-- get container's resoure info and pod's volume info
function PodStorageStat:setup(cons)
    -- reset the data struct first
    self.pods= {}
    self.cons = {}

    for _, con in ipairs(cons) do
        local pod_key = con.pod.name .. "-" .. con.pod.namespace
        self.cons[con.name] = con
        self.pods[pod_key] = con.pod
    end
end

-- query 10255/stats/summary to get storage stats
function PodStorageStat:querySummaryStats(token_path, ca_path)
    if self._token == "" then
        local f = io.open(token_path, "r")
        if not f then
            print("service token not exist!")
            return nil
        end

        self._token = f:read("*a")
        f:close()
    end

    local header = {
        ["Authorization"] = "Bearer ".. self._token
    }

    local host_ip = os.getenv("HOST_IP")
    if not host_ip then
        host_ip = "127.0.0.1"
    end

    local node_name = os.getenv("NODE_NAME")
    if not node_name then
        print("NODE_NAME not exist!")
        return nil
    end

    local url = "https://"..host_ip..":10250" .. summary_api

    local resp = {}
    local params = {
        url = url,
        method = "GET",
        verify = "none",
        protocol = "any",
        headers = header,
        cafile = ca_path,
        sink = ltn12.sink.table(resp)
    }

    local worked, code, _, _ = https.request(params)
    if not worked then
        print("failed to query summary api: ", code)
        return nil
    end

    return table.concat(resp)
end

local function appendMetric(self, table, con, pod, podns, protoType)
    local label = {
        {name="pod", index=pod},
        {name="nanmespace", index=podns},
        {name="container", index=con},
    }

    -- container empheral storage: rootfs and log
    if protoType == "rootfs" or protoType == "logs" then
        label[#label+1] = {name="storage", index=protoType}
        protoType = "container_ephemeral_storage"
    end

    if protoType == "pod_volume" and table.name then
        label[#label+1] = {name="Volume", index=table.name}
    end

    for type, value in pairs(table) do
        if type == "time" or type == "device" or type == "name" then
            goto continue
        end

        local cell = {
            {name=type, value=value},
        }
        self:appendLine(self:_packProto(protoType, label, cell))
        ::continue::
    end
end

local function convertToBytes(limit)
    local suffixes = {
        ["E"] = 10^18,
        ["P"] = 10^15,
        ["T"] = 10^12,
        ["G"] = 10^9,
        ["M"] = 10^6,
        ["k"] = 10^3,
        ["Ei"] = 2^60,
        ["Pi"] = 2^50,
        ["Ti"] = 2^40,
        ["Gi"] = 2^30,
        ["Mi"] = 2^20,
        ["Ki"] = 2^10
    }

    local value, suffix = limit:match("(%d+%.?%d*)(%a+)")
    value = tonumber(value)

    if suffixes[suffix] then
        return value * suffixes[suffix]
    elseif suffixes[suffix:upper()] then
        return value * suffixes[suffix:upper()]
    else
        return nil
    end
end

local function searchKey(table, key)
    if type(table) ~= "table" then return nil end

    if table[key] ~= nil then
        return table[key]
    end

    for k, v in pairs(table) do
        if type(v) ~= "table" then goto continue end
        local result = searchKey(v, key)
        if result ~= nil then
            return result
        end
        ::continue::
    end

    return nil
end

function PodStorageStat:packContainerLimit(con_name, pod_name, pod_ns)
    local con = self.cons[con_name]
    if not con or not con.resources or
        not con.resources.limits then return end

    local limits = con.resources.limits
    local es_limit = limits["ephemeral-storage"]
    if not es_limit then return end

    local labels = {
        {name="pod", index=pod_name},
        {name="nanmespace", index=pod_ns},
        {name="container", index=con_name},
        {name="storage", index=""}
    }

    local limit_bytes = convertToBytes(es_limit)
    local cell = {
        {name="limit", value=limit_bytes}
    }
    self:appendLine(self:_packProto("container_ephemeral_storage",
            labels, cell))
end

function PodStorageStat:packVolumeLimit(volume_name, pod_name, pod_ns)
    local pod = self.pods[pod_name.."-"..pod_ns]
    if not pod or not pod.volume then return end

    for _, vol in ipairs(pod.volume) do
        if vol.name == volume_name then
            local labels = {
                {name="pod", index=pod_name},
                {name="nanmespace", index=pod_ns},
                {name="container", index=""},
                {name="Volume", index=volume_name}
            }

            local volume_limit = searchKey(vol, "sizeLimit")
            if volume_limit == nil then return end

            local limit_bytes = convertToBytes(volume_limit)
            local cell = {
                {name="limit", value=limit_bytes}
            }
            self:appendLine(self:_packProto("pod_volume",
                    labels, cell))
            break
        end
    end
end

function PodStorageStat:proc(elapsed, lines)
    CvProc.proc(self)

    local statres = self:querySummaryStats(default_token_path, default_ca_path)
    if not statres then
        print("Failed to get summary stats from summary api!")
        return
    end

    local statobj = json.decode(statres)
    for _, pod in ipairs(statobj.pods) do
        local podname = pod.podRef.name
        local podns = pod.podRef.namespace
        -- skip blacklist namespace
        if blacklist[podns] then
            goto continue
        end

        for _, con in ipairs(pod.containers) do
            appendMetric(self, con.rootfs, con.name,
                podname, podns, "rootfs")
            appendMetric(self, con.logs, con.name,
                podname, podns, "logs")
            self:packContainerLimit(con.name, podname, podns)
        end

        appendMetric(self, pod["ephemeral-storage"], "",
            podname, podns, "pod_ephemeral_storage")

        --[[ dont't collect pod volume info for now
        for _, vol in ipairs(pod.volume) do
            appendMetric(self, vol, "",
                podname, podns, "pod_volume")
            self:packVolumeLimit(vol.name, podname, podns)
        end
        --]]
        ::continue::
    end

    self:push(lines)
end

return PodStorageStat
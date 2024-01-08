---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by wrp.
--- DateTime: 2023/7/10 10:46
---

local system = require("common.system")
local pystring = require("common.pystring")
require("common.class")

local CtransPro = class("CtransPro")

local function qFormDataDis(from,tData)
    local res = {}
    local len = #tData
    local c = 0
    for i = len, 1, -1 do
        local line = tData[i]
        if from == line.title then
            c = c + 1
            res[c] = line
        end
    end
    return res
end

local function qFormData(from, tData)
    local res = {}
    local len = #tData
    local last = 0
    local c = 0
    for i = len, 1, -1 do
        local line = tData[i]
        if from == line.title then
            if last == 0 or last == line.time then
                c = c + 1
                res[c] = line
                last = line.time
            else
                break
            end
        end
    end
    return res
end

local function packLine_us(title, ls, v, time)
    local tLs = {}
    for k, v in pairs(ls) do
        table.insert(tLs, string.format("%s=\"%s\"", k , v))
    end
    local label = ""
    if #tLs then
        label = pystring:join(",", tLs)
        label = "{" .. label .. "}"
    end
    return string.format("%s%s %.1f %d", title, label, v, time/1000)
end

local function packLine(title, ls, v, time)
    local tLs = {}
    local c = 0
    for k, v in pairs(ls) do
        c = c + 1
        tLs[c] = string.format("%s=\"%s\"", k , v)
    end
    local label = ""
    if #tLs then
        label = pystring:join(",", tLs)
        label = "{" .. label .. "}"
    end
    return string.format("%s%s %.1f", title, label, v)
end

function CtransPro:_init_(instance, fYaml, help, timestamps)
    self._instance = instance
    self._help = help or false
    self._cluster_id = nil
    local ms = system:parseYaml(fYaml)
    self._timestamps = timestamps or false
    if self._timestamps == true then
        self.pack_line = packLine_us
    else
        self.pack_line = packLine
    end
    self._tDescr = ms.metrics

    if ms.container then
        if ms.container.cluster_id == true then
            local cluster_id = os.getenv("CLUSTER_ID")
            if cluster_id then
                self._cluster_id = cluster_id
            end
        end
    end

end

local function checkLine(blacklist, whitelist, labels)
    if blacklist then
        for k, v in pairs(blacklist) do
            if labels[k] then
                if type(v) == "table" then
                    for _, vv in ipairs(v) do
                        if labels[k]:match(vv) then
                            return false
                        end
                    end
                else
                    if labels[k]:match(v) then
                        return false
                    end
                end
            end
        end
        return true
    elseif whitelist then
        for k, v in pairs(whitelist) do
            if labels[k] then
                if type(v) == "table" then
                    for _, vv in ipairs(v) do
                        if labels[k]:match(vv) then
                            return true
                        end
                    end
                else
                    if labels[k]:match(v) then
                        return true
                    end
                end
            end
        end
        return false
    end
    return true
end

function CtransPro:export(datas)
    local res = {}
    local c = 0
    for _, line in ipairs(self._tDescr) do
        local from = line.from -- cpu_total
        local tFroms
        if line.discrete then
            tFroms = qFormDataDis(from, datas)
        else
            tFroms = qFormData(from, datas)
        end
        if #tFroms~=0 then
            local title = line.title --sysak_proc_cpu_total
            if self._help then
                local help = string.format("# HELP %s %s", title, line.help)
                c = c + 1
                res[c] = help
                local sType = string.format("# TYPE %s %s", title, line.type)
                c = c + 1
                res[c] = sType
            end

            local blacklist = line.blacklist
            local whitelist = line.whitelist
            if blacklist and whitelist then
                print("cannot set both blacklist and whitelist! ")
                goto continue
            end
            for _, tFrom in ipairs(tFroms) do
                if tFrom.values then
                    local labels = system:deepcopy(tFrom.labels)
                    if not labels then
                        labels = {}
                    end
                    labels.instance = self._instance
                    if self._cluster_id then
                        labels.cluster = self._cluster_id
                    end
                    for k, v in pairs(tFrom.values) do
                        labels[line.head] = k
                        if checkLine(blacklist, whitelist, labels)==true then

                            c = c + 1
                            res[c] = self.pack_line(title, labels, v, 1)
                        end

                    end

                end
            end
            ::continue::
        end
    end
    local lines = pystring:join("\n", res)
    return lines
end

function CtransPro:toMetric(datas)
    local res = {}
    local c = 0
    local instance = self._instance
    for _, line in ipairs(self._tDescr) do
        local from = line.from -- cpu_total
        local tFroms
        if line.discrete then
            tFroms = qFormDataDis(from, datas)
        else
            tFroms = qFormData(from, datas)
        end
        if #tFroms ~= 0 then
            local title = line.title --sysak_proc_cpu_total

            local blacklist = line.blacklist
            local whitelist = line.whitelist
            if blacklist and whitelist then
                print("cannot set both blacklist and whitelist! ")
                goto continue
            end
            for _, tFrom in ipairs(tFroms) do
                if tFrom.values then
                    local labels = system:deepcopy(tFrom.labels)
                    if not labels then
                        labels = {}
                    end
                    labels.instance = instance
                    for k, v in pairs(tFrom.values) do
                        local labels_u = system:deepcopy(labels)
                        labels_u[line.head] = k
                        if checkLine(blacklist, whitelist, labels_u) then
                            c = c + 1
                            res[c] = {title, labels_u, v}
                        end
                    end

                end
            end
            ::continue::
        end
    end
    return res
end

return CtransPro
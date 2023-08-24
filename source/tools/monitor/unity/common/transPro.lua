---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by wrp.
--- DateTime: 2023/7/10 10:46
---

local system = require("common.system")
local pystring = require("common.pystring")
require("common.class")

local CtransPro = class("CtransPro")

local function qFormData(from, tData)
    local res = {}
    local len = #tData
    local last = 0
    local c = 0
    for i = len, 1, -1 do
        local line = tData[i]
        print(line.title)
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
    local ms = system:parseYaml(fYaml)
    self._timestamps = timestamps or false
    if self._timestamps == true then
        self.pack_line = packLine_us
    else
        self.pack_line = packLine
    end
    self._tDescr = ms.metrics
    print(self._tDescr)
end

function CtransPro:export(datas)
    local res = {}
    local c = 0
    for _, line in ipairs(self._tDescr) do
        --local from = line.from -- cpu_total
        --local tFroms = qFormData(from, datas)
        if #datas then
            local title = line.title --sysak_proc_cpu_total
            if self._help then
                local help = string.format("# HELP %s %s", title, line.help)
                c = c + 1
                res[c] = help
                local sType = string.format("# TYPE %s %s", title, line.type)
                c = c + 1
                res[c] = sType
            end

            for _, tFrom in ipairs(datas) do
                if tFrom.vs then
                    local labels = system:deepcopy(tFrom.labels)
                    if not labels then
                        labels = {}
                    end
                    labels.instance = self._instance
                    for k, v in pairs(tFrom.vs) do
                        labels[line.head] = v.name
                        c = c + 1
                        res[c] = self.pack_line(title, labels, v.value, 1)
                    end
                end

            end
        end
    end
    --c = c + 1
    --res[c] = ""
    local lines = pystring:join("\n", res)
    --print(lines)
    return lines
end

return CtransPro
---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liaozhaoyan.
--- DateTime: 2023/2/9 11:24 PM
---

local cjson = require("cjson.safe")
local pystring = require("common.pystring")
local system = require("common.system")
local module = {}
local json = cjson.new()

json.encode_escape_forward_slash(false)

local function parseLabel(sls, ls)
    local lss = pystring:split(sls, ",")
    for _, cell in ipairs(lss) do
        local kv = pystring:split(cell, "=", 1)
        ls[kv[1]] = kv[2]
    end
end

local function getNextQuote(vStr)
    local quote = string.byte('"')
    local slash = string.byte('\\')
    for idx = 2, #vStr do
        if string.byte(vStr, idx) == quote then
            if string.byte(vStr, idx - 1) ~= slash then
                return idx
            end
        end
    end
    return -1
end

local function parseValue(svs, vs, log)
    local flag = true
    local vStr = svs
    while flag do
        local kvs = pystring:split(vStr, "=", 1)
        local k = kvs[1]
        vStr = kvs[2]
        local quote = string.byte('"')
        local dot = string.byte(',')
        if string.byte(vStr, 1) == quote then  -- '"'
            local idx = getNextQuote(vStr)
            assert(idx > 0, "bad string, " .. vStr)

            idx = idx
            local s = string.sub(vStr, 1, idx)
            log[k] = json.decode(s)
            if string.byte(vStr, idx + 1) == dot then
                vStr = string.sub(vStr, idx + 2)
            else
                flag = false
            end
        else   -- number
            local vss = pystring:split(vStr, ",", 1)
            local v = vss[1]
            vStr = vss[2]
            vs[k] = tonumber(v)
            if not vStr then
                flag = false
            end
        end
    end
end

function module.parse(line)
    local hvs = pystring:split(line, " ", 1)
    local heads, svs = hvs[1], hvs[2]
    local ths = pystring:split(heads, ",", 1)
    local title, ls, vs, log = ths[1], {}, {}, {}
    if #ths > 1 then
        local sls = ths[2]
        parseLabel(sls, ls)
    end
    parseValue(svs, vs, log)
    return title, ls, vs, log
end

function module.pack(title, ls, vs)
    local line = title
    if ls and system:keyCount(ls) > 0 then
        local lss = {}
        local c = 0
        for k, v in pairs(ls) do
            c = c + 1
            v = string.gsub(v, "%s", "_")
            lss[c] = table.concat({k, v}, "=")
        end
        line = line .. ',' .. pystring:join(",", lss)
    end
    local vss = {}
    local c = 0
    for k, v in pairs(vs) do
        local tStr = type(v)
        if tStr == "number" then
            c = c + 1
            vss[c] = table.concat({k, tostring(v)}, "=")
        elseif tStr == "string" then
            c = c + 1
            vss[c] = table.concat({k, json.encode(v)}, "=")
        else
            system:dumps(tStr)
            error("bad value type for " .. tStr)
        end
    end
    line = line .. ' ' .. pystring:join(",", vss)
    return line
end

function module.packMetric(line)
    local cells
    local title = line.line
    local ls = {}
    local vs = {}

    if line.vs then
        cells = line.ls
        if cells then
            for _, cell in ipairs(cells) do
                ls[cell.name] = cell.index
            end
        end

        cells = line.vs
        for _, cell in ipairs(cells) do
            vs[cell.name] = cell.value
        end

        return module.pack(title, ls, vs)
    end
end

function module.packLog(line)
    local cells
    local title = line.line
    local ls = {}
    local vs = {}

    if line.log then
        cells = line.ls
        if cells then
            for _, cell in ipairs(cells) do
                ls[cell.name] = cell.index
            end
        end

        cells = line.log
        for _, cell in ipairs(cells) do
            vs[cell.name] = cell.log
        end

        return module.pack(title, ls, vs)
    end
end

function module.packs(line)
    local cells
    local title = line.line
    local ls = {}
    local vs = {}

    cells = line.ls
    if cells then
        for _, cell in ipairs(cells) do
            ls[cell.name] = cell.index
        end
    end

    cells = line.vs
    if cells then
        for _, cell in ipairs(cells) do
            vs[cell.name] = cell.value
        end
    end

    cells = line.log
    if cells then
        for _, cell in ipairs(cells) do
            vs[cell.name] = cell.log
        end
    end

    return module.pack(title, ls, vs)
end

return module

---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liaozhaoyan.
--- DateTime: 2023/1/12 3:28 PM
---

require("common.class")

local CprotoData = require("common.protoData")
local CprotoQueue = class("loop")
local cjson = require("cjson.safe")
local json = cjson.new()
local system = require("common.system")

function CprotoQueue:_init_(que)
    self._proto = CprotoData.new(que)
    self._ffi = require("collector.native.plugincffi")
    self._que = que
end

function CprotoQueue:que()
    return self._que
end

function CprotoQueue:load_label(unity_line, line)
    local c = #line.ls
    for i=0, 4 - 1 do
        local name = self._ffi.string(unity_line.indexs[i].name)
        local index = self._ffi.string(unity_line.indexs[i].index)

        if #name > 0 then
            c = c + 1
            line.ls[c] = {name = name, index = index}
        else
            return
        end
    end
end

function CprotoQueue:load_value(unity_line, line)
    local c = #line.vs
    for i=0, 32 - 1 do
        local name = self._ffi.string(unity_line.values[i].name)
        local value = unity_line.values[i].value

        if #name > 0 then
            c = c + 1
            line.vs[c] = {name = name, value = value}
        else
            return
        end
    end
end

function CprotoQueue:load_log(unity_line, line)
    local name = self._ffi.string(unity_line.logs[0].name)
    if #name > 0 then
        local log = self._ffi.string(unity_line.logs[0].log)
        self._ffi.C.free(unity_line.logs[0].log)   -- should free from strdup
        table.insert(line.log, {name = name, log = log})
    end
end

function CprotoQueue:_proc(unity_lines, lines)
    local c = #lines["lines"]
    for i = 0, unity_lines.num - 1 do
        local unity_line = unity_lines.line[i]
        local line = {line = self._ffi.string(unity_line.table),
                      ls = {},
                      vs = {},
                      log = {}}

        self:load_label(unity_line, line)
        self:load_value(unity_line, line)
        self:load_log(unity_line, line)
        c = c + 1
        lines["lines"][c] = line
    end
end

function CprotoQueue:send(num, pline)
    local unity_lines = self._ffi.new("struct unity_lines")
    local lines = self._proto:protoTable()
    unity_lines.num = num
    unity_lines.line = pline
    self:_proc(unity_lines, lines)
    self._ffi.C.free(unity_lines.line)
    local stream = self._proto:encode(lines)
    return collector_qout(self._que, stream, #stream)
end

return CprotoQueue
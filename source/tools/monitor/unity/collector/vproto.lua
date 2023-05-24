---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liaozhaoyan.
--- DateTime: 2023/3/25 5:28 PM
---

require("common.class")
local system = require("common.system")

local CvProto = class("collector.vproto")

function CvProto:_init_(proto)
    self._proto = proto
end

function CvProto:proc(elapsed)
    self._lines = self._proto:protoTable()
end

function CvProto:appendLine(line)
    --assert(not self._lines, "your class should call CvProto.proc at first.")
    table.insert(self._lines["lines"], line)
end

function CvProto:copyLine(line)
    self:appendLine(system:deepcopy(line))
end

function CvProto:push(lines)
    local c = #lines["lines"]   -- not for #lines
    for _, line in ipairs(self._lines["lines"]) do
        c = c + 1
        lines["lines"][c] = line
    end
    self._lines = nil
end

function CvProto:_packProto(head, labels, vs, log)
    return {line = head, ls = labels, vs = vs, log = log}
end

function CvProto:packLog(title, name, msg)
    local logList = {}

    logList[1] = {
        name = name,
        log = msg
    }
    self:appendLine(self:_packProto(title, nil, nil, logList))
end

return CvProto

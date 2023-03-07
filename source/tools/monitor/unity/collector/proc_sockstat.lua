---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liaozhaoyan.
--- DateTime: 2023/1/17 12:27 AM
---

require("common.class")
local pystring = require("common.pystring")
local CvProc = require("collector.vproc")

local CprocSockStat = class("procsockstat", CvProc)

function CprocSockStat:_init_(proto, pffi, mnt, pFile)
    CvProc._init_(self, proto, pffi, mnt, pFile or "proc/net/sockstat")
end

function CprocSockStat:proc(elapsed, lines)
    CvProc.proc(self)
    local vs = {}
    local c = 0
    for line in io.lines(self.pFile) do
        local cells = pystring:split(line, ":", 1)
        if #cells > 1 then
            local head, body = cells[1], cells[2]
            head = string.lower(head)
            body = pystring:lstrip(body, " ")
            local bodies = pystring:split(body, " ")
            local len = #bodies / 2
            for i = 1, len do
                local title = string.format("%s_%s", head, bodies[2 * i - 1])
                local v = {
                    name = title,
                    value = tonumber(bodies[2 * i])
                }
                c = c + 1
                vs[c] = v
            end
        end
    end
    self:appendLine(self:_packProto("sock_stat", nil, vs))
    self:push(lines)
end

return CprocSockStat

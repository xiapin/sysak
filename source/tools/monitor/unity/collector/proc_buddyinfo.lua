---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liuxinwnei.
--- DateTime: 2023/02/08 17:00 PM
---

require("common.class")
local CkvProc = require("collector.kvProc")
local CvProc = require("collector.vproc")
local pystring = require("common.pystring")

local CprocBuddyinfo = class("proc_buddyinfo", CkvProc)

function CprocBuddyinfo:_init_(proto, pffi, mnt,pFile)
    CkvProc._init_(self, proto, pffi, mnt,  pFile or "proc/buddyinfo", "buddyinfo")
end

function CprocBuddyinfo:proc(elapsed, lines)
    CvProc.proc(self)
    local buddyinfo = {}
    for line in io.lines(self.pFile) do
        if string.find(line,"Normal") then
            local subline = pystring:split(line,"Normal",1)[2]
            for num in string.gmatch(subline, "%d+") do
               table.insert(buddyinfo,tonumber(num))
            end
            break
        end
    end

    if not buddyinfo then
        for line in io.lines(self.pFile) do
            if string.find(line,"DMA32") then
                local subline = pystring:split(line,"DMA32",1)[2]
                for num in string.gmatch(subline, "%d+") do
                   table.insert(buddyinfo,tonumber(num))
                end
                break
            end
        end
    end

    for k,v in pairs(buddyinfo) do
        local cell = {name="buddyinfo" .. (k-1), value=v}
        table.insert(self._protoTable["vs"], cell)
    end

    self:appendLine(self._protoTable)
    return self:push(lines)
end

return CprocBuddyinfo
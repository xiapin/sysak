---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liaozhaoyan.
--- DateTime: 2023/4/12 15:51
---

require("common.class")
local Cfifo = require("common.fifo")

local CdiskFifo = class("diskFifo", Cfifo)

function CdiskFifo:_init_(maxLen)
    Cfifo._init_(self, maxLen)
end

function CdiskFifo:iowait()
    local sum = 0
    local value
    local cells = {}
    local count = self._count

    if #self.list < count then
        return
    end

    for i, v in ipairs(self.list) do
        value = v.iowait
        cells[i], sum = value, sum + value
    end
    return {max = math.max(unpack(cells)), min = math.min(unpack(cells)), count = count, avg = sum / count}
end

function CdiskFifo:values(disk, key)
    local c = 0
    local sum = 0
    local value
    local cells = {}
    local count = self._count

    if #self.list < count then
        return
    end

    local d
    for _, v in ipairs(self.list) do
        d = v[disk]
        if d then
            value = d[key]
            c = c + 1
            cells[c], sum = value, sum + value
        else   -- not full
            return
        end
    end
    return {max = math.max(unpack(cells)), min = math.min(unpack(cells)), count = count, avg = sum / count}
end

return CdiskFifo

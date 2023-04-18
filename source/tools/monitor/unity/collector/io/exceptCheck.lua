---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liaozhaoyan.
--- DateTime: 2023/4/12 16:26
---

require("common.class")
local system = require("common.system")
local CdiskFifo = require("collector.io.diskFifo")
local CexecptCheck = class("CexecptCheck")

-- [[  v format
--    iowait: 1
--    disk_a: {
--        iops   = 1,
--        bps    = 2,
--        qusize = 3,
--        await  = 4,
--        util   = 5,
--    }
--  ]]

local fifoSize = 60
local keys = {"iops", "bps", "qusize", "await", "util"}

local function addItem()
    return {
        baseThresh = {
            nrSample = 0,
            curWinMinVal = 1e9,
            curWinMaxVal = 0,
            moveAvg = 0,
            thresh = 0
        },
        compensation = {
            thresh = 0,
            shouldUpdThreshComp = true,
            decRangeThreshAvg = 0,
            decRangeCnt = 0,
            minStableThresh = 1e9,
            maxStableThresh = 0,
            stableThreshAvg = 0,
            nrStableThreshSample = 0,
        },
        dynTresh = 1e9,
        usedWin = 0,
    }
end

local function addItems()
    local ret = {}
    for _, key in ipairs(keys) do
        ret[key] = addItem()
    end
    return ret
end

function CexecptCheck:_init_()
    self._fifo = CdiskFifo.new(fifoSize)
    self._waitItem = addItem()
    self._diskItem = {}
end

local function calc(item, vs)
    local bt = item.baseThresh
    bt.curWinMinVal = vs.min
    bt.curWinMaxVal = vs.max
    bt.moveAvg      = vs.arg
    bt.nrSample     = vs.count
end

function CexecptCheck:calcs()
    local iowaits = self._fifo:iowait()
    if iowaits then
        calc(self._waitItem, iowaits)
    end

    local vs
    for disk, item in pairs(self._diskItem) do
        for _, key in ipairs(keys) do
            vs = self._fifo:values(disk, key)
            if vs then
                calc(item[key], vs)
            end
        end
    end
    system:dumps(self._diskItem)
end

function CexecptCheck:addValue(v)
    for disk, _ in pairs(v) do  -- new  iowait names one disk
        if disk ~= "iowait" then
            if not system:keyIsIn(self._diskItem, disk) then
                self._diskItem[disk] = addItems()
            end
        end
    end
    for disk, _ in pairs(self._diskItem) do    -- del
        if not system:keyIsIn(v, disk) then
            self._diskItem[disk] = nil
        end
    end
    self._fifo:push(v)
    self:calcs()
end

return CexecptCheck

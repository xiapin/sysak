---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liaozhaoyan.
--- DateTime: 2023/3/20 5:16 PM
---

require("common.class")
local CcollectorStat = require("collector.guard.collector_stat")
local system = require("common.system")

local CguardSched = class("guardSched")

function CguardSched:_init_(tid, procs, names, jperiod)
    self._stat = CcollectorStat.new(tid)
    self._jperiod = jperiod
    self._procs = procs
    self._names = names
    self._limit = 1e5
end

function CguardSched:proc(t, lines)
    local toRemove = {}

    local start = lua_local_clock()  -- unit us
    local stop = 0
    local j1 = self._stat:jiffies()

    for i, obj in ipairs(self._procs) do
        obj:proc(t, lines)
        stop = lua_local_clock()
        if stop - start >= self._limit then   --
            local j2 = self._stat:jiffies()
            if j2 - j1 >= self._limit / 1e6 * self._jperiod * 3 / 4 then  -- 3/4 time used bye plugin
                table.insert(toRemove, i)
            end
        end
        start = stop
    end

    if #toRemove > 0 then
        system:reverseTable(toRemove)  -- list should reverse at first.
        for _, i in ipairs(toRemove) do
            print("remove " .. self._names[i])
            table.remove(self._procs, i)
            table.remove(self._names, i)
        end
    end
end

return CguardSched

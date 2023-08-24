-----
----- Generated by EmmyLua(https://github.com/EmmyLua)
----- Created by wrp.
----- DateTime: 2023/7/24 16:01
-----
--
require("common.class")
local CcollectorStat = require("collector.guard.collector_stat")
local system = require("common.system")
local pystring = require("common.pystring")
local CguardSched = require("collector.guard.guardSched")
local obHelper = require("collector.observe.obHelper")
local CprotoData = require("common.protoData")

local CguardObserve = class("guardObserve")

function CguardObserve:_init_(tid, jperiod, resYaml, que, proc_path, procffi)
    self._proto = CprotoData.new(que)
    self._proc_path = proc_path
    self._procffi = procffi

    self._stat = CcollectorStat.new(tid)
    self._jperiod = jperiod

    self._limit = resYaml.config.limit.cellLimit
    if self._limit == nil then
        self._limit = 1e4 * 5 -- 50ms
    elseif self._limit ~= -1 then
        self._limit = self._limit * 1e3
    end

    self._resYaml = resYaml
    if resYaml.observe then
        self._hasOb = true
        self._obperiod = resYaml.observe.period
        self._timerstart = -self._obperiod
    else
        self._hasOb = false
    end

    self._pids = {}
    self._obs = {}

end

function CguardObserve:getPids()
    local pids = {}

    local comms = self._resYaml.observe.comms
    for  commk, commv in pairs(comms) do
        local res = obHelper:getPidByComm(commk)
        for _, pid in ipairs(res) do
            table.insert(pids,pid)

        end
    end
    return pids
end

function CguardObserve:proc(t,lines)
    if self._hasOb then
        local now = os.time()
        if self._timerstart + self._obperiod < now then
            self._pids = self:getPids()
            self._obs = {}

            local cnt = 1
            local CobProcess = require("collector.observe.obProcess" )
            for _, pid in ipairs(self._pids) do
                local comm
                local fpid = io.open("/proc/"..pid.."/comm")
                if fpid == nil then
                    goto continue
                end
                for line in fpid:lines() do
                    comm = line
                end
                fpid:close()

                local cgroup = ""
                local conf = self._resYaml.observe.comms[comm]
                local confs = pystring:split(conf," ")
                for _, c in ipairs(confs) do
                    if c == "cgroup" then
                        cgroup = obHelper:getCgroupSystemd(pid)
                    end
                end
                local labels ={
                    pid = pid,
                    comm = comm,
                    cgroup = cgroup
                }

                self._obs[cnt] = CobProcess.new(self._jperiod, labels, self._proto, self._procffi, self._proc_path)

                cnt = cnt + 1
                ::continue::
            end

            print("observe pids reset")
            self._timerstart = now
        end

        local toRemove = {}

        local start = lua_local_clock()  -- unit us
        local stop = 0
        local j1 = self._stat:jiffies()
        for i, obj in ipairs(self._obs) do
            if i % 100 == 0 then -- need to update jiffies
                j1 = self._stat:jiffies()
            end
            local ret, overTime = obj:proc(t, lines)
            if ret == -1 then
                table.insert(toRemove, i)
            else
                stop = lua_local_clock()
                if ret ~= 1 then
                    overTime = 0
                end
                if self._limit ~= -1 then
                    if stop - start - overTime >= self._limit then   --
                        local j2 = self._stat:jiffies()
                        if j2 - j1 >= self._limit / 1e6 * self._jperiod * 3 / 4 then  -- 3/4 time used by plugin
                            table.insert(toRemove, i)
                        end
                        j1 = j2
                    end
                end

            end
            start = stop

            ::continue::
        end

        if #toRemove > 0 then
            system:reverseTable(toRemove)  -- list should reverse at first.
            for _, i in ipairs(toRemove) do

                table.remove(self._obs, i)

            end
        end
    end

end

return CguardObserve
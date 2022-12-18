---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liaozhaoyan.
--- DateTime: 2022/12/16 10:27 PM
---

require("class")
local pystring = require("pystring")
local CvProc = require("vproc")

local CprocStat = class("procstat", CvProc)

function CprocStat:_init_(proto, pffi, pFile)
    CvProc._init_(self, proto, pffi, pFile or "/proc/stat")
    self._funs = self:setupTable()
    self._cpuArr = {}
end

function CprocStat:setupTable()
    return {
        ctxt = function(s) self:ctxt(s)  end,
        btime = function(s) self:btime(s)  end,
        processes = function(s) self:processes(s)  end,
        procs_running = function(s) self:procs_running(s)  end,
    }
end

function CprocStat:_cpuHead()
    return {"user", "nice", "sys", "idle", "iowait",
            "hardirq", "softirq", "steal", "guest", "guestnice"}
end

function CprocStat:_procCpu(now, last)
    if last then
        local vs = {}
        local sum = 0
        local index = self:_cpuHead()
        for i = 1, #index do
            local delta = now.value[i - 1] - last.value[i - 1]
            table.insert(vs, delta)
            sum = sum + delta
        end

        if sum > 0 then
            local res = {}
            local total = tonumber(sum)
            for i = 1, #vs do
                local v = tonumber(vs[i])
                local cell = {name=index[i], value=tonumber(v / total)}
                table.insert(res, cell)
            end
            return res
        end
    end
end

function CprocStat:_pCpuTotal(data)
    local res = self:_procCpu(data, self._lastCpuTotal)
    self._lastCpuTotal = data

    if res then
        self:appendLine(self:_packProto("cpu_total", nil, res))
    end
end

function CprocStat:_pperCpu(data, vcpu)
    local res = self:_procCpu(data, self._cpuArr[vcpu])
    self._cpuArr[vcpu] = data

    if res then
        local label = {name = "cpu_name", index = "cpu" .. vcpu}
        self:appendLine( self:_packProto("cpus", label, res))
    end
end

function CprocStat:_fcpu(line)
    local s = string.sub(line, 4)
    local ch = string.byte(s, 1)

    if ch == 32 then -- blank is 32
        local data = self._ffi.new("var_long_t")
        assert(self._cffi.var_input_long(self._ffi.string(s), data) == 0)
        assert(data.no == 10)
        self:_pCpuTotal(data)
    else
        local data = self._ffi.new("var_kvs_t")
        assert(self._cffi.var_input_kvs(self._ffi.string(s), data) == 0)
        assert(data.no == 10)
        local sNcpu = self._ffi.string(data.s)
        local Ncpu = tonumber(sNcpu)
        self:_pperCpu(data, Ncpu)
    end
end

function CprocStat:_sirqHead()
    return {"hi", "timer", "nettx", "netrx", "bsirq", "iopoll",
            "tasklet", "sched", "hrtimer", "rcu", "nr"}
end

function CprocStat:_procSirq(now, last)
    if last then
        local res = {}
        local index = self:_sirqHead()
        for i = 1, #index do
            local delta = now.value[i - 1] - last.value[i - 1]
            local cell = {name=index[i], value=tonumber(delta)}
            table.insert(res, cell)
        end
        return res
    end
end

function CprocStat:_fsirq(line)
    local s = string.sub(line, 8)

    local data = self._ffi.new("var_long_t")
    assert(self._cffi.var_input_long(self._ffi.string(s), data) == 0)
    assert(data.no == 11)

    local res = self:_procSirq(data, self._lastSirq)
    self._lastSirq = data

    if res then
        self:appendLine(
                self:_packProto("sirq", nil, res)
        )
    end
end

function CprocStat:_packCounter(head, value)
    return {
        line = "stat_counters",
        ls = nil,
        vs = {
            name = head,
            value = value
        }
    }
end

function CprocStat:ctxt(line)
    local v = tonumber(line)
    if self._lastCtxt then
        local res = v - self._lastCtxt
        self:appendLine(self:_packCounter("ctxt", res))
    end
    self._lastCtxt = v
end

function CprocStat:btime(s)
    local res = tonumber(s)
    self:appendLine(self:_packCounter("btime", res))
end

function CprocStat:processes(s)
    local now = tonumber(s)
    if self._lastProcesses then
        local res = now - self._lastProcesses
        self:appendLine(self:_packCounter("processes_forks", res))
    end
    self._lastProcesses = now
end

function CprocStat:procs_running(s)
    local res = tonumber(s)
    self:appendLine(self:_packCounter("procs_running", res))
end

function CprocStat:procs_blocked(s)
    local res = tonumber(s)
    self:appendLine(self:_packCounter("procs_blocked", res))
end

function CprocStat:proc(elapsed)
    CvProc.proc(self)
    for line in io.lines(self.pFile) do
        if pystring:startswith(line, "cpu") then
            self:_fcpu(line)
        elseif pystring:startswith(line, "softirq") then
            self:_fsirq(line)
        else
            local res = pystring:split(line, ' ', 1)
            if self._funs[res[1]] then
                self._funs[res[1]](res[2])
            end
        end
    end
    return self:push()
end

return CprocStat

---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liaozhaoyan.
--- DateTime: 2023/4/12 00:01
---

require("common.class")
local unistd = require("posix.unistd")
local pwait = require("posix.sys.wait")
local signal = require("posix.signal")
local pystring = require("common.pystring")
local system = require("common.system")

local CexecBase = class("execBase")
local interval = 5   --- poll for every 5 second

local function run(cmd, args)
    local pid, err = unistd.fork()
    if pid > 0 then   -- for self
        print("pid: " .. pid)
        return pid
    elseif pid == 0 then   -- for child
        local errno
        prctl_death_kill()
        _, err, errno = unistd.exec(cmd, args)
        assert(not errno, "exec failed." .. err .. errno)
    else
        error("fork report" .. err)
    end
end

function CexecBase:_init_(cmd, args, seconds)
    self.cmd = cmd
    self._cnt = 0
    self._loop = seconds / interval

    self._pid = run(cmd, args)
end

function CexecBase:addEvents(e)
    e:addEvent(self.cmd, self, interval, true, self._loop)
end

local function kill(pid)
    signal.kill(pid, signal.SIGKILL)  -- force to kill task
    pwait.wait(pid)                     -- wait task
end

function CexecBase:work()
    local cnt = self._cnt
    if cnt >= self._loop then
        local pid, stat, exit = pwait.wait(self._pid, pwait.WNOHANG)
        if pid == nil then
            error("wait failed " .. stat .. exit)
        end
        if not exit then -- process not exit
            print("force to kill " .. self._pid)
            kill(self._pid)
        end
        return -1  -- delete from task list.
    end
    self._cnt = cnt + 1
end

return CexecBase
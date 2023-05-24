---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liaozhaoyan.
--- DateTime: 2023/4/22 01:34
---

---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liaozhaoyan.
--- DateTime: 2023/4/12 00:01
---

require("common.class")
local exec = require("common.exec")
local pwait = require("posix.sys.wait")
local pystring = require("common.pystring")
local system = require("common.system")
local CvProc = require("collector.vproc")

local CforkRun = class("execBase", CvProc)

function CforkRun:_init_(opts, proto, pffi, mnt)
    CvProc._init_(self, proto, pffi, mnt)
    self._pid = exec.run(opts.cmd, opts.args)
    self._opts = opts

    print("fork run: ",  self._pid)
end

function CforkRun:_del_()
    if self._pid then
        exec.kill(self._pid)
    end
    print("kill " .. self._pid)
end

function CforkRun:proc(elapsed, lines)
    local pid, stat, exit = pwait.wait(self._pid, pwait.WNOHANG)
    if pid == nil then
        error("wait failed " .. stat .. exit)
    elseif exit then
        print("mon thread exit " .. self._pid)
        self._pid = nil
        return -1
    end
end

return CforkRun
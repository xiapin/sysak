---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liaozhaoyan.
--- DateTime: 2023/3/25 4:21 PM
---

require("common.class")
local CprotoData = require("common.protoData")
local postQue = require("beeQ.postQue.postQue")
local CvProto = require("collector.vproto")
local pystring = require("common.pystring")
local system = require("common.system")
local cjson = require("cjson.safe")
local CexecBase = require("collector.postEngine.execBase")
local CexecDiag = require("collector.postEngine.execDiag")
local Cengine = class("engine", CvProto)

local diagExec = {
    io_hang = {block = 60, time = 15, cmd = "../../../iosdiag",
               report = {title = "iosdiag",
                         files = {"/var/log/sysak/iosdiag/hangdetect/result.log.stat",
                                  "/var/log/sysak/iosdiag/hangdetect/result.log.seq"}}},
    net_edge = {block = 5 * 60, time = 60, so = {virtiostat = 5 * 3}},
}

function Cengine:_init_(que, proto_q, fYaml, tid)
    CvProto._init_(self, CprotoData.new(que))
    self._que = que
    self._fYaml = fYaml
    self._tid  = tid
    self._task = nil

    local res = system:parseYaml(fYaml)
    self._resDiag = res.diagnose
    self._diags = {}
end

function Cengine:setMainloop(main)
    self._main = main
end

function Cengine:setTask(taskMons)
    self._task = taskMons
end

function Cengine:run(e, res, diag)
    local args = res.args
    local second = res.second or diag.time
    if diag.cmd then
        local exec = CexecDiag.new(diag.cmd, args, second, self._que, diag.report, res.uid)
        exec:addEvents(e)
    end
    local so = diag.so
    if so then
        for plugin, loop in pairs(so) do
            print(plugin, loop)
            self._main.soPlugins:add(plugin, loop)
        end
    end
    self._diags[res.exec] = diag.block
end

function Cengine:pushTask(e, msgs)
    local events = pystring:split(msgs, '\n')
    for _, msg in ipairs(events) do
        local res = cjson.decode(msg)
        local cmd = res.cmd
        if cmd == "mon_pid" then
            self._task:add(res.pid, res.loop)
        elseif cmd == "exec" then  -- exec a cmd
            local execCmd = res.exec
            local args = res.args or {}
            local second = res.second or 1
            local exec = CexecBase.new(execCmd, args, second)
            exec:addEvents(e)
        elseif cmd == "diag" and self._resDiag then
            local exec = res.exec
            local diag = self._resDiag[exec]
            if diag then
                system:dumps(diag)
                if self._diags[exec] then
                    print("cmd " .. exec .. " is blocking.")
                else
                    self:run(e, res, diag)
                end
            end
        end
    end
end

function Cengine:proc(t, event, msgs)
    local lines = self._proto:protoTable()

    CvProto.proc(self, t)

    self:packLog("post_que", "post", cjson.encode(msgs))
    local bytes = self._proto:encode(lines)
    self._proto:que(bytes)

    self:pushTask(event, msgs)
end

function Cengine:checkDiag()
    local toDel = {}
    for k, v in pairs(self._diags) do
        if v > 0 then
            self._diags[k] = v - 1
        else
            table.insert(toDel, k)
        end
    end
    for _, k in ipairs(toDel) do
        self._diags[k] = nil
    end
end

function Cengine:work(t, event)
    local msgs = postQue.pull()
    if msgs then
        self:proc(t, event, msgs)
    end
    self:checkDiag()
end

return Cengine

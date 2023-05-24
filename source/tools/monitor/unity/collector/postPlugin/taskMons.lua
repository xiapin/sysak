---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liaozhaoyan.
--- DateTime: 2023/3/27 1:52 PM
---

require("common.class")
local CtaskMon = require("collector.postPlugin.taskMon")
local unistd = require("posix.unistd")
local CvProto = require("collector.vproto")
local CtaskMons = class("taskMons", CvProto)

function CtaskMons:_init_(proto, pffi, rYaml)
    CvProto._init_(self, proto)
    self._pffi = pffi
    self._mnt = rYaml.config.proc_path
    self._limit = rYaml.config.limit.tasks or 10
    self._taskList = {}
end

function CtaskMons:add(pid, loop)
    local lines = self._proto:protoTable()
    CvProto.proc(self, 1)

    if #self._taskList >= self._limit then
        self:packLog("post_req", "post", "task pids already overflow.")
    else
        if self._taskList[pid] then  -- already in
            self:packLog("post_req", "post", "pid " .. pid .. " is already in mon list.")
        else  -- check if in proc
            local dPath = self._mnt .. 'proc/' .. pid
            local res = unistd.access(dPath, 'r')
            if res then  -- exist
                self._taskList[pid] = CtaskMon.new(self._proto, self._pffi, self._mnt, pid, loop)
                self:packLog("post_req", "post", "add pid " .. pid .. " to monitor.")
            else
                self:packLog("post_req", "post", "pid " .. pid .. " is not exist.")
            end
        end
    end
    self:push(lines)

    local bytes = self._proto:encode(lines)
    self._proto:que(bytes)
end

function CtaskMons:proc(elapsed, lines)
    local res

    for pid, task in pairs(self._taskList) do
        res = task:proc(elapsed, lines)
        if res == -1 then
            self._taskList[pid] = nil
        end
    end
end

return CtaskMons

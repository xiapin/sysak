---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liaozhaoyan.
--- DateTime: 2022/12/16 10:39 PM
---

require("common.class")
local CprotoData = require("common.protoData")
local procffi = require("collector.native.procffi")
local system = require("common.system")
local CpluginManager = require("collector.pluginManager")
local calcJiffies = require("collector.guard.calcJiffies")
local CguardSched = require("collector.guard.guardSched")
local CguardDaemon = require("collector.guard.guardDaemon")
local CguardSelfStat = require("collector.guard.guardSelfStat")

local Cloop = class("loop")

function Cloop:_init_(que, proto_q, fYaml, tid)
    local res = system:parseYaml(fYaml)
    self._daemon = CguardDaemon.new(res)

    self._proto = CprotoData.new(que)
    self._tid = tid
    self:loadLuaPlugin(res, res.config.proc_path)
    local jperiod = calcJiffies.calc(res.config.proc_path, procffi)  --

    self._guardSched = CguardSched.new(tid, self._procs, self._names, jperiod)
    self._soPlugins = CpluginManager.new(procffi, proto_q, res, tid, jperiod)
    self._guardStat = CguardSelfStat.new(self._proto, procffi, "/", res, jperiod)
end

function Cloop:loadLuaPlugin(res, proc_path)
    local luas = res.luaPlugins

    self._procs = {}
    self._names = {}
    if res.luaPlugins then
        local c = 1
        for _, plugin in ipairs(luas) do
            local CProcs = require("collector." .. plugin)
            self._procs[c] = CProcs.new(self._proto, procffi, proc_path)
            self._names[c] = plugin
            c = c + 1
        end
    end
    print("add " .. system:keyCount(self._procs) .. " lua plugin.")
end

function Cloop:work(t)
    local lines = self._proto:protoTable()

    self._guardSched:proc(t, lines)
    self._soPlugins:proc(t, lines)
    self._guardStat:proc(t, lines)
    local bytes = self._proto:encode(lines)
    self._proto:que(bytes)
    self._daemon:feed()
end

return Cloop

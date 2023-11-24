---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by wrp.
--- DateTime: 2023/7/17 15:26
---

require("common.class")

local CcoMetrics = require("httplib.coMetrics")
local CcoHttpCliInst = require("httplib.coHttpCliInst")
local CtransPro = require("common.transPro")

local CcoAutoMetrics = class("coAutoMetrics", CcoMetrics)

function CcoAutoMetrics:_init_(fYaml, config, instance)
    CcoMetrics._init_(self, config, fYaml)

    local ChttpCli = require("httplib.httpCli")
    local cli = ChttpCli.new()
    local res = cli:get("100.100.100.200/latest/meta-data/region-id")
    local regionid =res.body
    self._project = "sysom-metrics-" .. regionid
    self._endpoint = regionid .. "-intranet.log.aliyuncs.com"
    self._metricstore = "auto"
    self._url = "/prometheus/" .. self._project .. "/" .. self._metricstore .. "/api/v1/write"
    self._host = self._project .. "." .. self._endpoint

    local pushMetrics = {
        host = self._host,
        url = self._url,
        port = 80
    }

    CcoHttpCliInst._init_(self, instance, pushMetrics)
    -- go ffi
    local ffi = require("common.protobuf.metricstore.ffi_lua")
    self.ffi = ffi.ffi
    self.awesome = ffi.awesome

    --fox ffi
    local foxFFI = require("tsdb.native.foxffi")
    self.foxffi = foxFFI.ffi
    self.foxcffi = foxFFI.cffi

    self._transPro = CtransPro.new(instance, fYaml, false, false)
end

return CcoAutoMetrics
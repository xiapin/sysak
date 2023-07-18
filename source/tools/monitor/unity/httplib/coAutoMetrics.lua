---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by wrp.
--- DateTime: 2023/7/17 15:26
---

require("common.class")

local CcoMetrics = require("httplib.coMetrics")

local CcoAutoMetrics = class("coAutoMetrics", CcoMetrics)

function CcoAutoMetrics:_init_(fYaml)
    CcoMetrics._init_(self,fYaml)

    local ts = io.popen('curl 100.100.100.200/latest/meta-data/region-id')
    local regionid = ts:read("*all")

    self._project = "sysom-metrics-" .. regionid
    self._endpoint = regionid .. "-intranet.log.aliyuncs.com"
    self._metricstore = "auto"
    self._url = "/prometheus/" ..self._project.."/"..self._metricstore.."/api/v1/write"
    self._host = self._project .."." .. self._endpoint

end

return CcoAutoMetrics
---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liaozhaoyan.
--- DateTime: 2023/4/7 01:36
---

package.path = package.path .. ";../?.lua;"

local system = require("common.system")
local coCli = require("httplib.coCli")
local coInflux = require("httplib.coInflux")
local coMetrics = require("httplib.coMetrics")
local coAutoMetrics = require("httplib.coAutoMetrics")
local coMetricstore = require("httplib.coMetricstore")
local coSls = require("httplib.coSls")
local coSlsLog = require("httplib.coSlsLog")
local coSlsMetric = require("httplib.coSlsMetric")

function work(fd, fYaml)
    local conf = system:parseYaml(fYaml)
    local tos = conf.pushTo
    local frame = coCli.new(fd)

    local Cidentity = require("beaver.identity")
    local inst = Cidentity.new(fYaml)
    local instance = inst:id()

    local _funcs = {
        Influx = function(fYaml, config, instance) return coInflux.new(fYaml, config, instance) end,
        Metrics = function(fYaml, config, instance) return coMetrics.new(fYaml, config, instance) end,
        AutoMetrics = function(fYaml, config, instance) return coAutoMetrics.new(fYaml, config, instance) end,
        Metricstore = function(fYaml, config, instance) return coMetricstore.new(fYaml, config, instance) end,
        Sls = function(fYaml, config, instance) return coSls.new(fYaml, config, instance) end,
        SlsLog = function(fYaml, config, instance) return coSlsLog.new(fYaml, config, instance) end,
        SlsMetric = function(fYaml, config, instance) return coSlsMetric.new(fYaml, config, instance)  end
    }

    local clis = {}
    for _, push in ipairs(tos) do
        table.insert(clis, _funcs[push.to](fYaml, push, instance))
    end
    frame:poll(clis)

    print("end push.")
    return 0
end

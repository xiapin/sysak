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

function work(fd, fYaml)
    local conf = system:parseYaml(fYaml)
    local to = conf.pushTo.to
    local frame = coCli.new(fd)
    local _funcs = {
        Influx = function(fYaml) return coInflux.new(fYaml) end,
        Metrics = function(fYaml) return coMetrics.new(fYaml) end
    }
    local c = _funcs[to](fYaml)
    --local c = _funcs[to]("/etc/sysak/base.yaml")
    frame:poll(c)

    print("end push.")
    return 0
end

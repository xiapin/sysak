---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liaozhaoyan.
--- DateTime: 2022/12/21 11:44 AM
---

package.path = package.path .. ";../?.lua;"

local Cframe = require("beaver.frame")
local CurlApi = require("beaver.url_api")
local CurlRpc = require("beaver.url_rpc")
local CurlIndex = require("beaver.index")
local Cexport = require("beaver.export")
local CurlGuide = require("beaver.url_guide")
local CurlExportHtml = require("beaver.url_export_html")
local CurlExportRaw = require("beaver.url_export_raw")
local CLocalBeaver = require("beaver.localBeaver")
local CbaseQuery = require("beaver.query.baseQuery")
local system = require("common.system")

g_lb = nil

function init(que, fYaml)
    fYaml = fYaml or "../collector/plugin.yaml"
    local web = Cframe.new()
    local res = system:parseYaml(fYaml)

    CbaseQuery.new(web, fYaml)
    CurlApi.new(web, que, fYaml)

    if res.config.url_safe ~= "close" then
        CurlIndex.new(web)
        CurlRpc.new(web)
        CurlGuide.new(web)
    end


    local Cidentity = require("beaver.identity")
    local inst = Cidentity.new(fYaml)
    local export = Cexport.new(inst:id(), fYaml)
    CurlExportHtml.new(web, export)
    CurlExportRaw.new(web, export)

    g_lb = CLocalBeaver.new(web, fYaml)
    return 0
end

function echo()
    return g_lb:poll()
end

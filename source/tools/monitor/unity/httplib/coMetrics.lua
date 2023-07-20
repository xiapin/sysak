---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by wrp.
--- DateTime: 2023/7/7 17:07
---

require("common.class")

local CcoHttpCliInst = require("httplib.coHttpCliInst")
local system = require("common.system")
local pystring = require("common.pystring")
local lineParse = require("common.lineParse")
local CtransPro = require("common.transPro")
local base64 = require("base64")
local addition = require("common.addition")

local CcoMetrics = class("coMetrics", CcoHttpCliInst)

function CcoMetrics:_init_(fYaml)

    local res = system:parseYaml(fYaml)
    local _metrics = res.metrics
    self._mhead = _metrics.head
    self._title = _metrics.title

    local Cidentity = require("beaver.identity")
    local inst = Cidentity.new(fYaml)
    local instance = inst:id()

    local _addition = res.pushTo.addition

    self._key1, self._key2 = addition:decode(_addition)
    self._project = res.pushTo.project
    self._endpoint = res.pushTo.endpoint
    self._metricstore = res.pushTo.metricstore
    self._url = "/prometheus/" ..self._project.."/"..self._metricstore.."/api/v1/write"
    self._host = self._project .."." .. self._endpoint

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

function CcoMetrics:echo(tReq)
    --if tReq.code ~= "204" then
    print(tReq.code, tReq.data)
    --end
end

function CcoMetrics:trans(msgs)
    local res
    local c = 0
    local lines

    lines = msgs.lines
    res = self._transPro:export(lines)
    local prome = self.ffi.new("GoString")
    prome.p = res
    prome.n = #res
    local prome_ptr = self.ffi.cast("GoString*", prome)
    local byte = self.ffi.new("GoSlice")
    local byte_ptr = self.ffi.cast("GoSlice*", byte) --{ void *data; GoInt len; GoInt cap; } GoSlice
    local data_len = self.awesome.metricSnappy(prome,byte_ptr)
    data_len = tonumber(data_len)
    local data = self.ffi.cast("GoUint8*", byte_ptr.data)

    return self.foxffi.string(data, data_len)
end

function CcoMetrics:pack(body)
    local line = self:packCliHead('POST', self._url)
    local keys = self._key1 .. ":"..self._key2
    local keys64 = base64.encode(keys)
    local head = {
        Host = self._host,
        ["Content-Encoding"] = "snappy",
        ["Content-Type"] = "application/x-protobuf",
        --["X-Prometheus-Remote-Write-Version"] = "0.1.0",
        ["Content-Length"] = #body,
        ["Authorization"] = "Basic " .. keys64,
    }
    local heads = self:packCliHeaders(head)
    print("pack finish")
    return pystring:join("\r\n", {line, heads, body})
end

return CcoMetrics
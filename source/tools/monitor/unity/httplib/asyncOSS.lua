---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liaozhaoyan.
--- DateTime: 2023/5/24 20:07
---

require("common.class")
local system = require("common.system")
local pystring = require("common.pystring")
local sha1 = require("sha1")
local sls_api = require("sls_api")
local CasyncHttp = require("httplib.asyncHttp")
local base64 = require("base64")
local CasyncOSS = class("asyncOSS", CasyncHttp)

function CasyncOSS:_init_(res)
    CasyncHttp._init_(self)
    self._bucket = res.bucket
    self._endPoint = res.endPoint
    self._k1, self._k2 = sls_api.decode(res.addition)
end

function CasyncOSS:sign(cType, date, uri)
    local ss = {
        "PUT",  -- VERB
        "",     -- md5
        cType,  -- Content-Type
        date,   -- Date
        uri,    --
    }
    local s = table.concat(ss, '\n')
    return base64.encode(sha1.hmac_binary(self._k2, s))
end

function CasyncOSS:auth(cType, date, uri)
    local ss = {"OSS ", self._k1, ":", self:sign(cType, date, uri)}
    return table.concat(ss)
end

function CasyncOSS:put(uuid, stream)
    local bucket = self._bucket
    local uri = os.date("/%Y/%m") .. "/" .. uuid

    local uris = table.concat({"/", bucket, uri})
    local date = system:timeRfc1123(os.time())
    local cType = "application/octet-stream"
    local host = bucket .. "." .. self._endPoint

    local headers = {
        Host = host,
        ["Content-Type"] = cType,
        ["Content-Length"] = #stream,
        date = date,
        authorization = self:auth(cType, date, uris)
    }

    CasyncHttp.put(self, host, uri, headers, stream)
end

return CasyncOSS
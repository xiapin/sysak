---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liaozhaoyan.
--- DateTime: 2023/5/23 20:43
---

local sha1 = require("sha1")
local md5 = require("md5")
local base64 = require("base64")
local system = require("common.system")
local pystring = require("common.pystring")

local ChttpCli = require("httplib.httpCli")
local CossCli = class("ossCli", ChttpCli)

function CossCli:_init_(endPoint, bucket, ak, sk)
    ChttpCli._init_(self)
    self._endPoint = endPoint
    self._bucket = bucket
    self._ak = ak
    self._sk = sk
end

function CossCli:sign(cType, date, uri)
    local ss = {
        "PUT",  -- VERB
        "",     -- md5
        cType,  -- Content-Type
        date,   -- Date
        uri,    --
    }
    local s = table.concat(ss, '\n')
    return base64.encode(sha1.hmac_binary(self._sk, s))
end

function CossCli:auth(cType, date, uri)
    local ss = {"OSS ", self._ak, ":", self:sign(cType, date, uri)}
    return table.concat(ss)
end

function CossCli:put(uri, stream, cType)
    local bucket = self._bucket
    local uris = table.concat({"/", bucket, uri})
    cType = cType or "application/octet-stream"

    local host = bucket .. "." .. self._endPoint
    local date = system:timeRfc1123(os.time())
    local header = {
        Host = host,
        ["Content-Type"] = cType,
        ["Content-Length"] = #stream,
        date = date,
        authorization = self:auth(cType, date, uris)
    }
    print(header.authorization)
    local url = table.concat({"http://", self._endPoint, uri})
    print(url)

    return ChttpCli.put(self, url, stream, header)
end

return CossCli

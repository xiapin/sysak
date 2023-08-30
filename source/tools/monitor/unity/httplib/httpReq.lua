---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liaozhaoyan.
--- DateTime: 2023/8/27 11:29 PM
---

-- refer to https://github.com/daurnimator/lua-http/blob/master/examples/simple_request.lua
-- doc https://daurnimator.github.io/lua-http/0.4/#http.request

require("common.class")
local request = require "http.request"
local system = require("common.system")
local ChttpComm = require("httplib.httpComm")
local ChttpReq = class("httpReq", ChttpComm)


function ChttpReq:_init_(proxy, tmo)
    ChttpComm._init_(self)
    self._proxy = proxy
    self._tmo = tmo or 1
end

local function setReqConfig(obj, req)
    if obj._proxy then
        req.proxy = obj._proxy
    end
end

local function eachHeaders(header)
    local head = {}
    for k, v in header:each() do
        head[k] = v
    end
    return head
end

function ChttpReq:get(uri)
    local req = request.new_from_uri(uri)
    setReqConfig(self, req)
    req.headers:upsert(":method", "GET")

    local headers, stream = req:go(self._tmo)
    return {
        code = headers:get(":status"),
        head = eachHeaders(headers),
        body = stream:get_body_as_string()
    }
end

function ChttpReq:post(uri, body, headers)
    local req = request.new_from_uri(uri)
    setReqConfig(self, req)
    req.headers:upsert(":method", "POST")
    for k, v in pairs(headers) do
        req.headers:upsert(k, v)
    end
    req:set_body(body)

    local hdrs, stream = req:go(self._tmo)
    return {
        code = hdrs:get(":status"),
        head = eachHeaders(hdrs),
        body = stream:get_body_as_string()
    }
end

function ChttpReq:postTable(uri, t)
    local req = self:jencode(t)
    local headers = {
        ["Content-Type"] = "application/json",
    }
    return self:post(uri, req, headers)
end

function ChttpReq:postLine(uri, line)
    local headers = {
        ["Content-Type"] = "text/plain",
    }
    return self:post(uri, line, headers)
end

local function addContent(content, c, line)
    content[c] = line
    return c + 1
end

function ChttpReq:postFormData(Url, headers, fData)
    if not headers["accept"] then
        headers["accept"] = "application/json"
    end
    if not headers["Content-Type"] then
        headers["Content-Type"] = "multipart/form-data"
    end

    local boundary = "----" .. system:randomStr(32)
    local c = 1
    local content = {}
    for k, v in pairs(fData) do
        c = addContent(content, c, boundary)   -- add boundary
        if type(v) == "table" then -- file: name, stream, type
            c = addContent(content, c, string.format('Content-Disposition: form-data; name="%s"; filename="%s"', k, v[1]))
            c = addContent(content, c, string.format('Content-Type: %s', v[3]))
            c = addContent(content, c, "")
            c = addContent(content, c, v[2])
        else
            c = addContent(content, c, string.format('Content-Disposition: form-data; name="%s"', k))
            c = addContent(content, c, "")
            c = addContent(content, c, v)
        end
    end
    addContent(content, c, boundary .. "--")
    local s = table.concat(content, "\r\n")
    headers["Content-Type"] = string.format("multipart/form-data; boundary=%s", boundary)
    return self:post(Url, s, headers)
end

return ChttpReq

---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liaozhaoyan.
--- DateTime: 2022/12/22 12:08 PM
---

require("common.class")
local system = require("common.system")
local ChttpApp = require("httplib.httpApp")
local CfoxTSDB = require("tsdb.foxTSDB")
local CfoxSQL = require("tsdb.foxSQL")
local postQue = require("beeQ.postQue.postQue")
local CpushLine = require("beaver.pushLine")
local CasyncDns = require("httplib.asyncDns")
local CasyncHttp = require("httplib.asyncHttp")
local CasyncHttps = require("httplib.asyncHttps")
local CasyncOSS = require("httplib.asyncOSS")
local CurlApi = class("urlApi", ChttpApp)

function CurlApi:_init_(frame, que, fYaml)
    ChttpApp._init_(self)
    self._pushLine = CpushLine.new(que)
    local res = system:parseYaml(fYaml)
    if res.config.url_safe==nil or res.config.url_safe ~= "close" then
        self._urlCb["/api/sum"] = function(tReq) return self:sum(tReq)  end
        self._urlCb["/api/sub"] = function(tReq) return self:sub(tReq)  end
        self._urlCb["/api/trig"] = function(tReq) return self:trig(tReq)  end
        self._urlCb["/api/line"] = function(tReq) return self:line(tReq)  end
        self._urlCb["/api/dns"] = function(tReq) return self:dns(tReq)  end
        self._urlCb["/api/proxy"] = function(tReq) return self:proxy(tReq)  end
        self._urlCb["/api/ssl"] = function(tReq) return self:ssl(tReq) end
        if res.diagnose then
            self._diagAuth = res.diagnose.token
            self._diagHost = res.diagnose.host
            self._urlCb["/api/diag"] = function(tReq) return self:diag(tReq) end
        end

    end

    self._urlCb["/api/query"] = function(tReq) return self:query(tReq)  end

    self._urlCb["/api/sql"] = function(tReq) return self:qsql(tReq) end

    self:_ossIntall(fYaml)

    self:_install(frame)
    self:_setupQs(fYaml)
    self._proxyhttp = CasyncHttp.new()
    self._proxyhttps = CasyncHttps.new()


end

function CurlApi:_ossIntall(fYaml)
    local res = system:parseYaml(fYaml)
    if res.oss then
        self._oss = CasyncOSS.new(res.oss)
        self._urlCb["/api/oss"] = function(tReq) return self:oss(tReq)  end
    end
end

local function reqOSS(oss, uuid, stream)
    return oss:put(uuid, stream)
end

function CurlApi:oss(tReq)
    local uuid = tReq.header['uuid']
    local cLen = tonumber(tReq.header['content-length'])
    if uuid and cLen and cLen > 0 then
        local stream = tReq.data
        if stream then
            local stat, body = pcall(reqOSS, self._oss, uuid, stream)
            if stat then
                return body
            else
                return "bad req dns " .. body, 400
            end
        else
            return "need stream arg.", 400
        end
    else
        return "need uuid and content-length > 0." .. tReq.data, 400
    end
end

local function reqSSL(https, host, uri, port)
    port = port or 443
    return https:get(host, uri, port)
end

function CurlApi:ssl(tReq)
    local stat, tJson = pcall(self.getJson, self, tReq)
    if stat and tJson then
        local host = tJson.host
        local uri = tJson.uri
        if host and uri then
            local https = CasyncHttps.new()
            local stat, body = pcall(reqSSL, https, host, uri)
            if stat then
                return {body = body}
            else
                return "bad req dns " .. body, 400
            end
        else
            return "need domain arg.", 400
        end
    else
        return "bad dns " .. tReq.data, 400
    end
end

local function reqProxy(proxy, host, uri)
    return proxy:get(host, uri)
end

local function proxyPost(proxy, host, uri, headers, body)
    return proxy:post(host, uri, headers, body)
end

function CurlApi:diag(tReq)
    local stat, tJson = pcall(self.getJson, self, tReq)
    if stat and tJson then
        local host = self._diagHost
        local uri = "/api/v1/tasks/sbs_task_create/"
        if not host then
            host = tJson.host
        end
        --local headers = tJson.headers
        local reqbody = tJson.body
        local headers = {
            accept = "application/json",
            ["Content-Type"] = "application/json",
            authorization = self._diagAuth
        }
        local service_name = reqbody.service_name

        if host and uri then
            local stat
            local body
            if string.sub(host,1,5) == "https" then
                host = string.sub(host,9)
                stat, body = pcall(proxyPost, self._proxyhttps, host, uri, headers, self:jencode(reqbody))
            else
                host = string.sub(host,8)
                stat, body = pcall(proxyPost, self._proxyhttp, host, uri, headers, self:jencode(reqbody))
            end
            if stat then
                body = self:jdecode(body)
                if body.code == 200 then
                    local data = body.data
                    data["service_name"] = service_name
                    local s = self:jencode(data)
                    --TODO：除了data，service_name也要传
                    postQue.post(s)
                end
                return {task_id = body.data.task_id}
            else
                return "bad req dns " .. body, 400
            end
        else
            return "need domain arg.", 400
        end
    else
        return "bad dns " .. tReq.data, 400
    end
end

function CurlApi:proxy(tReq)
    local stat, tJson = pcall(self.getJson, self, tReq)
    if stat and tJson then
        local host = tJson.host
        local uri = tJson.uri
        if host and uri then
            local stat, body = pcall(reqProxy, self._proxy, host, uri)
            if stat then
                return {body = body}
            else
                return "bad req dns " .. body, 400
            end
        else
            return "need domain arg.", 400
        end
    else
        return "bad dns " .. tReq.data, 400
    end
end

local function reqDns(domain)
    local dns = CasyncDns.new()
    return dns:dns_lookup(domain)
end

function CurlApi:dns(tReq)
    local stat, tJson = pcall(self.getJson, self, tReq)
    if stat and tJson then
        local domain = tJson.domain
        if domain then
            local stat, ip = pcall(reqDns, domain)
            if stat then
                return {domain = tReq.data, ip = ip}
            else
                return "bad req dns " .. tReq.data .. ip, 400
            end
        else
            return "need domain arg.", 400
        end
    else
        return "bad dns " .. tReq.data, 400
    end
end

function CurlApi:line(tReq)
    local stat, _ = pcall(self._pushLine.procLines, self._pushLine, tReq.data)
    if stat then
        return "ok"
    else
        return "bad line " .. tReq.data, 400
    end
end

function CurlApi:trig(tReq)
    local stat, tJson = pcall(self.getJson, self, tReq)
    if stat and tJson then
        local s = self:jencode(tJson)
        if #s > 5 then
            postQue.post(s)
        end
        return tJson
    else
        return {}
    end
end

function CurlApi:sum(tReq)
    local stat, tJson = pcall(self.getJson, self, tReq)
    if stat then
        return {sum=tJson.num1 + tJson.num2}
    else
        return {}
    end
end

function CurlApi:sub(tReq)
    local stat, tJson = pcall(self.getJson, self, tReq)
    if stat then
        return {sub=tJson.num1 - tJson.num2}
    else
        return {}
    end
end

function CurlApi:qlast(tJson)
    local ts = tJson["time"]
    local secs = 0
    if string.find(ts, "%d+[hms]$") then
        local unit = string.sub(ts, -1)
        local num = tonumber(string.sub(ts, 1, -2))
        if unit == "h" then
            secs = num * 60 * 60
        elseif unit == "m" then
            secs = num * 60
        else
            secs = num
        end
    else
        secs = tonumber(ts)
    end
    local tbls = nil
    if system:keyIsIn(tJson, "table") and type(tJson["table"]) == "table" then
        tbls = tJson["table"]
    end
    return self.fox:qNow(secs, tbls)
end

function CurlApi:qdate(tJson)
    local start = tJson["start"]
    local stop  = tJson["stop"]

    local dStart = self.fox:str2date(start)
    local dStop  = self.fox:str2date(stop)

    if system:keyIsIn(tJson, "tz") then
        dStart = self.fox:moveSec(dStart, -tJson["tz"] * 60 * 60)
        dStop  = self.fox:moveSec(dStop,  -tJson["tz"] * 60 * 60)
    end

    local tbls = nil
    if system:keyIsIn(tJson, "table") and type(tJson["table"]) == "table" then
        tbls = tJson["table"]
    end
    return self.fox:qDate(dStart, dStop, tbls)
end

function CurlApi:qtable(tJson)
    local secs = 0
    if system:keyIsIn(tJson, "duration") then
        secs = tJson["duration"] * 60 * 60
    else
        secs = 2 * 60 * 60
    end
    return self.fox:qTabelNow(secs)
end

function CurlApi:qsql(tReq)
    local stat, tJson = pcall(self.getJson, self, tReq)
    if stat then
        local res = self.foxSQL:parse(tJson)
        if res.error ~= nil or res.cursorpos ~= nil then
            print("sql parse error")
            return {}
        end
        return self.foxSQL:sql(res)
    else
        return {}
    end


end

function CurlApi:_setupQs(fYaml)
    self.fox = CfoxTSDB.new(fYaml)
    self.foxSQL = CfoxSQL.new(fYaml)
    self._q = {}
    self._q["last"] = function(tJson) return self:qlast(tJson) end
    self._q["table"] = function(tJson) return self:qtable(tJson) end
    self._q["date"] = function(tJson) return self:qdate(tJson) end
    --self._q["sql"] = function(tJson) return self:qsql(tJson) end
end

function CurlApi:lquery(tJson)
    if system:keyIsIn(self._q, tJson["mode"]) then
        return self._q[tJson["mode"]](tJson)
    else
        return {}
    end
end

function CurlApi:query(tReq)
    local stat, tJson = pcall(self.getJson, self, tReq)
    if stat then
        local cStat, ms = pcall(self.lquery, self, tJson)
        if cStat then
            return ms
        else
            return {}
        end
    else
        return {}
    end
end

return CurlApi

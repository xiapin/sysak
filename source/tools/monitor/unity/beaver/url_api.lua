---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liaozhaoyan.
--- DateTime: 2022/12/22 12:08 PM
---

require("common.class")
local system = require("common.system")
local ChttpApp = require("httplib.httpApp")
local CfoxTSDB = require("tsdb.foxTSDB")
local postQue = require("beeQ.postQue.postQue")
local CpushLine = require("beaver.pushLine")

local CurlApi = class("urlApi", ChttpApp)

function CurlApi:_init_(frame, que, fYaml)
    ChttpApp._init_(self)
    self._pushLine = CpushLine.new(que)
    self._urlCb["/api/sum"] = function(tReq) return self:sum(tReq)  end
    self._urlCb["/api/sub"] = function(tReq) return self:sub(tReq)  end
    self._urlCb["/api/query"] = function(tReq) return self:query(tReq)  end
    self._urlCb["/api/trig"] = function(tReq) return self:trig(tReq)  end
    self._urlCb["/api/line"] = function(tReq) return self:line(tReq)  end
    self:_install(frame)
    self:_setupQs(fYaml)
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

function CurlApi:_setupQs(fYaml)
    self.fox = CfoxTSDB.new(fYaml)
    self._q = {}
    self._q["last"] = function(tJson) return self:qlast(tJson) end
    self._q["table"] = function(tJson) return self:qtable(tJson) end
    self._q["date"] = function(tJson) return self:qdate(tJson) end
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

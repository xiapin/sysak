---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liaozhaoyan.
--- DateTime: 2022/12/23 11:53 PM
---

require("common.class")
local unistd = require("posix.unistd")
local pystring = require("common.pystring")
local system = require("common.system")
local ChttpBase = require("httplib.httpBase")

local ChttpHtml = class("ChttpHtml", ChttpBase)

function ChttpHtml:_init_(frame)
    ChttpBase._init_(self)

    self._reCb = {
        md = function(s, keep, suffix) return self:renderMd(s, keep, suffix)  end,
        html = function(s, keep, suffix) return self:renderHtml(s, keep, suffix)  end,
        jpg = function(s, keep, suffix) return self:renderImage(s, keep, "jpeg")  end,
        jpeg = function(s, keep, suffix) return self:renderImage(s, keep, suffix)  end,
        gif = function(s, keep, suffix) return self:renderImage(s, keep, suffix)  end,
        png = function(s, keep, suffix) return self:renderImage(s, keep, suffix)  end,
        bmp = function(s, keep, suffix) return self:renderImage(s, keep, suffix)  end,
        txt = function(s, keep, suffix) return self:renderText(s, keep, suffix)  end,
        text = function(s, keep, suffix) return self:renderText(s, keep, suffix)  end,
        log = function(s, keep, suffix) return self:renderText(s, keep, suffix)  end,
    }
end

local function loadFile(fPpath)
    local f = io.open(fPpath,"r")
    local s = f:read("*all")
    f:close()
    return s
end

function ChttpHtml:markdown(text)
    local md = require("common.lmd")
    return md:toHtml(text)
end

function ChttpHtml:renderMd(s, keep, suffix)
    local tmd = self:markdown(s)
    local tRet = {
        title = "markdown document render from beaver.",
        content = tmd,
        cType = "text/html",
    }
    return self:echo(tRet, keep)
end

function ChttpHtml:renderHtml(s, keep, suffix)
    local cType = "text/html"
    return self:pack(cType, keep, s)
end

function ChttpHtml:renderText(s, keep, suffix)
    local cType = "text/plain"
    return self:pack(cType, keep, s)
end

function ChttpHtml:renderImage(s, keep, suffix)
    local cType = "image/" .. suffix
    return self:pack(cType, keep, s)
end

function ChttpHtml:reSource(tReq, keep, head, srcPath)
    local path = tReq.path
    path = srcPath .. pystring:lstrip(path, head)
    if unistd.access(path) then
        local s = loadFile(path)
        local _, suffix = unpack(pystring:rsplit(path, ".", 1))
        if system:keyIsIn(self._reCb, suffix) then
            return self._reCb[suffix](s, keep, suffix)
        end
    end
end

local function htmlPack(title, content)
    local h1 = [[
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>]]
    local h2 = [[</title>
</head>

<body>
]]
    local h3 = [[
</body>

</html>
]]

    local bodies = {h1, title, h2, content, h3}
    return pystring:join("", bodies)
end

function ChttpHtml:pack(cType, keep, body)
    local stat = self:packStat(200)
    local tHead = {
        ["Content-Type"] = cType,
        ["Connection"] = (keep and "keep-alive") or "close"
    }
    local headers = self:packHeaders(tHead, #body)
    local tHttp = {stat, headers, body}
    return pystring:join("\r\n", tHttp)
end

function ChttpHtml:echo(tRet, keep)
    local cType = tRet.type or "text/html"
    local body = htmlPack(tRet.title, tRet.content)

    return self:pack(cType, keep, body)
end

return ChttpHtml

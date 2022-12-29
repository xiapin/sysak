---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liaozhaoyan.
--- DateTime: 2022/12/23 11:53 PM
---

require("class")
local pystring = require("pystring")
local ChttpBase = require("httpBase")

local ChttpHtml = class("ChttpHtml", ChttpBase)

function ChttpHtml:_init_(frame)
    ChttpBase._init_(self)
end


function ChttpHtml:markdown(text)
    local md = require("markdown")
    return md(text)
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

function ChttpHtml:echo(tRet)
    local stat = self:packStat(200)
    local tHead = {
        ["Content-Type"] = "text/html",
    }
    local body = htmlPack(tRet.title, tRet.content)
    local headers = self:packHeaders(tHead, #body)
    local tHttp = {stat, headers, body}
    return pystring:join("\r\n", tHttp)
end

return ChttpHtml

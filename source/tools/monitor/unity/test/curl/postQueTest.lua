---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liaozhaoyan.
--- DateTime: 2023/3/24 11:52 PM
---

package.path = package.path .. ";../../?.lua;"
local system = require("common.system")
local ChttpCli = require("httplib.httpCli")

local cli = ChttpCli.new()
local url = "http://127.0.0.1:8400/api/que"
local req = {cmd = "mon_pid", pid = {24, 25}}
local res = cli:postTable(url, req)
system:dumps(res)

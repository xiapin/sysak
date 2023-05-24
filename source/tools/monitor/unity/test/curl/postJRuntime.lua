---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liaozhaoyan.
--- DateTime: 2023/5/23 00:11
---

package.path = package.path .. ";../../?.lua;"
local system = require("common.system")
local ChttpCli = require("httplib.httpCli")

local cli = ChttpCli.new()
local url = "http://127.0.0.1:8400/api/trig"
--local req = {cmd = "diag", exec = "io_hang", args = {"hangdetect", "vda"}}
local req = {cmd = "diag", exec = "jruntime", args = {"-d", "5", "--top", "2"}, uid = system:guid()}
local res = cli:postTable(url, req)
system:dumps(res)
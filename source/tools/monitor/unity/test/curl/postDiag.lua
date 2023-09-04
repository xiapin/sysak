---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liaozhaoyan.
--- DateTime: 2023/5/4 13:21
---

package.path = package.path .. ";../../?.lua;"
local system = require("common.system")
local ChttpCli = require("httplib.httpCli")

local cli = ChttpCli.new()
local url = "http://127.0.0.1:8400/api/diag"
--local req = {cmd = "diag", exec = "io_hang", args = {"hangdetect", "vda"}}
--local req = {cmd = "diag", exec = "io_hang", args = {"hangdetect", "vda"}, uid = system:guid()}
local headers = {
    --accept = "application/json",
    --["Content-Type"] = "application/json",
    --authorization = "eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzI1NiJ9.eyJpZCI6MSwidXNlcm5hbWUiOiJhZG1pbiIsImV4cCI6MTY5MzA0MzYwMi43NDc1NDh9.pm78vETkFeR8xX-TFA4ROVjVzO_VlfUuwUA3TzTxpfA"
}
local body = {
    service_name = "iohang",
    params = {
        instance= "127.0.0.1"
    }
}
local req = {host = "192.168.0.121", uri = "/api/v1/tasks/sbs_task_create/", headers = headers, body = body}
local res = cli:postTable(url, req)

system:dumps(res)
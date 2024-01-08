---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liaozhaoyan.
--- DateTime: 2023/8/12 5:48 PM
---

package.path = package.path .. ";../../?.lua"

local pbApi = require("test.protoBuf.pbInterFace")
local CfoxTSDB = require("tsdb.foxTSDB")
local system = require("common.system")

local fYaml = "fox.yaml"

local wdb = CfoxTSDB.new(fYaml)
wdb:setupWrite()
system:dumps(wdb:fileNames())

local c = 1
while true do
    local lines = {
        "hello,title=hello value1=3,value2=4",
        string.format("test v=3,add=%d", c),
        'log,title=hello str="test.hello."'
    }
    local res = pbApi.protoLines(table.concat(lines, "\n"))
    wdb:write(res)

    print("write.")
    c = c + 1
    system:sleep(1)
end

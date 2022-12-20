---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liaozhaoyan.
--- DateTime: 2022/12/17 1:07 PM
---

package.path = package.path .. ";../common/?.lua;"
package.path = package.path .. ";../tsdb/?.lua;"
package.path = package.path .. ";../tsdb/native/?.lua;"

local system = require("system")

local CfoxTSDB = require("foxTSDB")

local fox = CfoxTSDB.new()
local ret = fox:setupWrite()
assert(ret == 0)

local line = {
    lines = {
        {
            line = "metric1",
            ls = {
                { name = "title", index = "hello" }
            },
            vs = {
                { name = "value", value = 3.3 },
                { name = "cut", value = 3.4 }
            }
        },
        {
            line = "metric2",
            vs = {
                { name = "value", value = 3.3 },
                { name = "cut", value = 3.4 }
            },
            log = {
                { name = "hello", log = "world." },
            }
        },
    }
}

while true do
    system:sleep(1)
    line.lines[1].vs[1].value = line.lines[1].vs[1].value + 1
    line.lines[2].vs[2].value = line.lines[2].vs[2].value + 3
    local res = fox:packLine(line)
    assert(string.len(res) > 0)
    fox:write(res)
end
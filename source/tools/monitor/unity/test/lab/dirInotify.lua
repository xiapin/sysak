---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liaozhaoyan.
--- DateTime: 2023/3/24 2:15 PM
---

package.path = package.path .. ";../../?.lua;"

local Cinotifies = require("common.inotifies")
local system = require("common.system")

local ino = Cinotifies.new("/tmp")

while true do
    system:sleep(1)
    if ino:isChange() then
        ino = Cinotifies.new("/tmp")
        print("change.")
    end
end

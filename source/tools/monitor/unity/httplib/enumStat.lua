---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liaozhaoyan.
--- DateTime: 2023/4/4 17:15
---

local system = require("common.system")

local socketStatus = {
    "connecting",
    "connected",
    "sending",
    "receiving",
    "closed"
}

return system:Enum(socketStatus)

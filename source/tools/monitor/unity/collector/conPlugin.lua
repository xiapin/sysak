---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liaozhaoyan.
--- DateTime: 2023/3/10 8:26 AM
---

require("common.class")
local system = require("common.system")
local CconPlugin = class("conPlugin")

function CconPlugin:_init_(proto, procffi, que, proto_q, fYaml)
    self._proto = proto
    self._que = que
    self._plugins = setup()
end

function CconPlugin:proc(elapsed, lines)

end
---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liaozhaoyan.
--- DateTime: 2022/12/16 1:07 PM
---

require("class")
local Cone = require("one")

CThree = class("three", Cone)

function CThree:_init_(name)
    Cone._init_(self, name)
    self._child = Cone.new("child")
end


function CThree:say()
    print("three say " .. self.name)
    print("child say.")
    self._child:say()
end

return CThree
---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liaozhaoyan.
--- DateTime: 2023/3/24 1:52 PM
---

local inotify = require('inotify')
local handle = inotify.init()

-- Watch for new files and renames
local wd = handle:addwatch('/tmp/inotify/', inotify.IN_CREATE, inotify.IN_MOVE)

for ev in handle:events() do
    print(ev.name .. ' was created or renamed')
end

-- Done automatically on close, I think, but kept to be thorough
handle:rmwatch(wd)

handle:close()
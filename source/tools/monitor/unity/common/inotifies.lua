---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liaozhaoyan.
--- DateTime: 2023/3/24 2:34 PM
---

require("common.class")

local Cinotifies = class("inotifies")
local dirent = require("posix.dirent")
local pstat = require("posix.sys.stat")
local system = require("common.system")
local inotify = require('inotify')

local function walk_dirs(path, dirs)
    local files = dirent.files(path)
    for f in files do
        if string.sub(f, 1, 1) ~= '.' then
            local full = table.concat({path, f}, "/")
            local lStat = pstat.lstat(full)
            if pstat.S_ISDIR(lStat.st_mode) > 0 then
                table.insert(dirs, full)
                walk_dirs(full, dirs)
            end
        end
    end
end

local function mon_dirs(path)
    local handle = inotify.init()
    local ws = {}
    local c = 0

    local dirs = {path}
    walk_dirs(path, dirs)

    for _, dir in ipairs(dirs) do
        local w = handle:addwatch(dir, inotify.IN_CREATE, inotify.IN_MOVE, inotify.IN_DELETE)
        if w > 0 then
            c = c + 1
            ws[c] = w
        else
            error("add " .. dir .. " to watch failed.")
        end
    end

    system:fdNonBlocking(handle:getfd())
    return handle, ws
end

function Cinotifies:_init_(path)
    self._handle, self._ws = mon_dirs(path)
end

function Cinotifies:_del_()
    for _, w in ipairs(self._ws) do
        self._handle:rmwatch(w)
    end
    self._handle:close()
end

function Cinotifies:isChange()
    local events = self._handle:read()
    return #events > 0
end

return Cinotifies

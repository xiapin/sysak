require("common.class")

local dirent = require("posix.dirent")
local unistd = require("posix.unistd")
local pystring = require("common.pystring")
local Cinotifies = require("common.inotifies")

local cgrpV2 = class("cgroupv2")

function cgrpV2:_init_(resYaml, proto, pffi, mnt)
    self._verbose = false
    self._proto = proto
    self._pffi = pffi
    self._mnt = mnt
    self._resYaml = resYaml
    self._plugins = self:setupPlugins(proto, pffi, mnt)
end

function cgrpV2:setupPlugins(proto, pffi, mnt)
    local plugins = {}

    for _, path in ipairs(self._resYaml.cgroupv2.directPaths) do
        local lables = {
            { name = "path", index = path }
        }
        for _, plugin in ipairs(self._resYaml.cgroupv2.luaPlugin) do
            local CProcs = require("collector.container.cgroupv2." .. plugin)
            table.insert(plugins, CProcs.new(proto, pffi, mnt, path, lables))
        end
    end
    return plugins
end

function cgrpV2:proc(elapsed, lines)
    for _, plugin in ipairs(self._plugins) do
        local stat, res = pcall(plugin.proc, plugin, elapsed, lines)
        if not stat or res == -1 then
            print("cgroupv2: pcall plugin error.")
        end
    end
end

return cgrpV2

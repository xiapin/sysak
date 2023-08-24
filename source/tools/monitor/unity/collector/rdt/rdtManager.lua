require("common.class")

local dirent = require("posix.dirent")
local unistd = require("posix.unistd")
local stat = require("posix.sys.stat")
local bit = require("bit")
local pystring = require("common.pystring")
local system = require("common.system")
local Cinotifies = require("common.inotifies")

local rdtMgr = class("rdtManager")

function rdtMgr:_init_(resYaml, proto, pffi, mnt)
    local dir = mnt .. resYaml.resctrl.path
    if unistd.access(dir) then
        self._top = dir
        self._path = resYaml.resctrl.path
        self._enable = true
    else
        print("rdtManager: Machine or OS not support resctrl? Please check the resctrl path.\n")
        self._enable = false
        return
    end

    self._verbose = false
    self._proto = proto
    self._pffi = pffi
    self._mnt = mnt
    self._resYaml = resYaml
    if resYaml.resctrl.auto ~= nil and resYaml.resctrl.auto then
        self._resDirs, self._monDirs = self:searchDirs()
    else
        self._resDirs, self._monDirs = self:checkDirs()
    end

    self._plugins = self:setupPlugins(proto, pffi, mnt)
end

function rdtMgr:isSystemFile(file)
    local black_list = { "cpus_list", "info", "mon_data", "schemata", "tasks", "cpus", "id", "mode", "size", "mon_groups" }
    for _, fname in ipairs(black_list) do
        if file == fname then
            return true
        end
    end
    return false
end

function rdtMgr:searchDirs()
    local topDir = self._top
    local resDirs = {}
    local monDirs = {}

    table.insert(resDirs, topDir)

    local fnames = self:getAllFiles(topDir)
    -- get all res-group
    for _, f in ipairs(fnames) do
        if not self:isSystemFile(f) then
            local path = topDir .. "/" .. f
            table.insert(resDirs, path)
            -- print("Add new res:" .. path)
        end
    end

    for _, resDir in ipairs(resDirs) do
        local monDataPath = resDir .. "/" .. "mon_data"
        self:addAllMonDirs(monDirs, monDataPath)

        local monGroupsPath = resDir .. "/" .. "mon_groups"
        local files = self:getAllFiles(monGroupsPath)
        for _, f in ipairs(files) do
            local path = monGroupsPath .. "/" .. f .. "/mon_data"
            self:addAllMonDirs(monDirs, path)
        end
    end

    if self._verbose then
        for _, value in ipairs(resDirs) do
            print("res group: " .. value)
        end
        for _, value in ipairs(monDirs) do
            print("mon group: " .. value)
        end
    end
    return resDirs, monDirs
end

function rdtMgr:addAllMonDirs(monDirs, path)
    local files = self:getAllFiles(path)
    for _, fname in ipairs(files) do
        local p = path .. "/" .. fname
        table.insert(monDirs, p)
        -- print("Add new mon: " .. path .. "/" .. fname)
    end
end

function rdtMgr:setupPlugins(proto, pffi, mnt)
    local plugins = {}
    for i, path in ipairs(self._resDirs) do
        -- print(string.format("rdt plugin[%d] path=%s", i, path));

        path = pystring:replace(path, self._mnt, "")
        for _, plugin in ipairs(self._resYaml.resctrl.resLuaPlugin) do
            local CProcs = require("collector.rdt.plugin." .. plugin)
            local lables = {
                {
                    name = "path",
                    index = path
                }
            }
            table.insert(plugins, CProcs.new(proto, pffi, mnt, path, lables))
        end
    end

    for i, path in ipairs(self._monDirs) do
        local lables
        -- print(string.format("mon plugin[%d] path=%s", i, path));
        path = pystring:replace(path, self._mnt, "")

        if pystring:find(path, "mon_groups") ~= nil then
            local podname = ''
            local conname = ''
            local items = pystring:split(path, '/')

            for index, n in ipairs(items) do
                if n == "mon_groups" then
                    local str = items[index + 1]
                    local ret = string.find(str, "#")

                    if ret == nil then
                        break
                    end

                    local strs = pystring:split(str, "#")
                    podname = strs[1]
                    conname = strs[2]
                    break
                end
            end

            lables = {
                {
                    name = "path",
                    index = path
                },
                {
                    name = "podname",
                    index = podname
                },
                {
                    name = "conname",
                    index = conname
                }
            }
        else
            lables = {
                {
                    name = "path",
                    index = path
                }
            }
        end
        for _, plugin in ipairs(self._resYaml.resctrl.monLuaPlugin) do
            local CProcs = require("collector.rdt.plugin." .. plugin)
            table.insert(plugins, CProcs.new(proto, pffi, mnt, path, lables))
        end
    end
    return plugins
end

function rdtMgr:getAllFiles(path)
    local res = {}

    local ok, files = pcall(dirent.files, path)
    if not ok then
        return res
    end

    for f in files do
        if not pystring:startswith(f, ".") then
            table.insert(res, f)
        end
    end
    return res
end

function rdtMgr:checkDirs()
    local resDirs = {}
    local monDirs = {}
    if self._resYaml.resctrl.group == nil then
        return resDirs, monDirs
    end
    for _, rdtGroup in pairs(self._resYaml.resctrl.group) do
        local resGrpName = rdtGroup.name
        local mons = rdtGroup.monitor

        local rdt_group_path = self._top .. "/" .. resGrpName
        if unistd.access(rdt_group_path) then
            table.insert(resDirs, pystring:join("/", { self._path, resGrpName }))
            local fnames = getAllFiles(rdt_group_path .. "/" .. "mon_data")
            for _, f in ipairs(fnames) do
                if resGrpName ~= "" then
                    table.insert(monDirs, pystring:join("/", { self._path, resGrpName, "mon_data", f }))
                else
                    table.insert(monDirs, pystring:join("/", { self._path, "mon_data", f }))
                end
            end
        else
            print(string.format("rdtManager: Non-exist path %s", self._top .. "/" .. resGrpName));
            goto continue
        end

        if mons == nil then
            goto continue
        end
        for _, monGroup in pairs(mons) do
            local relativePath = pystring:join("/", { resGrpName, "mon_groups", monGroup, "mon_data" })
            local absolutePath = self._top .. "/" .. relativePath

            if unistd.access(absolutePath) then
                local fnames = getAllFiles(absolutePath)
                for _, f in ipairs(fnames) do
                    table.insert(monDirs, pystring:join("/", { self._path, relativePath, f }))
                end
            else
                print(string.format("rdtManager: Non-exist path %s", absolutePath));
            end
        end
        ::continue::
    end
    return resDirs, monDirs
end

function rdtMgr:proc(elapsed, lines)
    if not self._enable then
        return
    end
    -- print(string.format("rdtMgr: proc"))
    for i, plugin in ipairs(self._plugins) do
        local stat, res = pcall(plugin.proc, plugin, elapsed, lines)
        if not stat or res == -1 then
            print("rdtManager: pcall plugin error.")
        end
    end
end

return rdtMgr

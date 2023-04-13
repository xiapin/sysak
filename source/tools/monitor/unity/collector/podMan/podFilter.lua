---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liaozhaoyan.
--- DateTime: 2023/3/28 11:27 PM
---

require("common.class")

local dirent = require("posix.dirent")
local unistd = require("posix.unistd")
local stat = require("posix.sys.stat")
local bit = require("bit")
local pystring = require("common.pystring")
local system = require("common.system")
local Cinotifies = require("common.inotifies")

local CpodFilter = class("podFiler")

local podFiler = "^kubepods%-*"
local dockerFilter = "^cri%-containerd%-*"

local function listSrc(path)
    local res = {}
    local ok, files = pcall(dirent.files, path)
    if not ok then
        return res
    end
    local c = 1
    for f in files do
        if not pystring:startswith(f, ".") then
            res[c] = f
            c = c + 1
        end
    end
    return res
end

local function addDirs(dirs, path)
    if not system:valueIsIn(dirs, path) then
        table.insert(dirs, path)
    end
end

local function setupPlugins(res, proto, pffi, mnt, dirs)
    local c = 0
    local plugins = {}

    for _, dir in ipairs(dirs) do
        local ls = {
            {
                name = "path",
                index = dir,
            }
        }
        for _, plugin in ipairs(res.container.luaPlugin) do
            local CProcs = require("collector.container." .. plugin)
            local plug = CProcs.new(proto, pffi, mnt, dir, ls)
            if plug.pFile and unistd.access(plug.pFile) then
                c = c + 1
                plugins[c] = plug
            end
        end
    end

    return plugins
end

function CpodFilter:_init_(resYaml, proto, pffi, mnt)
    local topDir = mnt .. "sys/fs/cgroup"
    if unistd.access(topDir) then
        self._top = topDir
    else
        error("podFilter: cannot visit top dir " .. topDir)
    end

    self._resYaml = resYaml
    self._proto = proto
    self._pffi = pffi
    self._mnt = mnt

    self._ino = Cinotifies.new()
    self._dirs = self:walkTops1(self._resYaml.container)
    self._plugins = setupPlugins(self._resYaml, self._proto, self._pffi, self._mnt, self._dirs)
    print("add " .. #self._plugins)
end

function CpodFilter:enum1LDirs(root, format, parent, dirs)
	local alldirs = listSrc(root)
	for _, file in ipairs(alldirs)
	do
		local destPath = root..'/'..file
		local destentry = parent..'/'..file
		if string.match('/'..file, format) then
			self._ino:add(destPath)
			addDirs(dirs, destentry)
		end
	end
	return dirs
end

function CpodFilter:walkTops1(resYaml)
	local cgroups = {"cpuacct", "memory", "blkio", "perf_event"}
	local dirs = system:deepcopy(resYaml.directCgPath)

	for i,cg in ipairs(cgroups) do
		for _, value in ipairs(resYaml.indirectCgPath1) do
			if nil == value.child1 then
				goto continue
			end
			local level1 = {}
			local root = self._top.."/"..cg
			self:enum1LDirs(root..value.path, value.child1, value.path, level1)
			for _, parent in ipairs(level1) do
				addDirs(dirs, parent)
				if nil ~= value.child2 then
					self:enum1LDirs(root..parent, value.child2, parent, dirs)
				end
			end
			::continue::
		end
	end
	return dirs
end

function CpodFilter:walkTops(resYaml)
    local topDirs = {"cpuacct", "memory", "blkio", "perf_event"}
    local dirs = system:deepcopy(resYaml.directCgPath)

    for _, top in ipairs(topDirs) do
        local top_s = pystring:join("/", {self._top, top})
        for _, filter in ipairs(resYaml.indirectCgPath) do
            local filter_s = pystring:join("/", {top_s, filter})
            local srcs = listSrc(filter_s)
            for _, src in ipairs(srcs) do
                if string.match(src, podFiler) then
                    addDirs(dirs, pystring:join("/", {filter, src}))
                    local pod_s = pystring:join("/", {filter_s, src})
                    self._ino:add(pod_s)
                    local dockers = listSrc(pod_s)
                    for _, docker in ipairs(dockers) do
                        if string.match(docker, dockerFilter) then
                            self._ino:add(pystring:join("/", {pod_s, docker}))
                            addDirs(dirs, pystring:join("/", {filter, src, docker}))
                        end
                    end
                end
            end
        end
    end
    return dirs
end

function CpodFilter:proc(elapsed, lines)
    local ret, delta
    local rec = {}
    if self._ino:isChange() then
        print("cgroup changed.")
        local start = lua_local_clock()
        self._ino = Cinotifies.new()
	for i, plugin in ipairs(self._plugins) do
		pcall(plugin._del_, plugin)
	end
        self._dirs = self:walkTops1(self._resYaml.container)
        self._plugins = setupPlugins(self._resYaml, self._proto, self._pffi, self._mnt, self._dirs)
        local stop = lua_local_clock()
        ret, delta = 1, stop - start
	return ret,delta
    end
    for i, plugin in ipairs(self._plugins) do
        --local res = plugin:proc(elapsed, lines)
        local stat, res = pcall(plugin.proc, plugin, elapsed, lines)
        if not stat or res == -1 then
            table.insert(rec, i)
        end
    end

    for _, i in ipairs(rec) do  -- del bad plugin
        self._plugins[i] = nil
    end
    return ret, delta
end

return CpodFilter

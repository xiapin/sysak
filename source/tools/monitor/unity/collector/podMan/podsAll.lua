require("common.class")

local CpodsAll = class("podsApi")

function CpodsAll:setupPlugins()
    local c = 0
    local plugins = {}

    local cons = self._runtime:setupCons()
    if cons == nil then
        return nil
    end

    -- 对于除podmem外的容器插件（都是每一个容器实例化一个新的插件对象）
    for _, con in ipairs(cons) do
        local ls = {}
        if con.pod then    -- k8s environment
            ls = {
                {
                    name = "pod",
                    index = con.pod.name,
                },
                {
                    name = "bvt",
                    index = con.bvt,
                },
                {
                    name = "container",
                    --index = con.name.."-"..string.sub(con.id,0,4),
                    index = con.name,
                },
                {
                    name = "namespace",
                    index = con.pod.namespace,
                },
            }
        else    -- container environment, no pod info
            ls = {
                {
                    name = "pod",
                    index = con.name,
                },
                {
                    name = "container",
                    index = con.name,
                },
                {
                    name = "namespace",
                    index = "",
                }
            }
        end

        for _, plugin in ipairs(self._resYaml.container.luaPlugin) do
            -- skip podmem. todo: 将podmem和其他插件如何统一起来。
            if plugin == "podmem" or plugin == "pod_storage_stat" then
                goto continue
            end
            local CProcs = require("collector.container." .. plugin)
            c = c + 1
            plugins[c] = CProcs.new(self._proto, self._pffi, self._mnt, con.path, ls)
            -- no need to watch pFile, too many pod/container will exhaust system's inofity watchs
            --ino:add(plugins[c].pFile)
            ::continue::
        end
    end

    -- 对于podmem，只更新podmem维护的信息，不实例化新的对象
    self._podmem:setup(cons)
    self._podStorageStat:setup(cons)

    return plugins
end

function CpodsAll:_init_(resYaml, proto, pffi, mnt)
    self._runtime = nil
    self._runtime_name = nil
    self._podmem = nil
    self._podStorageStat = nil
    self._resYaml = resYaml
    self._proto = proto
    self._pffi = pffi
    self._mnt = mnt

    --[[
        遍历yaml中所有容器运行时，只要其中某一个可用，则使用该运行时
        cri-containerd: 通过cri接口从containerd.sock获取pod和容器信息
        k8sApi: K8s环境，通过10250端口获得pod信息，包括k8s+containerd, k8s+docker, k8s+CRI-O)), 
        docker: 单节点非k8s(ecs)容器环境
        Todo: cri-docker, cri-o，添加时只需要按模版编写lua调用go暴露的cri接口即可
    ]]
    for _, runtime in ipairs(self._resYaml.container.runtime) do
        local runtime_module = require("collector.podMan.runtime." .. runtime)
        local Cruntime = runtime_module.new(self._resYaml, self._mnt)
        if Cruntime:checkRuntime() == 1 then
            self._runtime = Cruntime
            self._runtime_name = runtime
            break
        end
        print("Using "..runtime.." failed! Fall back to next runtime")
    end

    if not self._runtime then
        print("No supported runtime, container monitor is unavaliable!")
        return
    end

    -- 初始化inotify（cgroup变化）
    self._runtime:initInotify()
    -- podmem & podStorageStat 不需要每一轮实例化一个新的对象，先特殊处理
    for _, plugin in ipairs(self._resYaml.container.luaPlugin) do
        if plugin == "podmem" then
            local CprocsPodMem = require("collector.container."..plugin)
            self._podmem = CprocsPodMem.new(self._proto, self._pffi, self._mnt)
        end
        if plugin == "pod_storage_stat" then
            if self._runtime_name ~= "k8sApi" then
                print("pod_storage_stat will lose limit info not using k8sApi runtime!")
            end

            local CprocsPodStorageStat = require("collector.container."..plugin)
            self._podStorageStat = CprocsPodStorageStat.new(self._proto, self._pffi, self._mnt)
        end
    end

    self._plugins = self:setupPlugins()
    if not self._plugins then
        return
    end

    print( "pods plugin add " .. #self._plugins)
end

function CpodsAll:proc(elapsed, lines)
    local rec = {}
	local is_change

    if not self._runtime then
        return
    end

	is_change = self._runtime:cgroupChanged()
    if is_change or not self._plugins then
		self._plugins = self:setupPlugins()
        if not self._plugins then
            return
        end
    end

    -- run podmem
    if self._podmem then
        local stat, res = pcall(self._podmem.proc, self._podmem, elapsed, lines)
        if not stat then
            print("Podmem Error: ", res)
        end
    end

    -- run pod storage stat
    if self._podStorageStat then
        stat, res = pcall(self._podStorageStat.proc, self._podStorageStat,
                        elapsed, lines)
        if not stat then
            print("Pod Storage Stat Error: ", res)
        end
    end

    -- run other container plugins
    for i, plugin in ipairs(self._plugins) do
        --local res = plugin:proc(elapsed, lines)
        stat, res = pcall(plugin.proc, plugin, elapsed, lines)
        if not stat or res == -1 then
	        table.insert(rec, i)
        end
    end

    --[[
        容器删除后，对应的plugin会执行失败，删除对应的plugins，inoity同时也会
        识别到cgroup路径的变化，重新为现存的容器重新实例化对应的plugins。
        （倒序删除元素，确保删除元素不会影响后续元素的索引）
    ]]
    for i = #rec, 1, -1 do
        table.remove(self._plugins, rec[i])
    end

end

return CpodsAll

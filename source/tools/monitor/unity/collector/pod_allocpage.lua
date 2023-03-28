---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liuxinwnei.
--- DateTime: 2023/02/08 17:00 PM
---

require("common.class")
local fcntl = require("posix.fcntl")
local unistd = require("posix.unistd")
local dirent = require("posix.dirent")
local stdlib = require("posix.stdlib")
local stat = require("posix.sys.stat")
local CkvProc = require("collector.kvProc")
local CvProc = require("collector.vproc")
local pystring = require("common.pystring")
local dockerinfo = require("common.dockerinfo")

local CPodAlloc = class("podalloc", CkvProc)

function CPodAlloc:_init_(proto, pffi, mnt, pFile)
    CkvProc._init_(self, proto, pffi, mnt, pFile , "pod_alloc")
    self._ffi = require("collector.native.plugincffi")
    self.root_fs = mnt
    self.proc_fs = mnt .. "/proc/"
    self.name_space = {}
    self.pod_mem = {}
    self.total = 0
end

function CPodAlloc:file_exists(file)
    local f=stat.lstat(file)
    if f ~= nil then
        return true
    else
        return false
    end
end

function CPodAlloc:switch_ns(pid)
    local pid_ns =  self.proc_fs .. pid .. "/ns/net"
    if not self:file_exists(pid_ns) then return end

    local f = fcntl.open(pid_ns,fcntl.O_RDONLY)
    if f == nil then return false end
    local res = self._ffi.C.setns(f,0)
    if res == -1 then return false end
    unistd.close(f)
    return true
end

function CPodAlloc:get_container_info(did)
    local restable = {}
    local podname = did
    local podns = did
    local cname = did
    
    restable = dockerinfo:get_inspect(did, self.root_fs)
    if not restable then return podname end
    if #restable > 0 then
        restable = restable[1]
    end
    if restable['Config'] then
        local config = restable['Config']
        if config['Labels'] then
            local label = config['Labels']
            if label['io.kubernetes.pod.name'] then
                podname = label['io.kubernetes.pod.name']
            end
            if label['io.kubernetes.container.name'] then
                cname = label['io.kubernetes.container.name']
            end
            if label['io.kubernetes.pod.namespace'] then
                podns = label['io.kubernetes.pod.namespace']
            end
        end
        if podname == did and restable['Name'] then
            cname = restable['Name']
            podname = restable['Name']
        end
    elseif restable['status'] then
        podname = restable['status']['labels']['io.kubernetes.pod.name']
        cname = restable['status']['labels']['io.kubernetes.container.name']
        podns = restable['status']['labels']['io.kubernetes.pod.namespace']
    end
    if pystring:startswith(podname,"/") then podname=string.sub(podname,2,-1) end
    if not self.pod_mem[podname] then
        self.pod_mem[podname] = {}
        self.pod_mem[podname]["allocpage"] = 0
        self.pod_mem[podname]["podns"] = podns
        self.pod_mem[podname]["podname"] = podname
    end
    return podname
end

function CPodAlloc:get_pidalloc()
    local pods = {}
    local dockerids = {}
    for net,pidn in pairs(self.name_space) do
        if pidn == "self" then pidn = "1" end

        local ns_res = self:switch_ns(pidn)
        if not ns_res then goto continue end 
        -- local env = posix.getenv()
        -- env["PROC_ROOT"] = self.proc_fs

        stdlib.setenv("PROC_ROOT",self.proc_fs)
        local pfile = io.popen("ss -anp","r")
        io.input(pfile)
        for line in io.lines() do
            repeat
            local proto,recv,task,pid = string.match(line,"(%S*)%s*%S*%s*(%d*).*users:%S*\"(%S*)\",pid=(%d*)")
            if not proto or not recv or not task or not pid then break end
            if proto ~="tcp" and proto ~="udp" and proto ~="raw" then break end

            recv = tonumber(recv)

            local dockerid = ""
            if not dockerids[pid] then
                dockerid = dockerinfo:get_dockerid(pid, self.root_fs)
                if dockerid == "unknow" then break end
                dockerids[pid] = dockerid
            else
                dockerid = dockerids[pid]
            end

            local podname = dockerid
            if not pods[dockerid] then
                podname = self:get_container_info(dockerid)
                pods[dockerid] = podname
            else
                podname = pods[dockerid]
            end
            if recv < 1024 and podname == dockerid then break end

            if not self.pod_mem[podname] then
                self.pod_mem[podname] = {}
                self.pod_mem[podname]["allocpage"] = 0
                self.pod_mem[podname]["podns"] = podname
                self.pod_mem[podname]["podname"] = podname
            end
            self.pod_mem[podname]["allocpage"] = self.pod_mem[podname]["allocpage"] + recv
            self.total = self.total + recv
        until true
        end
        pfile:close()
        self:switch_ns("1")
        stdlib.setenv("PROC_ROOT","")
        ::continue::
    end
end

function CPodAlloc:scan_namespace()
    local root = self.proc_fs
    for pid in dirent.files(root) do
        repeat
        if pystring:startswith(pid,".") then break end
        if not self:file_exists(self.proc_fs .. pid .. "/comm") then break end

        local proc_ns = self.proc_fs .. pid .. "/ns/net"
        if not self:file_exists(proc_ns) then break end

        local slink = unistd.readlink(proc_ns)
        if not slink then break end
        if not string.find(slink,"net") then break end

        local inode = string.match(slink,"%[(%S+)%]")
        if not inode then break end

        if not self.name_space[inode] then self.name_space[inode] = pystring:strip(pid) end
        if not self:file_exists(root .. self.name_space[inode] .. "/comm") then self.name_space[inode] = pystring:strip(pid) end
    until true
    end
end

function CPodAlloc:proc(elapsed, lines)
    CvProc.proc(self)
    self.name_space = {}
    self.pod_mem = {}
    self.total = 0
    self:scan_namespace()
    self:get_pidalloc()

    for k,v in pairs(self.pod_mem) do
        local cell = {{name="pod_allocpage", value=v['allocpage']/1024}}
        local label = {{name="podname",index=v['podname'],}, {name="namespace",index = v['podns'],},}
        self:appendLine(self:_packProto("pod_alloc", label, cell))
    end
    local cell = {{name="pod_allocpage_total", value=self.total/1024}}
    self:appendLine(self:_packProto("pod_alloc", nil, cell))

    self:push(lines)
end

return CPodAlloc

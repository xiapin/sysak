---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liuxinwnei.
--- DateTime: 2023/05/29 17:00 PM
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
local json = require("cjson")
local CPodMem = class("podmem", CkvProc)
local ChttpCli = require("httplib.httpCli")
local podman = require("collector.podMan.podsAll")

function CPodMem:_init_(proto, pffi, mnt, pFile)
    CkvProc._init_(self, proto, pffi, mnt, pFile , "podmem")
    self._ffi = require("collector.native.plugincffi")
    self.cffi = self._ffi.load("podmem")
    self.root_fs = mnt
    self.proc_fs = mnt .. "/proc/"
    self.allcons = {}
    self.inodes = {}
    self.podmemres = {}
    self.blacklist = {["arms-prom"] = 1, ["kube-system"] = 1, ["kube-public"] = 1, ["kube-node-lease"] = 1}
    -- self.blacklist= {}
    self.cffi.monitor_init(self.root_fs)
end

-- function CPodMem:_del_()
--     self.cffi.monitor_exit()
-- end

function CPodMem:get_inode(file)
    local f=stat.lstat(file)
    if f ~= nil then
        return f["st_ino"] 
    else
        return -1
    end
end


function CPodMem:get_allcons()
    local cri_cons = podman:getAllcons(self.root_fs)
    if cri_cons == nil then
        return -1
    end
    for _,v in pairs(cri_cons) do
        if not self.blacklist[v['pod']['namespace']] then
            local path =  self.root_fs .. "/sys/fs/cgroup/memory/" .. v['path']
            local inode = self:get_inode(path)
            self.allcons[v['name']..v['pod']['name']] = {["pod"]=v['pod']['name'], ["ns"]=v['pod']['namespace'], ["path"]=path,  ["inode"] = inode, ["cname"]=v['name']}
            self.inodes[inode] = v['name']..v['pod']['name']
        end
    end
    return 0
end

function CPodMem:proc(elapsed, lines)
    CvProc.proc(self)
    self.allcons = {}
    self.inodes = {}
    self.podmemres = {}
    local res = self:get_allcons()
    if res == -1 then
        return
    end
    -- for k,v in pairs(self.inodes) do print(k,v) end
    -- for k,v in pairs(self.allcons) do for k1,v1 in pairs(v) do print(k,k1,v1) end end
    local fs = io.open("/tmp/.memcg.txt", "w")
    for k,v in pairs(self.allcons) do
        fs:write(v['path'])
        fs:write("\n")
    end
    fs:close()

    local resptr = self.cffi.scanall()
    if not resptr then return end

    local res = self._ffi.string(resptr)

    local reslines = pystring:splitlines(res)
    for _,line in pairs(reslines) do
        local cinode, cache, size, filen = line:match("cinode=(%d+) cached=(%d+) size=(%d+) file=(%S+)") 
        if self.inodes[tonumber(cinode)] ~= nil then 
            if filen:find("overlayfs/snapshots/%d+/fs") ~= nil then 
                filen = pystring:split(pystring:split(filen,"overlayfs/snapshots/")[2],"/fs/")[2] 
                filen = "/" .. filen
                end 
            if filen:find("diff") ~= nil then filen = pystring:split(filen,"diff")[2] end 
            local cname = self.inodes[tonumber(cinode)]
            if not self.podmemres[cname] then self.podmemres[cname]= {} end
            self.podmemres[cname][filen] = {["pod"]=self.allcons[cname]['pod'], ["ns"]=self.allcons[cname]['ns'], ["size"]=size, ["cache"]=cache, ["cname"]=self.allcons[cname]['cname']}
        end
    end
    self._ffi.C.free(resptr)

    for k,v in pairs(self.podmemres) do
        for k1,v1 in pairs(v) do 
            -- for k2,v2 in pairs(v1) do print(k,k1,k2,v2) end
            local cell = {{name="size", value=tonumber(v1['size'])}}
            local label = {{name="pod",index=v1['pod'],}, {name="namespace",index = v1['ns'],},{name="file",index = k1,},{name="container", index=v1['cname']},}
            self:appendLine(self:_packProto("podmem", label, cell))
            cell = {{name="cached", value=tonumber(v1['cache'])}}
            label = {{name="pod",index=v1['pod'],}, {name="namespace",index = v1['ns'],},{name="file",index = k1,},{name="container", index=v1['cname']},}
            self:appendLine(self:_packProto("podmem", label, cell))
        end
    end
    self:push(lines)
end
return CPodMem

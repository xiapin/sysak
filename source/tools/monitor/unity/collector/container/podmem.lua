---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liuxinwnei.
--- DateTime: 2023/05/29 17:00 PM
---

require("common.class")
local stat = require("posix.sys.stat")
local CkvProc = require("collector.kvProc")
local CvProc = require("collector.vproc")
local pystring = require("common.pystring")
local CPodMem = class("podmem", CkvProc)
--local podman = require("collector.podMan.podsAll")

function CPodMem:_init_(proto, pffi, mnt)
    CkvProc._init_(self, proto, pffi, mnt, nil, "podmem")
    self._ffi = require("collector.native.plugincffi")
    self.cffi = self._ffi.load("podmem")
    self.root_fs = mnt
    self.proc_fs = mnt .. "/proc/"
    self.allcons = {}
    self.inodes = {}
    self.is_pod = 0
    self.podmemres = {}
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

function CPodMem:setup(cons)
    -- reset the data struct first
    self.allcons = {}
    self.inodes = {}

    -- setup root cgroup
    local root_name = "/"
    local root_path = self.root_fs .. "sys/fs/cgroup/memory"
    local root_inode = self:get_inode(root_path)
    if root_inode == -1 then
        print("Get root cgroup path inode failed: ", root_path)
        return
    end
    self.allcons[root_name] = {["path"]=root_path, ["pod"]=root_name, ["ns"]="", ["inode"]=root_inode, ["cname"]=root_name}
    self.inodes[root_inode] = root_name

     -- setup self.allcons and self.inodes
    for _, v in pairs(cons) do
        local path =  self.root_fs .. "sys/fs/cgroup/memory" .. v['path']
        local inode = self:get_inode(path)
        if inode == -1 then
            print("Get cgroup path inode failed: ", path)
            goto continue
        end
        if v.pod then  -- is pod
            self.is_pod = 1
            self.allcons[v['name']..v['pod']['name']] = {["pod"]=v['pod']['name'], ["ns"]=v['pod']['namespace'], ["path"]=path,  ["inode"] = inode, ["cname"]=v['name']}
            self.inodes[inode] = v['name']..v['pod']['name']
        else -- is container
            self.allcons[v['name']] = {["path"]=path, ["inode"]=inode, ["cname"]=v['name']}
            self.inodes[inode] = v['name']
        end
        ::continue::
    end
end

function CPodMem:proc(elapsed, lines)
    CvProc.proc(self)
    self.podmemres = {}

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

    if self.is_pod == 1 then -- is pod
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
    else -- is container
        for _,line in pairs(reslines) do
            local cinode, cache, size, filen = line:match("cinode=(%d+) cached=(%d+) size=(%d+) file=(%S+)")
            -- todo: 显示容器内文件名
            if self.inodes[tonumber(cinode)] ~= nil then
                local cname = self.inodes[tonumber(cinode)]
                local cell = {{name = "size", value = tonumber(size)}}
                local label = {{name="pod",index=cname}, {name="namespace",index = "",},
                        {name = "container", index = cname}, {name = "file", index = filen}}
                self:appendLine(self:_packProto("podmem", label, cell))
                cell = {{name = "cached", value = tonumber(cache)}}
                self:appendLine(self:_packProto("podmem", label, cell))
            end
        end
        self._ffi.C.free(resptr)
        self:push(lines)
    end
end
return CPodMem
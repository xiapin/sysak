---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liaozhaoyan.
--- DateTime: 2022/12/16 11:36 PM
---

require("common.class")
local CkvProc = require("collector.kvProc")
local CvProc = require("collector.vproc")
local pystring = require("common.pystring")

local CprocMeminfo = class("proc_meminfo", CkvProc)

function CprocMeminfo:_init_(proto, pffi, mnt, pFile)
    CkvProc._init_(self, proto, pffi, mnt,pFile or "proc/meminfo", "meminfo")
    self._protoTable_dict = {
        line = self._protoTable.line,
        ls = nil,
        vs = {}
    }
    self:readIomem()
end

function CprocMeminfo:readIomem()
    local reserved = 0
    for line in io.lines("/proc/iomem") do
        if string.find(line,"Reserved") or string.find(line,"Crash kernel") or string.find(line,"Kernel code") or string.find(line,"Kernel data") or string.find(line,"Kernel bss") then
            local cells = pystring:split(pystring:split(pystring:strip(line)," ",1)[1],"-",1)
            reserved = reserved + (tonumber(cells[2], 16)-tonumber(cells[1], 16))
        end
    end
    self._protoTable_dict["vs"]["res"], _ = math.modf(reserved/1024)
end

function CprocMeminfo:readVmalloc()
    local pages = 0
    --for line in io.lines("/proc/vmallocinfo") do
     --   if string.find(line,"vmalloc") and string.find(line,"pages=") then
      --      local cells = pystring:split(pystring:split(pystring:strip(line),"pages=",1)[2]," ",1)
       --     pages = pages + tonumber(cells[1])
        --end
    --end
    self._protoTable_dict["vs"]["VmallocUsed"]=pages * 4
end

--[[
function CprocMeminfo:readUsed()
    local f = io.popen('free -k','r')
    io.input(f)
    for line in io.lines() do
        if string.find(line,'Mem') then
            local used = tonumber(pystring:split(pystring:lstrip(pystring:split(pystring:lstrip(pystring:split(pystring:strip(line),':',1)[2]),' ',1)[2]),' ',1)[1])
            self._protoTable_dict["vs"]["free_used"]=used
            break
        end
    end
    f:close()
end
]]

function CprocMeminfo:readHugepage(size,name)
    local file = "/sys/kernel/mm/hugepages/hugepages-" .. size .. "kB/nr_hugepages"
    local f=io.open(file,"r")

    if f ~= nil then
        local pages = tonumber(f:read("*a"))
        io.close(f)
        self._protoTable_dict["vs"][name]=pages * size
    end
end

function CprocMeminfo:readKV(line)
    local data = self._ffi.new("var_kvs_t")
    assert(self._cffi.var_input_kvs(self._ffi.string(line), data) == 0)
    assert(data.no >= 1)

    local name = self._ffi.string(data.s)
    name = self:checkTitle(name)
    local value = tonumber(data.value[0])

    local cell = {name=name, value=value}
    self._protoTable_dict["vs"][name]=value
    table.insert(self._protoTable["vs"], cell)
end

function CprocMeminfo:proc(elapsed, lines)
    self._protoTable.vs = {}
    CvProc.proc(self)
    for line in io.lines(self.pFile) do
        self:readKV(line)
    end
    local tmp_dict = self._protoTable_dict.vs
    self:readVmalloc()
    --self:readUsed()
    self:readHugepage(2048,"huge_2M")
    self:readHugepage(1048576,"huge_1G")

    local cell = {name="total", value=tmp_dict["MemTotal"]+tmp_dict["res"]}
    table.insert(self._protoTable["vs"], cell)

    --cell = {name="used", value=tmp_dict["free_used"]+tmp_dict["Shmem"]}
    --table.insert(self._protoTable["vs"], cell)

    local used = tmp_dict["MemTotal"] - tmp_dict["MemFree"] - tmp_dict["Cached"] - tmp_dict["Buffers"] - tmp_dict["SReclaimable"] + tmp_dict["Shmem"]
    cell = {name="used", value = used }
    table.insert(self._protoTable["vs"], cell)

    local kernel_other = tmp_dict["Slab"]+tmp_dict["KernelStack"]+tmp_dict["PageTables"]+tmp_dict["VmallocUsed"]
    cell = {name="kernel_other", value=kernel_other}
    table.insert(self._protoTable["vs"], cell)

    local user = tmp_dict["Active_anon"]+tmp_dict["Active_file"]+tmp_dict["Mlocked"]+tmp_dict["Inactive_file"]+tmp_dict["Inactive_anon"]
    if tmp_dict['huge_2M'] then
        user = user + tmp_dict['huge_2M']
    end
    if tmp_dict['huge_1G'] then
        user = user + tmp_dict['huge_1G']
    end
    cell = {name="user_used", value=user}
    table.insert(self._protoTable["vs"], cell)

    local page_used = tmp_dict["MemTotal"] - tmp_dict["MemFree"] - user - kernel_other
    if page_used < 1 then
        page_used=1024
    end
    self._protoTable_dict["vs"]["alloc_page"]=page_used
    cell = {name="alloc_page", value=page_used}
    table.insert(self._protoTable["vs"], cell)

    cell = {name="kernel_used", value=page_used+kernel_other+tmp_dict["res"]}
    table.insert(self._protoTable["vs"], cell)

    cell = {name="user_anon", value=tmp_dict["Active_anon"]+tmp_dict["Inactive_anon"]}
    table.insert(self._protoTable["vs"], cell)

    cell = {name="user_filecache", value=tmp_dict["Cached"]-tmp_dict["Shmem"]}
    table.insert(self._protoTable["vs"], cell)

    cell = {name="user_buffers", value=tmp_dict["Buffers"]}
    table.insert(self._protoTable["vs"], cell)

    cell = {name="user_mlock", value=tmp_dict["Mlocked"]}
    table.insert(self._protoTable["vs"], cell)

    cell = {name="user_shmem", value=tmp_dict["Shmem"]}
    table.insert(self._protoTable["vs"], cell)

    if tmp_dict['huge_2M'] then
        cell = {name="user_huge_2M", value=tmp_dict["huge_2M"]}
        table.insert(self._protoTable["vs"], cell)
    end

    if tmp_dict['huge_1G'] then
        cell = {name="user_huge_1G", value=tmp_dict["huge_1G"]}
        table.insert(self._protoTable["vs"], cell)
    end

    cell = {name="kernel_reserved", value=tmp_dict["res"]}
    table.insert(self._protoTable["vs"], cell)

    cell = {name="kernel_vmalloc", value=tmp_dict["VmallocUsed"]}
    table.insert(self._protoTable["vs"], cell)

    self:appendLine(self._protoTable)
    self:push(lines)

end

return CprocMeminfo

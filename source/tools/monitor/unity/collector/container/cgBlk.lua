require("common.class")
local pystring = require("common.pystring")
local unistd = require("posix.unistd")
local CvProc = require("collector.vproc")

local CgBlk = class("cgBlk", CvProc)

function CgBlk:_init_(proto, pffi, mnt, pFile, ls)
    CvProc._init_(self, proto, pffi, mnt, pFile)
    self.mnt = mnt
    self.ls = ls
end

-- get device name form (major, minor)
function CgBlk:devNoToName(devNo)
    local blkFile = self.mnt .. "sys/dev/block/" .. devNo

    if unistd.access(blkFile) == 0 then
        for line in io.lines(blkFile .. "/uevent" ) do
            if pystring:startswith(line, "DEVNAME") then 
                local cell = pystring:split(line, "=")
                return "/dev/" .. cell[2]
            end
        end
    end
    return devNo
end

function CgBlk:copyTable(original)
    local copy = {}
    for key, value in pairs(original) do
        copy[key] = value
    end
    return copy
end

function CgBlk:_proc(line, metrics, tableName)
    local vs = {}
    local label = self:copyTable(self.ls)
    local cells = pystring:split(line)
    -- skip last line
    if #cells == 2 then goto exit end

    local op = cells[2]
    local devName = self:devNoToName(cells[1])
    table.insert(label, {name = "device", index = devName})
    
    if op == "Read" then 
        vs= {{
            name = "reads_" .. metrics,
            value = tonumber(cells[3])
        }}
    elseif op == "Write" then
        vs = {{
            name = "writes_" .. metrics,
            value = tonumber(cells[3])
        }}
    elseif op == "Total" then
        vs= {{
            name = "total_" .. metrics,
            value = tonumber(cells[3])
        }}
    else
        goto exit
    end

    self:appendLine(self:_packProto(tableName, label , vs))
    ::exit::
end 

function CgBlk:proc(elapsed)
    CvProc.proc(self)
end

return CgBlk
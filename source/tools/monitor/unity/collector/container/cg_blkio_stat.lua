require("common.class")
local pystring = require("common.pystring")
local unistd = require("posix.unistd")
local CvProc = require("collector.vproc")

local root = "sys/fs/cgroup/blkio/"
local blkMetrics = {
    {
        path = "/blkio.throttle.io_service_bytes", 
        metrics = "service_bytes"
    },
    {
        path = "/blkio.throttle.io_serviced",
        metrics = "serviced" 
    },
    {
        path = "//blkio.throttle.total_bytes_queued",
        metrics = "bytes_queued"
    },
    {
        path = "/blkio.throttle.total_io_queued",
        metrics = "io_queued"
    },
    {
        path = "/blkio.throttle.io_wait_time",
        metrics = "wait_time"
    }
}

local CgBlkIoStat = class("cg_blkio_stat", CvProc) 

function CgBlkIoStat:_init_(proto, pffi, mnt, path, ls)
    CvProc._init_(self, proto, pffi, mnt, nil)
    self.mnt = mnt
    self.ls = ls
    self.path = path
end

-- get device name form (major, minor)
function CgBlkIoStat:devNoToName(devNo)
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

function CgBlkIoStat:copyTable(original)
    local copy = {}
    for key, value in pairs(original) do
        copy[key] = value
    end
    return copy
end

function CgBlkIoStat:_proc(line, metrics, tableName)
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

function CgBlkIoStat:proc(elapsed, lines)
    local tableName = "cg_blkio_stat"
    CvProc.proc(self)

    for _, blk_metrics in ipairs(blkMetrics) do
        local cg_path = self.mnt .. root .. self.path .. blk_metrics.path
        for line in io.lines(cg_path) do
            self:_proc(line, blk_metrics.metrics, tableName)
        end
    end

    self:push(lines)
end

return CgBlkIoStat

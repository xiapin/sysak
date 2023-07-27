require("common.class")
local CgBlk = require("collector.container.cgBlk")
local root = "sys/fs/cgroup/blkio/"
local dfile = "/blkio.throttle.io_serviced"

local CgBlkIoServiced = class("cg_blkio_io_serviced", CgBlk)

function CgBlkIoServiced:_init_(proto, pffi, mnt, path, ls)
    CgBlk._init_(self, proto, pffi, mnt, root .. path .. dfile, ls)
end

function CgBlkIoServiced:proc(elapsed, lines)
    local tableName = "cg_blkio_io_serviced"
    local metrics = "serviced"
    CgBlk.proc(self)

    for line in io.lines(self.pFile) do
        self:_proc(line, metrics, tableName)
    end
    --printTable(self._line)
    self:push(lines)
end

return CgBlkIoServiced
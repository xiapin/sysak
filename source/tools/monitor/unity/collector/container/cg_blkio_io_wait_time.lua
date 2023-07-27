require("common.class")
local CgBlk = require("collector.container.cgBlk")
local root = "sys/fs/cgroup/blkio/"
local dfile = "/blkio.throttle.io_wait_time"

local CgBlkIoWait = class("cg_blkio_io_wait_time", CgBlk)

function CgBlkIoWait:_init_(proto, pffi, mnt, path, ls)
    CgBlk._init_(self, proto, pffi, mnt, root .. path .. dfile, ls)
end

function CgBlkIoWait:proc(elapsed, lines)
    local tableName = "cg_blkio_io_wait_time"
    local metrics = "wait_time"
    CgBlk.proc(self)

    for line in io.lines(self.pFile) do
        self:_proc(line, metrics, tableName)
    end
    --printTable(self._line)
    self:push(lines)
end

return CgBlkIoWait
require("common.class")
local CgBlk = require("collector.container.cgBlk")
local root = "sys/fs/cgroup/blkio/"
local dfile = "/blkio.throttle.total_io_queued"

local CgBlkIoQueued = class("cg_blkio_io_queued", CgBlk)

function CgBlkIoQueued:_init_(proto, pffi, mnt, path, ls)
    CgBlk._init_(self, proto, pffi, mnt, root .. path .. dfile, ls)
end

function CgBlkIoQueued:proc(elapsed, lines)
    local tableName = "cg_blkio_io_queued"
    local metrics = "io_queued"
    CgBlk.proc(self)

    for line in io.lines(self.pFile) do
        self:_proc(line, metrics, tableName)
    end
    --printTable(self._line)
    self:push(lines)
end

return CgBlkIoQueued
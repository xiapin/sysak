require("common.class")
local CgBlk = require("collector.container.cgBlk")
local root = "sys/fs/cgroup/blkio/"
local dfile = "/blkio.throttle.io_service_bytes"

local CgBlkIoServiceBytes = class("cg_blkio_io_service_bytes", CgBlk)

function CgBlkIoServiceBytes:_init_(proto, pffi, mnt, path, ls)
    CgBlk._init_(self, proto, pffi, mnt, root .. path .. dfile, ls)
end

function CgBlkIoServiceBytes:proc(elapsed, lines)
    local tableName = "cg_blkio_io_service_bytes"
    local metrics = "service_bytes"
    CgBlk.proc(self)

    for line in io.lines(self.pFile) do
        self:_proc(line, metrics, tableName)
    end
    --printTable(self._line)
    self:push(lines)
end

return CgBlkIoServiceBytes
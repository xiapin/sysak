require("common.class")
local CgBlk = require("collector.container.cgBlk")
local root = "sys/fs/cgroup/blkio/"
local dfile = "/blkio.throttle.total_bytes_queued"

local CgBlkBytesQueued = class("cg_blkio_bytes_queued", CgBlk)

function CgBlkBytesQueued:_init_(proto, pffi, mnt, path, ls)
    CgBlk._init_(self, proto, pffi, mnt, root .. path .. dfile, ls)
end

function CgBlkBytesQueued:proc(elapsed, lines)
    local tableName = "cg_blkio_bytes_queued"
    local metrics = "bytes_queued"
    CgBlk.proc(self)

    for line in io.lines(self.pFile) do
        self:_proc(line, metrics, tableName)
    end
    --printTable(self._line)
    self:push(lines)
end

return CgBlkBytesQueued
require("common.class")
local pystring = require("common.pystring")
local CvProc = require("collector.vproc")

local CprocFd = class("procFd", CvProc)

function CprocFd:_init_(proto, pffi, mnt, pFile)
    CvProc._init_(self, proto, pffi, mnt, pFile or "proc/sys/fs/file-nr")
end

function CprocFd:proc(elapsed, lines)
    local c = 0
    CvProc.proc(self)
    local values = {}

    local file = io.open(self.pFile, "r")
    if file then
        local line = file:read("*l")
        if line then
            local cells = pystring:split(line)
            if cells[1] then
                local ls
                values = {
                    name = "file-nr",
                    value = cells[1],
                }
                self:appendLine(self:_packProto("procfd", nil, {values}))
            end

            if cells[3] then
                values = {
                    name = "file-max",
                    value = cells[3],
                }
                self:appendLine(self:_packProto("procfd", nil, {values}))
            end
        end

        file:close()
    end

    self:push(lines)
end

return CprocFd

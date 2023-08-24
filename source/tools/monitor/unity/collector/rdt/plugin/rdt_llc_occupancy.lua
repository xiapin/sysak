require("common.class")
local pystring = require("common.pystring")
local system = require("common.system")
local unistd = require("posix.unistd")
local CvProc = require("collector.vproc")
local rdt_llc_occupancy = class("rdt_llc_occupancy", CvProc)

function rdt_llc_occupancy:_init_(proto, pffi, mnt, path, labels)
    CvProc._init_(self, proto, pffi, mnt, path .. "/" .. "llc_occupancy")
    self.labels = labels
end

function rdt_llc_occupancy:proc(elapsed, lines)
    if not unistd.access(self.pFile) then
        return
    end

    CvProc.proc(self)
    local values = {}

    for line in io.lines(self.pFile) do
        local v = {
            name = "llc_occ",
            value = tonumber(line)
        }
        table.insert(values, v)

        self:appendLine(self:_packProto("rdt_usage", self.labels, values))
    end

    self:push(lines)
end

return rdt_llc_occupancy

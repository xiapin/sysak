require("common.class")
local pystring = require("common.pystring")
local system = require("common.system")
local unistd = require("posix.unistd")
local CvProc = require("collector.vproc")
local rdt_local_mem_bw = class("rdt_local_mem_bw", CvProc)
local rdtffi = require("collector.native.ffi_rdt_helper")

function rdt_local_mem_bw:_init_(proto, pffi, mnt, path, labels)
    CvProc._init_(self, proto, pffi, mnt, path .. "/" .. "mbm_local_bytes")
    self._labels = labels
    self._prev   = nil
    self._ffi    = rdtffi["rawffi"]
    self._rdtffi = rdtffi["rdtffi"]
end

function rdt_local_mem_bw:proc(elapsed, lines)
    if not unistd.access(self.pFile) then
        return
    end

    CvProc.proc(self)
    local values = {}
    
    for line in io.lines(self.pFile) do
        local v = {
            name = "local_mem_bw",
            value = self._rdtffi.calculate(line, self._prev or "0")
        }

        table.insert(values, v)
        self:appendLine(self:_packProto("rdt_usage", self._labels, values))
        self._prev = line
    end

    self:push(lines)
end

return rdt_local_mem_bw

require("common.class")
local pystring = require("common.pystring")
local system = require("common.system")
local CvProc = require("collector.vproc")

local cpu_root = "sys/fs/cgroup/cpu/"
local procs_file = "/cgroup.procs"
local proc = "proc/"
local net_dev = "/net/dev"

local ConNetStat = class("con_net_stat", CvProc)

function ConNetStat:_init_(proto, pffi, mnt, path, ls)
    CvProc._init_(self, proto, pffi, mnt, cpu_root..path..procs_file)
    self.ls = ls
    self.mnt = mnt
    self.procnetpath = ""
    self.init_pid = ""

end

--- open cgroup.procs to get the first pid
function ConNetStat:_getInitPid_()
    local pfile = io.open(self.pFile, "r")
    local firstline = pfile:read("*line")
    self.init_pid = firstline
    io.close(pfile)
end

--- procnetpath = "/proc/self/net/dev"
function ConNetStat:_getProcNetPath_()
    self:_getInitPid_()
    self.procnetpath = self.mnt..proc..self.init_pid..net_dev
end

local function isIgnoredDevice(devname)
    local ignoredDevicePrefixes = {"lo", "veth", "docker"}
    for _, value in ipairs(ignoredDevicePrefixes) do
        if pystring:startswith(devname, value) then
            return true
        end
    end
    return false
end

function ConNetStat:proc(elapsed, lines)
    local c = 1
    local k = 1
    local devName
    local RxBytes, RxPackets = 0, 0
    local TxBytes, TxPackets = 0, 0
    local RxPacketsDrop, TxPacketsDrop = 0, 0
    local values = {}
    CvProc.proc(self)
    self:_getProcNetPath_()

    for line in io.lines(self.procnetpath) do
        local cell
        local new_line = line

        --- skip first two lines
        if c < 3 then
            c = c + 1
            goto continue
        end

        new_line = pystring:replace(new_line, ":", "")
        --- "eth0" and "lo" may start with " "
        new_line = pystring:lstrip(new_line)
        cell = pystring:split(new_line)
        if #cell ~= 17 then
            print("invalid interface stats line " .. new_line)
            goto continue
        end

        devName = cell[1]
        --- ignore lo and veth interface
        if isIgnoredDevice(devName) then
            goto continue
        end

        RxBytes = RxBytes + tonumber(cell[2])
        RxPackets = RxPackets + tonumber(cell[3])
        RxPacketsDrop = RxPacketsDrop + tonumber(cell[5])
        TxBytes = TxBytes + tonumber(cell[10])
        TxPackets = TxPackets + tonumber(cell[11])
        TxPacketsDrop = TxPacketsDrop + tonumber(cell[13])

        ::continue::
    end

    values[k] = {
        name = "net_rx_bytes",
        value = RxBytes
    }
    values[k + 1] = {
        name = "net_rx_packets",
        value = RxPackets
    }
    values[k + 2] = {
        name = "net_rx_dropped",
        value = RxPacketsDrop
    }
    values[k + 3] = {
        name = "net_tx_bytes",
        value = TxBytes
    }
    values[k + 4] = {
        name = "net_tx_packets",
        value = TxPackets
    }
    values[k + 5] = {
        name = "net_tx_dropped",
        value = TxPacketsDrop
    }

    self:appendLine(self:_packProto("con_net_stat", self.ls, values))
    self:push(lines)
end

return ConNetStat
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

local function copyTable(original)
    local copy = {}
    for key, value in pairs(original) do
        copy[key] = value
    end
    return copy
end

function ConNetStat:proc(elapsed, lines)
    local c = 1
    local k = 1
    local local_ls = copyTable(self.ls)
    local devName
    local RxBytes, RxPackets = 0, 0
    local TxBytes, TxPackets = 0, 0
    local values = {}
    CvProc.proc(self)
    self:_getProcNetPath_()
    table.insert(local_ls, {name = "interface", index = ""})

    for line in io.lines(self.procnetpath) do
        local cell

        --- skip first two lines
        if c < 3 then
            c = c + 1
            goto continue
        end
    
        line = pystring:replace(line, ":", "")
        --- "eth0" and "lo" may start with " "
        line = pystring:lstrip(line)
        cell = pystring:split(line)
        if #cell ~= 17 then
            print("invalid interface stats line " .. line)
            goto continue
        end

        devName = cell[1]
        --- ignore lo and veth interface
        if isIgnoredDevice(devName) then
            goto continue
        end

        RxBytes = tonumber(cell[2])
        RxPackets = tonumber(cell[3])
        RxPacketsDrop = tonumber(cell[5])
        TxBytes = tonumber(cell[10])
        TxPackets = tonumber(cell[11])
        TxPacketsDrop = tonumber(cell[13])

        -- change "device" label to certain dev 
        for _, item in ipairs(local_ls) do 
            if item.name == "interface" then
                item.index = devName
                break
            end
        end
        
        values[k] = {
            name = "network_receive_bytes",
            value = RxBytes
        }
        values[k + 1] = {
            name = "network_receive_packets",
            value = RxPackets
        }
        values[k + 2] = {
            name = "network_receive_packets_dropped",
            value = RxPacketsDrop
        }
        values[k + 3] = {
            name = "network_transmit_bytes",
            value = TxBytes
        }
        values[k + 4] = {
            name = "network_transmit_packets",
            value = TxPackets
        }
        values[k + 5] = {
            name = "network_transmit_packets_dropped",
            value = TxPacketsDrop
        }

        self:appendLine(self:_packProto("con_net_stat", local_ls , values))

        ::continue::
    end    
    self:push(lines)
end

return ConNetStat
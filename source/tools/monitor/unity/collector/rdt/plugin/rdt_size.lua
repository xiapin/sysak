require("common.class")
local pystring = require("common.pystring")
local system = require("common.system")
local unistd = require("posix.unistd")
local CvProc = require("collector.vproc")
local rdt_size = class("rdt_size", CvProc)

function rdt_size:_init_(proto, pffi, mnt, path, labels)
    CvProc._init_(self, proto, pffi, mnt, path .. "/" .. "size")
    self.labels = labels
end

function rdt_size:proc(elapsed, lines)
    if not unistd.access(self.pFile) then
        return
    end

    CvProc.proc(self)
    -- MB:0=100;1=100
    -- L3:0=fff;1=fff
    for line in io.lines(self.pFile) do
        local arr = pystring:split(line, ":")
        local type = pystring:strip(arr[1], ' ') -- MB or L3
        local info = pystring:split(arr[2], ";") -- 0=100;1=100

        for _, resouce in ipairs(info) do
            -- 0=100
            local values = {}
            local data = pystring:split(resouce, "=")
            local socketId = data[1]
            local num = tonumber(data[2])

            local v = {
                name = type,
                value = num
            }

            table.insert(values, v)
            local labels = system:deepcopy(self.labels)
            table.insert(labels, { name = "socket", index = socketId })

            self:appendLine(self:_packProto("rdt_alloc_policy", labels, values))
        end
    end

    self:push(lines)
end

return rdt_size

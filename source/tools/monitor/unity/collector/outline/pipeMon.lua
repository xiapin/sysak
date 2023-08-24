---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liaozhaoyan.
--- DateTime: 2023/2/17 12:18 AM
---

require("common.class")

local unistd = require("posix.unistd")
local socket = require("socket")
local system = require("common.system")
local CprotoData = require("common.protoData")
local lineParse = require("common.lineParse")
local pystring = require("common.pystring")
local CpipeMon = class("CpipeMon")

function CpipeMon:_init_(que, fYaml)
    self._paths = {}
    self._socks = {}
    self._proto = CprotoData.new(que)

    self:setupPipe(fYaml)
end

function CpipeMon:_del_()
    for _, sock in ipairs(self._socks) do
        sock:close()
    end

    for _, path in ipairs(self._paths) do
        unistd.unlink(path)
    end
end

function CpipeMon:setupPipe(fYaml)
    local res = system:parseYaml(fYaml)

    for i, path in ipairs(res.outline) do
        if unistd.access(path) then
            unistd.unlink(path)
        end
        self._paths[i] = path

        socket.unix = require("socket.unix")
        local s = socket.unix.udp()
        if s then
            s:bind(path)
            print("bind " .. path)
            table.insert(self._socks, s)

        else
            error("create udp pipe failed.")
        end
    end
end

local function trans(title, ls, vs, log)
    local labels = {}
    local values = {}
    local logs = {}

    local c = 0
    for k, v in pairs(ls) do
        c = c + 1
        labels[c] = {name=k, index=v}
    end
    c = 0
    for k, v in pairs(vs) do
        c = c + 1
        values[c] = {name=k, value=v}
    end
    c = 0
    for k, v in pairs(log) do
        c = c + 1
        logs[c] = {name=k, log=v}
    end
    return {line = title, ls = labels, vs = values, log = logs}
end

function CpipeMon:procLine(line)
    return trans(lineParse.parse(line))
end

function CpipeMon:procLines(stream)
    local ss = pystring:split(stream, "\n")
    local lines = self._proto:protoTable()

    for _, line in ipairs(ss) do
        table.insert(lines.lines, self:procLine(line))
    end
    local bytes = self._proto:encode(lines)
    self._proto:que(bytes)
    return 0
end

function CpipeMon:poll()
    while true do
        local socks, _, err = socket.select(self._socks, nil, -1)
        if err then
            print("out line poll return: " .. err)
            return 0
        end
        for _, sock in pairs(socks) do
            if type(sock) == "number" then
                break
            end
            local stream, err = sock:receive()
            if stream then
                res, msg = pcall(self.procLines, self, stream)
                if not res then
                    print("bad line:\n" .. stream)
                    print(msg)
                end
            else
                print("recv line return: " .. err)
                return 0
            end
        end
    end
    return 0
end

return CpipeMon
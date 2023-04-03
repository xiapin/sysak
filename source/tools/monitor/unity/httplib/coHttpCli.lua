---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liaozhaoyan.
--- DateTime: 2023/4/3 11:42
---

require("common.class")
local ChttpComm = require("httplib.httpComm")
local CcoHttpCli = class("coHttpCli", ChttpComm)
local pystring = require("common.pystring")
local socket = require("posix.sys.socket")
local luaSocket = require("socket")
local unistd = require("posix.unistd")
local system = require("common.system")

local ip_pattern = "(%d%d?%d?)%.(%d%d?%d?)%.(%d%d?%d?)%.(%d%d?%d?)"

local function match_ip(ip)
    local d1, d2, d3, d4 = ip:match(ip_pattern)
    if d1 and d2 and d3 and d4 then
        local num1, num2, num3, num4 = tonumber(d1), tonumber(d2), tonumber(d3), tonumber(d4)
        if num1 >= 0 and num1 <= 255 and num2 >= 0 and num2 <= 255 and num3 >= 0 and num3 <= 255 and num4 >= 0 and num4 <= 255 then
            return true
        end
    end
    return false
end

local function getIp(host)
    local ip
    if match_ip(host) then
        ip = host
    else
        ip = luaSocket.dns.toip(host)
    end
    return ip
end

local function installFd(ip, port)
    local fd, res, err, errno
    fd, err, errno = socket.socket(socket.AF_INET, socket.SOCK_STREAM, 0)
    if fd then  -- for socket
        local tConn = {family=socket.AF_INET, addr=ip, port=port}
        res, err, errno = socket.connect(fd, tConn)
        if res then
            return fd
        else
            unistd.close(fd)
            system:posixError("socket connect failed", err, errno)
        end
    else  -- socket failed
        system:posixError("create socket failed", err, errno)
    end
end

local function setupSocket(host, port)
    local ip = getIp(host)
    return installFd(ip, port)
end

function CcoHttpCli:_init_(host, port)
    self._host = host
    port = port or 80
    self.fd = setupSocket(host, port)
end

function CcoHttpCli:_del_()
    if self.fd then
        unistd.close(self.fd)
    end
end

function CcoHttpCli:write(stream)
    local sent, err, errno
    local res

    sent, err, errno = socket.send(self.fd, stream)
    if sent then
        return
    else
        system:posixError("socket send error.", err, errno)
    end
end

function CcoHttpCli:read()
    local res, err, errno
    res, err, errno = socket.recv(self.fd, 8192)
    return res
end

function CcoHttpCli:_get(url)
    url = url or "/"
    local line = self:packCliHead('GET', url)
    local head = {
        Host = self._host,
        ["Accept-Encoding"] = "null"
    }
    local heads = self:packCliHeaders(head)
    return pystring:join("\r\n", {line, heads, ""})
end

function CcoHttpCli:get(url)
    local stream = self:_get(url)
    self:write(stream)
    print(self:read())
end

return CcoHttpCli

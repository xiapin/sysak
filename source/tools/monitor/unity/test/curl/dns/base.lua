package.path = package.path .. ";../../../?.lua;"
local socket = require("socket")
local pystring = require("common.pystring")
local system = require("common.system")

local function lookup_server()
    local f = io.open("/etc/resolv.conf")
    local server = ""
    for line in f:lines() do
        if pystring:startswith(line, "nameserver") then
            local res = pystring:split(line)
            server = res[2]
            break
        end
    end

    f:close()
    return server
end

function dns_lookup(domain_name)
    local udp = socket.udp()
    local dst = lookup_server()
    print(dst)
    udp:setpeername(dst, 53)
    udp:settimeout(5)

    local cnt = 0
    local queries = {}
    local head = string.char(
            0x12, 0x34, -- Query ID
            0x01, 0x00, -- Standard query
            0x00, 0x01, -- Number of questions
            0x00, 0x00, -- Number of answers
            0x00, 0x00, -- Number of authority records
            0x00, 0x00  -- Number of additional records
    )
    cnt = cnt + 1
    queries[cnt] = head

    local names = pystring:split(domain_name, ".")
    for _, name in ipairs(names) do
        cnt = cnt + 1
        queries[cnt] = string.char(string.len(name))
        cnt = cnt + 1
        queries[cnt] = name
    end
    cnt = cnt + 1
    local tail = string.char(
            0x00, -- End of domain name
            0x00, 0x01, -- Type A record
            0x00, 0x01 -- Class IN
    )
    queries[cnt] = tail

    local query = table.concat(queries)
    system:hexdump(query)
    udp:send(query)
    local response = udp:receive()
    system:hexdump(response)
    udp:close()
    if response then
        local ip_address = string.format("%d.%d.%d.%d", string.byte(response, -4, -1))
        return ip_address
    else
        return nil
    end
end

local domain_name = "www.baidu.com"
local ip_address = dns_lookup(domain_name)
if ip_address then
    print(string.format("%s -> %s", domain_name, ip_address))
else
    print(string.format("Failed to resolve %s", domain_name))
end
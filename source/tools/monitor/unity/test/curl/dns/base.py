import socket


def dns_lookup(domain_name):
    dns_server = "100.100.2.136"
    dns_port = 53
    query = bytearray()
    query += bytearray.fromhex("AA AA") # Query ID
    query += bytearray.fromhex("01 00") # Flags
    query += bytearray.fromhex("00 01") # Questions
    query += bytearray.fromhex("00 00") # Answers
    query += bytearray.fromhex("00 00") # Authority records
    query += bytearray.fromhex("00 00") # Additional records
    for part in domain_name.split("."):
        query += bytes([len(part)])
        query += part.encode()
    query += bytearray.fromhex("00") # End of domain name
    query += bytearray.fromhex("00 01") # Type A record
    query += bytearray.fromhex("00 01") # Class IN

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.sendto(query, (dns_server, dns_port))
    response, _ = sock.recvfrom(1024)
    sock.close()

    if response:
        ip_address = ".".join(str(byte) for byte in response[-4:])
        return ip_address
    else:
        return None


domain_name = "www.baidu.com"
ip_address = dns_lookup(domain_name)
if ip_address:
    print(f"{domain_name} -> {ip_address}")
else:
    print(f"Failed to resolve {domain_name}")
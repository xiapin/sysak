
import socket
server_address = ('localhost', 8398)

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(server_address)
sock.send("hello")
print(sock.recv(100))
sock.send("world.")
print(sock.recv(100))
sock.close()

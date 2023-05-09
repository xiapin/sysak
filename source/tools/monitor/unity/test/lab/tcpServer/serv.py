from socketserver import BaseRequestHandler, TCPServer
from nsenter import Namespace
import socket


def getIp():
    hostname = socket.gethostname()
    print(hostname)
    return socket.gethostbyname(hostname)


class EchoHandler(BaseRequestHandler):
    def handle(self):
        print('Got connection from', self.client_address)
        with Namespace('/var/run/netns/tang1', 'net'):
            print(getIp())
            while True:
                msg = self.request.recv(8192)
                print(msg)
                if not msg:
                    break
                self.request.send(msg)


if __name__ == '__main__':
    print(getIp())
    serv = TCPServer(('', 4321), EchoHandler)
    serv.serve_forever()

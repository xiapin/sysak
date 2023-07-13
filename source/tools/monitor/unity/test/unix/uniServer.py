import os
import socket
import select

class SocketServer(object):
    def __init__(self, sFile):
        super(SocketServer, self).__init__()

        server_address = sFile
        socket_family = socket.AF_UNIX
        socket_type = socket.SOCK_DGRAM
        self._sock = socket.socket(socket_family, socket_type)
        self._sock.bind(server_address)

    def loop(self, cb):
        fd = self._sock.fileno()
        with select.epoll() as poll:
            poll.register(fd, select.EPOLLIN)
            while True:
                events = poll.poll(-1)
                for fd, event in events:
                    if event & select.EPOLLIN:
                        s = os.read(fd, 128 * 1024).decode()
                        cb(s)
                    if event & (select.EPOLLHUP | select.EPOLLERR):
                        return -1

    def __del__(self):
        self._sock.close()


def cbTest(s):
    print("recv: %s" % s)


if __name__ == "__main__":
    s = SocketServer("/tmp/sysom")
    s.loop(cbTest)
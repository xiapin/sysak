import os
import socket
import random
import time
import signal
from threading import Thread


class CcoThread(Thread):
    def __init__(self, addr, tid):
        super(CcoThread, self).__init__()
        self.setDaemon(True)
        self._addr = addr
        self._tid = tid
        self.start()

    def is_socket_closed(self, sock):
        try:
            # this will try to read bytes without blocking and also without
            # removing them from buffer (peek only)
            data = sock.recv(16, socket.MSG_DONTWAIT | socket.MSG_PEEK)
            if len(data) == 0:
                return True
        except BlockingIOError:
            return False  # socket is open and reading from it would block
        except ConnectionResetError:
            return True  # socket was closed for some other reason
        except Exception as e:
            print("unexpected exception when checking if a socket is closed", str(e))
            return False
        return False

    def _work(self):
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect(self._addr)
        print("close test.")
        sock.send("a" * 64)
        time.sleep(21)
        if not self.is_socket_closed(sock):
            print("failed.")
            os.kill(os.getpid(), 9)
        print("sock is closed: ", self.is_socket_closed(sock))
        sock.close()

    def run(self):
        print("thread working.", self._tid)
        while True:
            self._work()
            time.sleep(1)


class CpoThread(Thread):
    def __init__(self, addr, tid):
        super(CpoThread, self).__init__()
        self.setDaemon(True)
        self._addr = addr
        self._tid = tid
        self.start()

    def _work(self, loop):
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect(self._addr)
        for _ in range(loop):
            sock.send("a" * 8192)
            send = "head%d" % self._tid
            sock.send(send)
            sock.send("\r\n")
            s = ""
            while len(s) < 8192 + 6:
                s += sock.recv(16384)
            assert s.endswith(send + "\r\n"), s
            sock.send("world.")
            time.sleep(0.01)
            sock.send("\r\n")
            s =sock.recv(100)
            assert(s.endswith("world.\r\n"))
            time.sleep(0.01)
        sock.close()

    def run(self):
        print("thread working.", self._tid)
        while True:
            var = random.randint(10, 50)
            self._work(var)
            time.sleep(0.1)


ts = []
addr = ('localhost', 8398)
ts.append(CcoThread(addr, 100))
for i in range(30):
    ts.append(CpoThread(addr, i))
    time.sleep(1)
signal.pause()

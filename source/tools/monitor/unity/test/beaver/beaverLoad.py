import socket
from signal import pause
from threading import Thread


class CsockThread(Thread):
    def __init__(self):
        super(CsockThread, self).__init__()
        self.start()

    def run(self):
        while True:
            s = socket.socket()
            s.connect(("127.0.0.1", 8400))
            print("connect ok.")
            s.send("hello".encode())
            r = s.recv(16).decode()
            assert (r == "hello")
            s.close()


if __name__ == "__main__":
    # echo hello | nc 127.0.0.1 8400
    for i in range(1):
        CsockThread()
    pause()


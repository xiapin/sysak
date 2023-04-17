import os
import socket

PIPE_PATH = "/var/sysom/line"
MAX_BUFF = 128 * 1024


class CnfPut(object):
    def __init__(self, path=PIPE_PATH):
        self._sock = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
        self._path = path
        if not os.path.exists(self._path):
            raise ValueError("pipe path is not exist. please check Netinfo is running.")

    def puts(self, s):
        if len(s) > MAX_BUFF:
            raise ValueError("message len %d, is too long ,should less than%d" % (len(s), MAX_BUFF))
        return self._sock.sendto(s, self._path)


if __name__ == "__main__":
    nf = CnfPut()
    nf.puts('runtime,mode=java log="hello runtime."')

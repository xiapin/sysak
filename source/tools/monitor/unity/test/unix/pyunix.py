import os
import time
import socket

PIPE_PATH = "/tmp/sysom"
MAX_BUFF = 128 * 1024


class CnfPut(object):
    def __init__(self):
        self._sock = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
        self._path = PIPE_PATH
        if not os.path.exists(self._path):
            raise ValueError("pipe path is not exist. please check Netinfo is running.")

    def puts(self, s):
        if len(s) > MAX_BUFF:
            raise ValueError("message len %d, is too long ,should less than%d" % (len(s), MAX_BUFF))
        return self._sock.sendto(s, self._path)


if __name__ == "__main__":
    nf = CnfPut()
    i = 10
    while True:
        print(nf.puts('io_burst,disk=/dev/vda1 limit=10.0,max=%d,log="io log burst"' % i))
        i += 1
        time.sleep(5)
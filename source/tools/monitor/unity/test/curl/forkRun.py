
import time
from outLine import CnfPut


def loop(nf):
    i = 1
    while True:
        nf.puts('forkRun,mode=java log="hello runtime.",count=%d' % i)
        i += 1
        time.sleep(15)


if __name__ == "__main__":
    time.sleep(2)
    nf = CnfPut()
    loop(nf)
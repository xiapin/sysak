import os
import sys
import re

rePid = re.compile(r"^\d+$")


def checkState(path, pid):
    fStatus = os.path.join(path, pid, "status")
    try:
        with open(fStatus, "r") as f:
            for _, line in enumerate(f):
                if line.startswith("State:"):
                    _, stats = line.split(":", 1)
                    stats, _ = stats.split("(", 1)
                    return stats.strip()
    except Exception:
        return "N"


def getCmd(path, pid):
    fCmd = os.path.join(path, pid, "cmdline")
    try:
        with open(fCmd, 'r') as f:
            return f.read()
    except Exception:
        return "unknown"


def getKstack(path, pid):
    fCmd = os.path.join(path, pid, "stack")
    try:
        with open(fCmd, 'r') as f:
            return f.read()
    except Exception:
        return "unknown"


def walk_pids(path, fil):
    for pid in os.listdir(path):
        if rePid.match(pid):
            tPath = "/proc/%s/task" % pid
            for tid in os.listdir(tPath):
                if checkState(tPath, tid) == fil:
                    print("task %s, comm: %s, status: %s" % (pid, getCmd(tPath, pid), fil))
                    print(getKstack(tPath, pid))


if __name__ == "__main__":
    if len(sys.argv) == 1:
        fil = "D"
    else:
        fil = sys.argv[1]
    walk_pids("/proc", fil)
    pass

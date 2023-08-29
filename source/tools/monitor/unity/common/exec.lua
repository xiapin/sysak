---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liaozhaoyan.
--- DateTime: 2023/5/4 10:50
---

local system = require("common.system")
local unistd = require("posix.unistd")
local pwait = require("posix.sys.wait")
local signal = require("posix.signal")

local module = {}

local function setFdOpt(fd, size)
    local err, errno
    local fcntl = require("posix.fcntl")
    local F_SETPIPE_SZ = 1024 + 7 -- F_LINUX_SPECIFIC_BASE + 7 refer to https://lxr.missinglinkelectronics.com/linux/include/uapi/linux/fcntl.h#L29

    system:fdNonBlocking(fd)
    _, err, errno = fcntl.fcntl(fd, F_SETPIPE_SZ, size)
    assert(not err, "set file F_SETPIPE_SZ failed.")
end

function module.run(cmd, args, fIn, fOut)
    local buffSize = 32 * 1024
    local pid, err = unistd.fork()
    if pid > 0 then   -- for self
        print("pid: " .. pid)
        if fIn then
            setFdOpt(fIn, buffSize)
        end
        if fOut then
            unistd.close(fOut)
        end
        return pid
    elseif pid == 0 then   -- for child
        local err, errno
        if prctl_death_kill then   -- check prctl_death_kill function is already register.
            prctl_death_kill()  -- when parent exit, child process will be killed.
        end

        if fIn then
            unistd.close(fIn)
        end
        if fOut then
            setFdOpt(fOut, buffSize)
            _, err, errno = unistd.dup2(fOut, 1)
            assert(not errno, "dup2 fd failed.")
        end
        _, err, errno = unistd.exec(cmd, args)
        assert(not errno, "exec failed." .. err .. errno)
    else
        error("fork report" .. err)
    end
end

function module.kill(pid)
    signal.kill(pid, signal.SIGKILL)  -- force to kill task
    pwait.wait(pid)                     -- wait task
end

return module

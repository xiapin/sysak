package metrics

import (
    // "fmt"
    "io/ioutil"
    "strconv"
    "strings"
    "sql-obs/common"
)

type CgroupInfo struct {
    path string
    // direct reclaim memory
    DRMemLayout       [7]uint64
    DRMemLatencyTotal uint64
    DRMemLatencyPS    float64
    // cgroup mem layout
    AnonMem  uint64
    CacheMem uint64
    ShMem    uint64
    // dirty page
    Dirty          uint64
    MemLimit       uint64
    MemUsage       uint64
}

var CgroupInfoMap map[string]*CgroupInfo = make(map[string]*CgroupInfo)

func getCgroupInfo(pid int, containerID string) *CgroupInfo {
    getMemCgroupPath(pid, containerID)
    getMemUsedLayout(CgroupInfoMap[containerID])
    getDRLatency(CgroupInfoMap[containerID])
    getDirtyShreshold(CgroupInfoMap[containerID])
    // fmt.Println(*CgroupInfoMap[containerID])
    return CgroupInfoMap[containerID]
}

func getDirtyShreshold(info *CgroupInfo) {
    path := info.path
    for {
        if path == "/sys/fs/cgroup" {
            getMemLimitByProc(info)
            return
        }
        limit := getCgroupValue(path + "/memory.limit_in_bytes")
        if limit != 0x7ffffffffffff000 {
            info.MemLimit = limit
            break
        }
        sep := strings.LastIndex(path, "/")
        path = path[0:sep]
    }
    info.MemUsage = getCgroupValue(path + "/memory.usage_in_bytes")
}

func getCgroupValue(path string) uint64 {
    data, err := ioutil.ReadFile(path)
    if err != nil {
        common.PrintOnlyErrMsg("read cgroup memory layout fail.")
        return 0
    }
    lines := strings.Split(string(data), "\n")
    memLimit, _ := strconv.ParseUint(lines[0], 10, 64)
    return memLimit
}

func getMemLimitByProc(info *CgroupInfo) {
    data, err := ioutil.ReadFile("/proc/meminfo")
    if err != nil {
        common.PrintOnlyErrMsg("read meminfo fail.")
        return
    }
    lines := strings.Split(string(data), "\n")
    for _, line := range(lines) {
        if (strings.HasPrefix(line, "MemAvailable")) {
            parts := strings.Fields(line)
            memAva, _ := strconv.ParseUint(parts[1], 10, 64)
            info.MemLimit = memAva * 1024
            info.MemUsage = 0
            return
        }
    }
}

func getMemUsedLayout(info *CgroupInfo) {
    data, err := ioutil.ReadFile(info.path +
        "/memory.stat")
    if err != nil {
        common.PrintOnlyErrMsg("read cgroup memory layout fail.")
        return
    }

    var anon uint64
    var cache uint64
    lines := strings.Split(string(data), "\n")
    for _, line := range lines {
        parts := strings.Fields(line)
        if len(parts) != 2 {
            continue
        }
        if parts[0] == "total_inactive_anon" ||
            parts[0] == "total_active_anon" {
            value, _ := strconv.ParseUint(parts[1], 10, 64)
            anon += value
        } else if parts[0] == "total_inactive_file" ||
            parts[0] == "total_active_file" {
            value, _ := strconv.ParseUint(parts[1], 10, 64)
            cache += value
        } else if parts[0] == "total_shmem" {
            info.ShMem, _ = strconv.ParseUint(parts[1], 10, 64)
        } else if parts[0] == "total_dirty" {
            info.Dirty, _ = strconv.ParseUint(parts[1], 10, 64)
        }
    }
    info.AnonMem = anon
    info.CacheMem = cache
}

func getDRLatency(info *CgroupInfo) {
    data, err := ioutil.ReadFile(info.path +
        "/memory.direct_reclaim_global_latency")
    if err != nil {
        common.PrintOnlyErrMsg("read cgroup memory layout fail.")
        return
    }

    var count uint64
    lines := strings.Split(string(data), "\n")
    for index, line := range lines {
        parts := strings.Fields(line)
        if len(parts) != 2 {
            continue
        }
        value, _ := strconv.ParseUint(parts[1], 10, 64)
        if strings.Contains(parts[0], "ms:") {
            count += (value - info.DRMemLayout[index])
            info.DRMemLayout[index] = value
        } else if strings.Contains(parts[0], "total") {
            if count == 0 || info.DRMemLatencyPS < 0 {
                info.DRMemLatencyPS = 0
            } else {
                info.DRMemLatencyPS = 
                    float64(value-info.DRMemLatencyTotal) / float64(count)
            }
            info.DRMemLatencyTotal = value
            break
        }
    }
}

func getMemCgroupPath(pid int, containerID string) {
    if _, ok := CgroupInfoMap[containerID]; ok {
        return
    }
    path, err := common.GetCgroupPath(pid, "memory")
    if err != nil {
        common.PrintOnlyErrMsg("get proc's cgroup file error.")
        return
    }
    CgroupInfoMap[containerID] = new(CgroupInfo)
    CgroupInfoMap[containerID].path = "/sys/fs/cgroup/memory" + path
    CgroupInfoMap[containerID].DRMemLatencyPS = -1
}

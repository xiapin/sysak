package common

import (
    "fmt"
    "io/ioutil"
    "regexp"
    "strings"
    "os"
    "strconv"
    "sort"
)

const (
    Container_Type_Docker = iota
    Container_Type_CRI_Containerd
    Container_Type_CRIO
    Container_Type_K8s_Other
    Container_Type_Unknown
)

var containerTypeStr = []string{
    "docker",
    "cri-containerd",
    "crio",
    "kubepods",
    "unknow",
}
var containerTypeRegexStr = []string{
    "docker",
    "cri-containerd",
    "crio",
    "\\S+",
    "unknow",
}

type regMatch struct {
    burstablePathRegex  *regexp.Regexp
    besteffortPathRegex *regexp.Regexp
    guaranteedPathRegex *regexp.Regexp
}

func getContainerType(path string) string {
    if strings.Contains(path, containerTypeStr[Container_Type_Docker]) {
        return containerTypeRegexStr[Container_Type_Docker]
    } else if strings.Contains(path, containerTypeStr[Container_Type_CRI_Containerd]) {
        return containerTypeRegexStr[Container_Type_CRI_Containerd]
    } else if strings.Contains(path, containerTypeStr[Container_Type_CRIO]) {
        return containerTypeRegexStr[Container_Type_CRIO]
    } else if strings.Contains(path, containerTypeStr[Container_Type_K8s_Other]) {
        // "/sys/fs/cgroup/cpu,cpuacct/kubepods.slice/kubepods-burstable.slice/"
        return containerTypeRegexStr[Container_Type_K8s_Other]
    }
    return containerTypeRegexStr[Container_Type_Unknown]
}

func getContainerTypeByPid(pid int) string {
    cgPath, err := GetCgroupPath(pid, "cpu,cpuacct")
    if err != nil {
        return containerTypeStr[Container_Type_Unknown]
    }
    i := Container_Type_Docker;
    for ; i < Container_Type_Unknown; i++ {
        if strings.Contains(cgPath, containerTypeStr[i]) {
            break
        }
    }
    return containerTypeStr[i]
}

func regexSearch(buffer string, reg *regexp.Regexp) []string {
    return reg.FindAllString(buffer, -1)
}

func regexMatch(buffer string, reg *regexp.Regexp) bool {
    return reg.MatchString(buffer)
}

func (r *regMatch) isMatch(path string) bool {
    if strings.Contains(path, "burstable") {
        return regexMatch(path, r.burstablePathRegex)
    } else if strings.Contains(path, "besteffort") {
        return regexMatch(path, r.besteffortPathRegex)
    } else {
        return regexMatch(path, r.guaranteedPathRegex)
    }
}

func getCGroupMatcher(path string) *regMatch {
    podRegex := "[0-9a-f]{8}[-_][0-9a-f]{4}[-_][0-9a-f]{4}" +
        "[-_][0-9a-f]{4}[-_][0-9a-f]{12}"
    containerRegex := "[0-9a-f]{64}"
    matcher := regMatch{}

    containerType := getContainerType(path)
    if containerType == containerTypeRegexStr[Container_Type_Unknown] {
        return nil
    }
    /*
     STANDARD: kubepods.slice/kubepods-pod2b801b7a_5266_4386_864e_45ed71136371.slice
     /cri-containerd-20e061fc708d3b66dfe257b19552b34a1307a7347ed6b5bd0d8c5e76afb1a870
     .scope/cgroup.procs
    */
    matcher.guaranteedPathRegex =
        regexp.MustCompile("^.*kubepods.slice/kubepods-pod" + podRegex +
            ".slice/" + containerType + "-" + containerRegex +
            ".scope/cgroup.procs$")

    /*
     STANDARD: kubepods.slice/kubepods-besteffort.slice/kubepods-besteffort-pod0d206349
     _0faf_445c_8c3f_2d2153784f15.slice/cri-containerd-efd08a78ad94af4408bcdb097fbcb603
     a31a40e4d74907f72ff14c3264ee7e85.scope/cgroup.procs
    */
    matcher.besteffortPathRegex =
        regexp.MustCompile("^.*kubepods.slice/kubepods-besteffort.slice/" +
            "kubepods-besteffort-pod" + podRegex + ".slice/" +
            containerType + "-" + containerRegex +
            ".scope/cgroup.procs$")

    /*
     STANDARD: kubepods.slice/kubepods-burstable.slice/kubepods-burstable-podee10fb7d
     _d989_47b3_bc2a_e9ffbe767849.slice/cri-containerd-4591321a5d841ce6a60a777223cf7f
     e872d1af0ca721e76a5cf20985056771f7.scope/cgroup.procs
    */
    matcher.burstablePathRegex =
        regexp.MustCompile("^.*kubepods.slice/kubepods-burstable.slice/" +
            "kubepods-burstable-pod" + podRegex + ".slice/" + containerType +
            "-" + containerRegex + ".scope/cgroup.procs$")

    if matcher.isMatch(path) {
        return &matcher
    }

    /*
     GKE: kubepods/pod8dbc5577-d0e2-4706-8787-57d52c03ddf2/14011c7d92a9e513dfd6
     9211da0413dbf319a5e45a02b354ba6e98e10272542d/cgroup.procs
    */
    matcher.guaranteedPathRegex =
        regexp.MustCompile("^.*kubepods/pod" + podRegex + "/" +
            containerRegex + "/cgroup.procs$")

    /*
     GKE: kubepods/besteffort/pod8dbc5577-d0e2-4706-8787-57d52c03ddf2/14011c7d9
     2a9e513dfd69211da0413dbf319a5e45a02b354ba6e98e10272542d/cgroup.procs
    */
    matcher.besteffortPathRegex =
        regexp.MustCompile("^.*kubepods/besteffort/pod" + podRegex + "/" +
            containerRegex + "/cgroup.procs$")

    /*
     GKE: kubepods/burstable/pod8dbc5577-d0e2-4706-8787-57d52c03ddf2/14011c7d92
     a9e513dfd69211da0413dbf319a5e45a02b354ba6e98e10272542d/cgroup.procs
    */
    matcher.burstablePathRegex =
        regexp.MustCompile("^.*kubepods/burstable/pod" + podRegex + "/" +
            containerRegex + "/cgroup.procs$")

    if matcher.isMatch(path) {
        return &matcher
    }

    /*
     pure docker: /sys/fs/cgroup/cpu/docker/
     1ad2ce5889acb209e1576339741b1e504480db77d
    */
    matcher.guaranteedPathRegex =
        regexp.MustCompile("^.*docker/" + containerRegex + "/cgroup\\.procs$")
    if matcher.isMatch(path) {
        return &matcher
    }
    return nil
}

func GetCgroupPath(pid int, t string) (string, error) {
    data, err := ioutil.ReadFile(fmt.Sprintf("/proc/%d/cgroup", pid))
    if err != nil {
        return "", err
    }
    lines := strings.Split(string(data), "\n")
    for _, line := range lines {
        parts := strings.Split(line, ":")
        if len(parts) < 3 {
            continue
        }
        if parts[1] == t {
            return parts[2], nil
        }
    }
    return "", PrintOnlyErrMsg("cgroup not found for pid %d", pid)
}

func GetContainerIdByPid(pid int) []string {
    var containerID, podID string

    path, err := GetCgroupPath(pid, "cpu,cpuacct")
    if err != nil {
        return nil
    }
    matcher := getCGroupMatcher(path + "/cgroup.procs")
    if matcher == nil {
        return nil
    }

    containerRegex := `[0-9a-f]{64}`
    reContainer := regexp.MustCompile(containerRegex)
    containerID = reContainer.FindString(path)

    podRegex := `[0-9a-f]{8}[-_][0-9a-f]{4}[-_][0-9a-f]{4}[-_][0-9a-f]{4}[-_][0-9a-f]{12}`
    rePod := regexp.MustCompile(podRegex)
    podID = rePod.FindString(path)

    return []string{containerID, podID}
}

func getMountPathByMatch(match ...string) ([]string, error) {
    var mnt []string
    filter := ""
    for _, m := range match {
        if len(m) > 0 {
            filter += fmt.Sprintf("| grep -E \"%s\"", m)
        }
    }
    s, err := ExecShell(fmt.Sprintf("mount %s", filter))
    if err != nil {
        PrintOnlyErrMsg("get match mount info fail")
        return nil, err
    }
    for _, entry := range s {
        if len(entry) > 0 {
            mnt = append(mnt, strings.Fields(entry)[2])
        }
    }
    if len(mnt) == 0 {
        err = PrintOnlyErrMsg(
            "not found mount path for match %s", match)
    }
    return mnt, err
}

func getOverlayFSIdByContainerId(containerId string) string {
    ret, err := ExecShell(fmt.Sprintf(
        "docker inspect --format='{{.GraphDriver.Data.MergedDir}}' %s",
        containerId))
    if err != nil {
        return ""
    }
    for _, r := range ret {
        overlayfsId := regexSearch(r, regexp.MustCompile("[0-9a-f]{64}"))
        if len(overlayfsId) > 0 {
            return overlayfsId[0]
        }
    }
    return ""
}

func getMntFromPidMountInfo(containerFile string, pid int) ([]string) {
    fsRoot := ""
    mountPoint := ""
    device := ""
    data, err := ioutil.ReadFile(fmt.Sprintf("/proc/%d/mountinfo", pid))
    if err != nil {
        PrintSysError(err)
        return nil
    }
    lines := strings.Split(string(data), "\n")
    if len(lines[len(lines)-1]) == 0 {
        lines = lines[:len(lines)-1]
    }
    for _, line := range lines {
        f := strings.Fields(line)
        // fields[4] is mount point
        if len(f[4]) < len(containerFile) {
            if strings.HasPrefix(containerFile, f[4]) {
                if len(f[4]) > len(fsRoot) {
                    fsRoot = f[3]
                    mountPoint = f[4]
                    device = f[len(f) - 2]
                }
            }
        }
    }
    if len(fsRoot) == 0 {
        return nil
    }
    return []string{fsRoot, mountPoint, device}
}

func filterPort(ns string, pid int) (int, error) {
    cmdStr := fmt.Sprintf("ip netns exec %s netstat -tanp", ns)
    lines, err := ExecShell(cmdStr)
    if err != nil {
        return 0, err
    }
    for _, line := range lines {
        parts := strings.Fields(line)
        if strings.Split(parts[len(parts)-1], "/")[0] == fmt.Sprintf("%d", pid) {
            arr := strings.Split(parts[3], ":")
            port, err := strconv.Atoi(arr[len(arr)-1])
            if err != nil {
                return 0, err
            }
            return port, nil
        }
    }
    return 0, PrintOnlyErrMsg("failed to find port for pid %d\n", pid)
}

func filterIP(ns string, pid int) (string, error) {
    cmdStr := fmt.Sprintf("ip netns exec %s ip addr show", ns)
    lines, err := ExecShell(cmdStr)
    if err != nil {
        return "", err
    }
    for _, line := range lines {
        if strings.Contains(line, "scope global") {
            parts := strings.Fields(line)
            return strings.Split(parts[1], "/")[0], nil
        }
    }
    return "", PrintOnlyErrMsg("failed to find ip for pid %d\n", pid)
}

func getPodIpAndPort(pid int) (string, int, error) {
    cmdStr := fmt.Sprintf("ip netns identify %d", pid)
    lines, err := ExecShell(cmdStr, "origin")
    if err != nil {
        return "", 0, err
    }
    ns := lines[0]
    port, err := filterPort(ns[:len(ns)-1], pid)
    if err != nil {
        PrintSysError(err)
        return "", 0, err
    }
    ip, err := filterIP(ns[:len(ns)-1], pid)
    if err != nil {
        PrintSysError(err)
        return "", 0, err
    }
    return ip, port, nil
}

func FindPodIpAndPort(pid int, containerId string) (string, int, error) {
    ip, port, retErr := getPodIpAndPort(pid)
    if retErr != nil {
        if getContainerTypeByPid(pid) == containerTypeStr[Container_Type_Docker] {
            retErr = nil
            cmdStr := fmt.Sprintf("docker port %s", containerId)
            lines, err := ExecShell(cmdStr, "origin")
            if err != nil {
                PrintSysError(err)
                return "", 0, err
            }
            var ports []int
            for _, line := range lines {
                if len(line) > 0 {
                    p, err := strconv.Atoi(strings.Split(line, "/")[0])
                    if err != nil {
                        continue
                    }
                    ports = append(ports, p)
                }
            }
            if len(ports) > 0 {
                sort.Ints(ports)
                port = ports[0]
            }
            if port == 0 {
                return "", 0, PrintOnlyErrMsg("failed to find port for pid %d\n", pid)
            }
            cmdStr = fmt.Sprintf("docker inspect %s | grep IPAddress", containerId)
            lines, err = ExecShell(cmdStr, "origin")
            if err != nil {
                PrintSysError(err)
                return "", 0, err
            }
            for _, line := range lines {
                if len(line) > 0 {
                    if strings.Contains(line, `"IPAddress":`) {
                        reg := regexp.MustCompile(
                            `"IPAddress":\s*"(\d+\.\d+\.\d+\.\d+)"`)
                        match := reg.FindStringSubmatch(line)
                        if len(match) >= 2 {
                            ip = match[1]
                            break
                        }
                    }
                }
            }
            if ip == "" {
                return "", 0, PrintOnlyErrMsg("failed to find ip for pid %d\n", pid)
            }
        }
    }
    return ip, port, retErr
}

func GetDeviceByFile(pid int, containerFile string) string {
    mntinfo := getMntFromPidMountInfo(containerFile, pid)
    if mntinfo != nil {
        return mntinfo[2]
    }
    return ""
}

func GetHostFilePathByContainerPath(containerId string,
    podId string, pid int, containerFile string) (string, error) {
	if len(containerId) == 0 && len(podId) == 0 {
		return containerFile, nil
	}
    cgPath, err := GetCgroupPath(pid, "cpu,cpuacct")
    if err != nil {
        return "", err
    }
    containerType := getContainerType(cgPath)
    if containerType == containerTypeRegexStr[Container_Type_Unknown] {
        return "", PrintOnlyErrMsg(
            "unknow container type, cgroup path: %s", cgPath)
    } else if containerType == containerTypeRegexStr[Container_Type_Docker] {
        podId = getOverlayFSIdByContainerId(containerId)
    }
    mntinfo := getMntFromPidMountInfo(containerFile, pid)
    if mntinfo != nil {
        match := containerId
        if len(podId) > 0 {
            match += ("|" + podId)
            if strings.Contains(podId, "_") {
                tmp := strings.ReplaceAll(podId, "_", `-`)
                match += ("|" + tmp)
            } else if strings.Contains(podId, "-") {
                tmp := strings.ReplaceAll(podId, "-", `_`)
                match += ("|" + tmp)
            }
        }
        fsRoot := mntinfo[0]
        mountPoint := mntinfo[1]
        device := mntinfo[2]
        mntPath, err := getMountPathByMatch(device, match)
        if err != nil {
            mntPath, err = getMountPathByMatch(device)
            if err != nil {
                return "", err
            }
        }
        for _, mnt := range mntPath {
            path := mnt + fsRoot + containerFile
            if mountPoint != "/" {
                path = mnt + fsRoot + strings.Replace(
                    containerFile, mountPoint, "", -1)
            }
            if _, err := os.Stat(path); !os.IsNotExist(err) {
                return path, nil
            }
        }
    }
    return "", PrintOnlyErrMsg(
        "not found host path for %s(from container %s)", 
        containerFile, containerId)
}

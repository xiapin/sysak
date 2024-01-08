package common

import (
    "fmt"
    "os"
    "path/filepath"
    "reflect"
    "regexp"
    "sort"
    "strconv"
    "strings"
    "io/ioutil"
    //"unsafe"
    "github.com/fsnotify/fsnotify"
)

type processInfo struct {
    Pid         int
    Port        int
    Ip          string
    Comm        string
    PodId       string
    ContainerId string
    Slowlog     string
    Errlog      string
    CnfPath     string
    PvDevice    []string
}

var appInfoList = make(map[string]*processInfo)

func findPort(pid int) (int, error) {
    out, err := ExecShell("netstat -tanp", "origin")
    if err != nil {
        return 0, err
    }
    re := regexp.MustCompile(fmt.Sprintf(".*LISTEN\\s+%d/mysqld", pid))
    matches := re.FindAllString(out[0], -1)
    if len(matches) > 0 {
        var ports []int
        for _, match := range matches {
            s := strings.Fields(match)
            arr := strings.Split(s[3], ":")
            port, err := strconv.Atoi(arr[len(arr)-1])
            if err != nil {
                continue
            }
            ports = append(ports, port)
        }
        if len(ports) > 0 {
            sort.Ints(ports)
            return ports[0], nil
        }
    }
    return 0, PrintOnlyErrMsg("failed to find port for pid %d\n", pid)
}

func findLogFile(pid int, logType string, moreMatch ...string) (string, error) {
    basePath := fmt.Sprintf("/proc/%d/", pid)
    logPath := ""

    if logType == "err" {
        logPath = filepath.Join(basePath, "fd", "2")
        realpath, err := os.Readlink(logPath)
        if err != nil {
            PrintSysError(err)
            return "", err
        }
        logPath = realpath
        for _, m := range moreMatch {
            if !strings.Contains(logPath, m) {
                logPath = ""
            }
        }
    } else {
        logPath = filepath.Join(basePath, "fd")
        cmdStr := fmt.Sprintf("ls -l %s | grep %q", logPath,
            strings.ReplaceAll(logType, ".", `\.`))
        for _, m := range moreMatch {
            cmdStr += fmt.Sprintf(
                "| grep %q", strings.ReplaceAll(m, ".", `\.`))
        }
        matches, err := ExecShell(cmdStr)
        if err != nil {
            return "", PrintOnlyErrMsg("not found the path of %s log", logType)
        }
        for _, match := range matches {
            s := strings.Fields(match)
            if len(s) > 0 {
                logPath = s[len(s)-1]
                for _, m := range moreMatch {
                    if !strings.Contains(logPath, m) {
                        logPath = ""
                        continue
                    }
                }
                break
            }
        }
    }
    if len(logPath) == 0 {
        return "", PrintOnlyErrMsg("not found the path of %s log", logType)
    }
    return logPath, nil
}

func getKeyValueFromString(str string, key string) string {
    re := regexp.MustCompile(key + `=([^ \t\n\r\f\v]+)`)
    match := re.FindStringSubmatch(str)
    if len(match) >= 2 {
        return match[1]
    }
    return ""
}

func getDeviceList(pid int) []string {
    var deviceList []string
    dirScanRecordList := make(map[string]bool)
    addString := func (s *[]string, substr string) {
        for _, str := range *s {
            if str == substr {
                return
            }
        }
        *s = append(*s, substr)
    }

    fds, err := filepath.Glob(fmt.Sprintf("/proc/%d/fd/*", pid))
    if err != nil {
        return deviceList
    }
    for _, fd := range fds {
        realPath, err := os.Readlink(fd)
        if err != nil {
            continue
        }
        if len(realPath) > 0 && filepath.IsAbs(realPath){
            dir := filepath.Dir(realPath)
            if _, ok := dirScanRecordList[dir]; !ok {
                dirScanRecordList[dir] = true
                device := GetDeviceByFile(pid, realPath)
                if len(device) > 0 && strings.Contains(device, "/dev/")  {
                    addString(&deviceList, device)
                }
            }
        }
    }
    return deviceList
}

func getProcessInfo(psEntry string) (processInfo, error) {
    port := 0
    ip := "localhost"
    errLogPath := ""
    slowLogPath := ""
    cnfPath := ""
    podId := "NULL"
    containerId := "NULL" 
    pid, err := strconv.Atoi(strings.Fields(psEntry)[1])
    if err != nil {
        return processInfo{}, err
    }

    id := GetContainerIdByPid(pid)
    if id != nil {
        if len(id[0]) > 0 {
            containerId = id[0]
        }
        if len(id[1]) > 0 {
            podId = id[1]
        }
    }
    //fmt.Printf("conrainer id: %s, pod id: %s\n", containerId, podId)
    p := getKeyValueFromString(psEntry, "--port")
    if len(p) > 1 {
        port, err = strconv.Atoi(p)
        if err != nil {
            port = 0
        }
    }

    if port == 0 {
        if containerId == "NULL" {
            port, err = findPort(pid)
            if err != nil {
                return processInfo{}, err
            }
        } else {
            ip, port, err = FindPodIpAndPort(pid, containerId)
            if err != nil {
                return processInfo{}, err
            }
        }
    }

    errLogPath = getKeyValueFromString(psEntry, "--log-error")
    slowLogPath = getKeyValueFromString(psEntry, "--slow-query-log")
    cnfPath = getKeyValueFromString(psEntry, "--defaults-file")

    if len(slowLogPath) == 0 {
        slowLogPath, _ = findLogFile(pid, "slow", ".log")
    }
    if len(errLogPath) == 0 {
        errLogPath, _ = findLogFile(pid, "err", ".log")
    }
    if id != nil {
        if len(slowLogPath) > 0 {
            slowLogPath, _ = GetHostFilePathByContainerPath(containerId,
                podId, pid, slowLogPath)
        }
        if len(errLogPath) > 0 {
            errLogPath, _ = GetHostFilePathByContainerPath(containerId,
                podId, pid, errLogPath)
        }
    }

    if len(cnfPath) == 0 {
        cnfPath, err = findLogFile(pid, "cnf", ".cnf")
    }
    if containerId != "NULL" {
        containerId = containerId[:12]
    }
    return processInfo{
            pid, port, ip, "", podId, containerId, slowLogPath, errLogPath,
            cnfPath, getDeviceList(pid)}, nil
}

func initAppInfoList(comm string) error {
    cmdStr := fmt.Sprintf("ps -ef | grep %s | grep -v grep", comm)
    matches, err := ExecShell(cmdStr)
    if err != nil {
        return err
    }
    for _, match := range matches {
        if len(match) > 0 {
            commBytes, err := ioutil.ReadFile(
                "/proc/" + strings.Fields(match)[1] + "/comm")
            if err != nil {
                continue
            }
            commStr := strings.TrimSpace(string(commBytes))
            if commStr != comm {
                continue
            }
            pInfo, err := getProcessInfo(match)
            if err != nil {
                //PrintSysError(err)
                continue
            }
            key := string(pInfo.ContainerId) + ":" + comm
            if pInfo.ContainerId == "NULL" {
                key = comm + ":" + strconv.Itoa(pInfo.Port)
            }
            if _, ok := appInfoList[key]; ok {
                appInfoList[key].Pid = pInfo.Pid
            } else {
                pInfo.Comm = comm
                appInfoList[key] = &pInfo
            }
        }
    }
    if len(appInfoList) == 0 {
        return PrintOnlyErrMsg("not found app %s", comm)
    }
    return nil
}

func getKeyByPidFromAppInfoList(pid int) string {
    for key, app := range appInfoList {
        if pid == app.Pid {
            return key
        }
    }
    return ""
}

func deleteAppInstancesInfo(event *fsnotify.Event, priData *interface{}) int {
    re := regexp.MustCompile(`(\d+)`)
    m := re.FindStringSubmatch(event.Name)
    if len(m) > 1 {
        pid, err := strconv.Atoi(m[1])
        if err != nil {
            return Handle_Done
        }
        key := getKeyByPidFromAppInfoList(pid)
        if len(key) > 0 {
            delete(appInfoList, key)
        }
    }
    return Handle_Done
}

func foundAppInstances(value reflect.Value,
    match map[string]interface{}) bool {
    matchCnt := len(match)
    for m, v := range match {
        field := value.FieldByName(m)
        if field.IsValid() {
            val := reflect.ValueOf(v)
            vType := reflect.TypeOf(v)
            if field.Type() == vType && 
               field.Interface() == val.Interface() {
                matchCnt--
                continue
            }
        }
    }
    if matchCnt == 0 {
        return true
    }
    return false
}

func ForeachAppInstances(comm string, inArgs []string,
    f func(values []interface{})) {
    for _, app := range appInfoList {
        if comm == app.Comm {
            var argsVal []interface{}
            value := reflect.ValueOf(app).Elem()
            for _, a := range inArgs {
                field := value.FieldByName(a)
                if field.IsValid() {
                    argsVal = append(argsVal, field.Interface())
                }
            }
            f(argsVal)
        }
    }
}

func GetAppInstanceMemberByPid(pid int, member string) interface{} {
    key := getKeyByPidFromAppInfoList(pid)
    if _, ok := appInfoList[key]; ok {
        app := appInfoList[key]
        value := reflect.ValueOf(app).Elem()
        field := value.FieldByName(member)
        if field.IsValid() {
            return field.Interface()
        }
    }
    return ""
}

func GetAppInstanceInfo(match map[string]interface{},
    member string) interface{} {
    for _, app := range appInfoList {
        value := reflect.ValueOf(app).Elem()
        found := foundAppInstances(value, match)
        if found {
            field := value.FieldByName(member)
            if field.IsValid() {
                return field.Interface()
            }
        }
    }
    return nil
}

func AppInstancesAjustMember(match map[string]interface{},
    ajust map[string]interface{}) {
    for _, app := range appInfoList {
        value := reflect.ValueOf(app).Elem()
        found := foundAppInstances(value, match)
        if found {
            for m, v := range ajust {
                field := value.FieldByName(m)
                if field.IsValid() {
                    if reflect.TypeOf(v) == reflect.TypeOf("") && 
                        filepath.IsAbs(v.(string)) {
                        if len(app.ContainerId) > 0 {
                            val, err := GetHostFilePathByContainerPath(
                                app.ContainerId, app.PodId, app.Pid,
                                v.(string))
                            if err == nil {
                                v = val
                            }
                        }
                        _, err := os.Stat(v.(string))
                        if !os.IsNotExist(err) {
                            field.Set(reflect.ValueOf(v))
                        }
                        continue
                    }
                    field.Set(reflect.ValueOf(v))
                }
            }
            break
        }
    }
}

func InitAppInstancesInfo(comm string) error {
    err := initAppInfoList(comm)
    if err != nil {
        return err
    }

    var files []string
    for _, app := range appInfoList {
        files = append(files,
            "/proc/"+strconv.Itoa(app.Pid))
    }
    return StartFilesOpWatcher(files, fsnotify.Remove,
        deleteAppInstancesInfo, nil, nil)
}

func GetAppInstanceCnt() int {
    cnt := 0
    for _, app := range appInfoList {
        if app != nil {
            cnt++
        }
    }
    return cnt
}
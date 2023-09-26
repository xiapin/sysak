package tracing

import (
    "net"
    "os"
    "sql-obs/common"
    "strconv"
    "strings"
    "time"
)

const (
    //PIPE_PATH string = "/var/sysom/outline" // 参考 YAML 中的配置
    MAX_BUFF int = 64 * 1024 // 最大消息长度
)


func StartTracingSql() {
    go traceSqlRequestRT("mysqld")
}

func traceSqlRequestRT(comm string) {
    var pidList []string
    common.ForeachAppInstances(comm, []string{"Pid"},
        func(values []interface{}) {
            pidList = append(pidList, strconv.Itoa(values[0].(int)))
        })

    if len(pidList) == 0 {
        common.PrintOnlyErrMsg("not found app %s", comm)
        return 
    }
    pidList = append([]string{"mysql", strconv.Itoa(len(pidList))}, pidList...)

    pidListStr := strings.Join(pidList, ",")
    for retry := 0; retry < 10; retry++ {
        if _, err := os.Stat("/var/ntopo"); os.IsNotExist(err) {
			common.PrintOnlyErrMsg("ntopo not startup, retry after 3secs")
            time.Sleep(3 * time.Second)
            continue
        }
        addr, err := net.ResolveUnixAddr("unix", "/var/ntopo")
        if err != nil {
            common.PrintSysError(err)
            break
        }
        sock, err := net.DialUnix("unix", nil, addr)
        if err != nil {
            common.PrintSysError(err)
            break
        }
    
        if len(pidListStr) > MAX_BUFF {
            common.PrintOnlyErrMsg("pidList too long")
            break
        }
    
        if _, err := sock.Write([]byte(pidListStr)); err != nil {
            common.PrintOnlyErrMsg("send pidlist failed, pidlist: %s", pidListStr)
            break
        }
		break
    }
}

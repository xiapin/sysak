package tracing

import (
	"fmt"
	"sql-obs/common"
	"strconv"
	"strings"
)

const (
	//PIPE_PATH string = "/var/sysom/outline" // 参考 YAML 中的配置
	MAX_BUFF int = 64 * 1024 // 最大消息长度
)

var pidList []string

func StartTracing() {
	fmt.Println("start tracing")
	go traceAppInstances("mysqld")
}

func traceAppInstances(comm string) error {
	cmdStr := fmt.Sprintf("ps -ef | grep %s | grep -v grep", comm)
	matches, err := common.ExecShell(cmdStr)
	if err != nil {
		return err
	}

	for _, match := range matches {
		if len(match) > 0 {
			pid, err := strconv.Atoi(strings.Fields(match)[1])
			if err != nil {
				continue
			}
			pidList = append(pidList, strconv.Itoa(pid))
		}
	}

	if len(pidList) == 0 {
		return common.PrintOnlyErrMsg("not found app %s", comm)
	}
	pidList = append([]string{"mysql", strconv.Itoa(len(pidList))}, pidList...)

	err = notifyMysqlInstancesInfo(strings.Join(pidList, ","))
	if err != nil {
		return err
	}

	return nil
}

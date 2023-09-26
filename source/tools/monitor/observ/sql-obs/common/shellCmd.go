package common

import (
	"os/exec"
	"strings"
)

func ExecShell(cmdStr ...string) ([]string, error) {
    cmd := exec.Command("sh", "-c", cmdStr[0])
    out, err := cmd.Output()
    if err != nil {
        PrintSysError(err)
        return nil, err
    }
    if len(cmdStr) > 1 && cmdStr[1] == "origin" {
        return []string{string(out)}, nil
    }
    s := strings.Split(string(out), "\n")
    if len(s[len(s)-1]) == 0 {
        s = s[:len(s)-1]
    }
    return s, nil
}

func DetectSqlObs() error {
    strs, _ := ExecShell("ps -ef | grep sql-obs | grep -v grep")
    if len(strs) > 1 {
        return PrintOnlyErrMsg("Please exit the running sql-obs first, and then try again.")
    }
    return nil
}

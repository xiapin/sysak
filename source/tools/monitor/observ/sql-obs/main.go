package main

import (
    "fmt"
    "os"
    "os/signal"
    "sql-obs/common"
    "sql-obs/events"
    "sql-obs/logs"
    "sql-obs/metrics"
    "sql-obs/tracing"
    "syscall"
    "io/ioutil"
    "strings"
)

type ErrorCode = common.ErrorCode

func main() {
    if err := common.DetectSqlObs(); err != nil {
        return
    }
    outlineFilePath, err:= getSysomOutline()
    if err != nil {
        common.PrintOnlyErrMsg("Not get path of unity outline")
        return
    }
    if err := common.InitAppInstancesInfo("mysqld"); err != nil {
        common.PrintDefinedErr(ErrorCode(common.Fail_Init_Mysqld_Instances_Info))
        return
    }
    fmt.Println("start create a mysql connection")
    if err := events.CreateDBConnections(); err != nil {
        common.PrintDefinedErr(ErrorCode(common.Fail_Create_DB_Connect))
        return
    }
    sigCh := make(chan os.Signal, 1)
    signal.Notify(sigCh, syscall.SIGINT, syscall.SIGTERM)
    fmt.Println("start mysql observability")
    if err := common.InitDataExport(outlineFilePath); err != nil {
        common.PrintDefinedErr(
            ErrorCode(common.Fail_Init_Data_Export),
            "Please confirm if the 'unity' is activated")
        events.DestroyResource()
        return
    }
    defer events.DestroyResource()
    defer common.UninitDataExport()
    events.StartEventsCheck()
    logs.StartLogCollect()
    tracing.StartTracing()
    metrics.StartMonitor()
    e := <-sigCh
    fmt.Printf("exit mysql observability, signal: %v\n", e)
}

func getSysomOutline() (string, error) {
    pipeFile := ""
    yamlF := common.GetYamlFile()
    yamlContent, err := ioutil.ReadFile(yamlF)
    if err != nil {
        return "", err
    }
    lines := strings.Split(string(yamlContent), "\n")
    for i, line := range lines {
        if strings.HasPrefix(line, "#") {
            continue
        }
        if strings.Contains(line, "outline:") {
            if len(lines) > i+1 {
                outline := strings.Split(lines[i+1], " ")
                pipeFile = strings.TrimRight(outline[len(outline)-1], "\n")
                break
            }
        }
    }
    if pipeFile == "" {
        return "", common.PrintOnlyErrMsg(
            "Unable to get label \"outline\" in %s", yamlF)
    }
    return pipeFile, nil
}

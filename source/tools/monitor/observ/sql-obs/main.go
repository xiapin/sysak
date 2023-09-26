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
)

type ErrorCode = common.ErrorCode

func main() {
    if err := common.DetectSqlObs(); err != nil {
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
    if err := common.InitDataExport("/var/sysom/outline"); err != nil {
        common.PrintDefinedErr(
            ErrorCode(common.Fail_Init_Data_Export),
            "Please confirm if the 'unity' is activated")
        events.DestroyResource()
        return
    }
    events.StartEventsCheck()
    logs.StartLogCollect()
    tracing.StartTracing()
    metrics.StartMonitor()
    e := <-sigCh
    fmt.Printf("exit mysql observability, signal: %v\n", e)
    events.DestroyResource()
}

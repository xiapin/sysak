package logs

import (
    "fmt"
    //"os"
    //"time"
    //"strconv"
    "sql-obs/common"
    "sql-obs/events"
)

type DBConnect = common.DBConnect
type ErrorCode = common.ErrorCode

func StartLogCollect() {
    fmt.Println("start log check")
    go OsKernelLogChkStart()
    events.ForeachDBConnect(DetectMysqlErrorLog)
}

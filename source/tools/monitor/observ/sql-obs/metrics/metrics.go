package metrics

import (
    "fmt"
    "sql-obs/common"
    "sql-obs/events"
)

type DBConnect = common.DBConnect

func StartMonitor() {
    fmt.Println("start metrics monitor")
    go StartOsBaseMonitor()
    events.ForeachDBConnect(StartSqlMonitor)
}

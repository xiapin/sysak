package metrics

import (
    "fmt"
    "sql-obs/common"
    "strconv"
    "strings"
    "time"
    "sync"
)

var mysqldInlineMetricsTlbName string = "sysom_obser_metrics_mysqld_innodb"
var dataUploadLock sync.Mutex
var dataExport string
var accCount int

type InnodbMetrics struct {
    ThreadCached       uint64 `json:"threadCached"`
    ThreadCreated      uint64 `json:"threadCreated"`
    ThreadConnected    uint64 `json:"threadConnected"`
    ThreadRunning      uint64 `json:"threadRunning"`
    MaxConnection      uint64 `json:"maxConnection"`
    BufferPoolTotal    uint64 `json:"bufferPoolTotal"`
    BufferPoolFree     uint64 `json:"bufferPoolFree"`
    LongTransactionCnt uint64 `json:"longTransactionCnt"`
    HisListLen         uint64 `json:"hisListLen"`
    HisListMaxLen      uint64 `json:"hisListMaxLen"`
    HisListMinLen      uint64 `json:"hisListMinLen"`
    ChkPointUsage      uint64 `json:"chkPointUsage"`
    RedologCapacity    uint64 `json:"redologCapacity"`
}

func getConnectionInfo(dbConn *DBConnect, metric *InnodbMetrics) {
    sqlCmd := "show status like 'Threads_%'"
    data, err := dbConn.GetRowsByQueryCmd(sqlCmd)
    if err != nil {
        common.PrintDefinedErr(ErrorCode(common.Fail_Get_DB_Variables))
        return
    }

    for _, m := range data {
        if value, ok := m["Threads_cached"].(string); ok {
            metric.ThreadCached, _ = strconv.ParseUint(value, 10, 64)
        } else if value, ok := m["Threads_connected"].(string); ok {
            metric.ThreadConnected, _ = strconv.ParseUint(value, 10, 64)
        } else if value, ok := m["Threads_created"].(string); ok {
            metric.ThreadCreated, _ = strconv.ParseUint(value, 10, 64)
        } else if value, ok := m["Threads_running"].(string); ok {
            metric.ThreadRunning, _ = strconv.ParseUint(value, 10, 64)
        }
    }
}

func getConnectionLimit(dbConn *DBConnect, metric *InnodbMetrics) {
    sqlCmd := "show VARIABLES like 'max_connections'"
    data, err := dbConn.GetRowsByQueryCmd(sqlCmd)
    if err != nil {
        common.PrintDefinedErr(ErrorCode(common.Fail_Get_DB_Variables))
        return
    }
    for _, m := range data {
        if value, ok := m["max_connections"].(string); ok {
            metric.MaxConnection, _ = strconv.ParseUint(value, 10, 64)
        }
    }
}

func getBufferPoolInfo(dbConn *DBConnect, metric *InnodbMetrics) {
    sqlCmd := "show global status like '%Innodb_buffer_pool%'"
    data, err := dbConn.GetRowsByQueryCmd(sqlCmd)
    if err != nil {
        common.PrintDefinedErr(ErrorCode(common.Fail_Get_DB_Variables))
        return
    }
    var pageData uint64
    var byteData uint64
    var pageFree uint64
    var pageTotal uint64
    for _, m := range data {
        if value, ok := m["Innodb_buffer_pool_pages_data"].(string); ok {
            pageData, _ = strconv.ParseUint(value, 10, 64)
        } else if value, ok := m["Innodb_buffer_pool_bytes_data"].(string); ok {
            byteData, _ = strconv.ParseUint(value, 10, 64)
        } else if value, ok := m["Innodb_buffer_pool_pages_free"].(string); ok {
            pageFree, _ = strconv.ParseUint(value, 10, 64)
        } else if value, ok := m["Innodb_buffer_pool_pages_total"].(string); ok {
            pageTotal, _ = strconv.ParseUint(value, 10, 64)
        }
    }
    pageSize := byteData / pageData
    metric.BufferPoolTotal = pageTotal * pageSize
    metric.BufferPoolFree = pageFree * pageSize
}

func getLongTransactionCount(dbConn *DBConnect, shreshold uint32, metric *InnodbMetrics) {
    sqlCmd := fmt.Sprintf("select trx_id, TRX_MYSQL_THREAD_ID as thread_id, TIME_TO_SEC(timediff(now(),trx_started)) as time from information_schema.innodb_trx where TIME_TO_SEC(timediff(now(),trx_started))>%d ORDER BY time ASC", shreshold)
    data, err := dbConn.GetRowsByQueryCmd(sqlCmd)
    if err != nil {
        common.PrintDefinedErr(ErrorCode(common.Fail_Get_DB_Variables))
        return
    }
    var thread_id []string
    for _, m := range data {
        if value, ok := m["thread_id"].(string); ok {
            thread_id = append(thread_id, value)
        }
    }
    metric.LongTransactionCnt = uint64(len(thread_id))
}

func getHistoryListLength(dbConn *DBConnect, metric *InnodbMetrics) {
    sqlCmd := "SELECT count, max_count, min_count FROM information_schema.innodb_metrics WHERE name='trx_rseg_history_len'"
    data, err := dbConn.GetRowsByQueryCmd(sqlCmd)
    if err != nil {
        common.PrintDefinedErr(ErrorCode(common.Fail_Get_DB_Variables))
        return
    }
    for _, m := range data {
        if value, ok := m["count"].(string); ok {
            metric.HisListLen, _ = strconv.ParseUint(value, 10, 64)
        }
        if value, ok := m["max_count"].(string); ok {
            metric.HisListMaxLen, _ = strconv.ParseUint(value, 10, 64)
        }
        if value, ok := m["min_count"].(string); ok {
            metric.HisListMinLen, _ = strconv.ParseUint(value, 10, 64)
        }
    }
}

func getCheckPointAge(dbConn *DBConnect, metric *InnodbMetrics) {
    // get checkpoint age
    var ftd_lsn uint64
    var pg_lsn uint64
    sqlCmd := "show engine innodb status"
    data, err := dbConn.GetRowsListByQueryCmd(sqlCmd)
    if err != nil {
        common.PrintDefinedErr(ErrorCode(common.Fail_Get_DB_Variables))
        return
    }
    if len(*data) > 0 {
        lines := strings.Split((*data)[0], "\n")
        for _, line := range lines {
            if strings.HasPrefix(line, "Log flushed up to") {
                fields := strings.Fields(line)
                ftd_lsn, _ = strconv.ParseUint(fields[len(fields)-1], 10, 64)
            } else if strings.HasPrefix(line, "Pages flushed up to") {
                fields := strings.Fields(line)
                pg_lsn, _ = strconv.ParseUint(fields[len(fields)-1], 10, 64)
            }
        }
    }
    metric.ChkPointUsage = ftd_lsn - pg_lsn

    // get redo log capacity
    sqlCmd = `show variables like '%innodb_log_file%'`
    data2, err := dbConn.GetRowsByQueryCmd(sqlCmd)
    if err != nil {
        common.PrintDefinedErr(ErrorCode(common.Fail_Get_DB_Variables))
        return
    }
    var fileSize uint64
    var fileCount uint64
    for _, m := range data2 {
        if value, ok := m["innodb_log_file_size"].(string); ok {
            fileSize, _ = strconv.ParseUint(value, 10, 64)
        } else if value, ok := m["innodb_log_files_in_group"].(string); ok {
            fileCount, _ = strconv.ParseUint(value, 10, 64)
        }
    }
    metric.RedologCapacity = fileSize * fileCount
}

func addDataToExport(data string, exportThresh int) {
    dataUploadLock.Lock()
    if len(dataExport) > 0 {
        dataExport += "\n"
    }
    dataExport += data
    accCount++
    if accCount >= exportThresh {
        common.ExportData(dataExport)
        accCount = 0
        dataExport = ""
    }
    dataUploadLock.Unlock()
}

func startEngineMonitor(dbConn *DBConnect) {
    var innodbMetrics InnodbMetrics
    for {
        getConnectionInfo(dbConn, &innodbMetrics)
        getConnectionLimit(dbConn, &innodbMetrics)
        getBufferPoolInfo(dbConn, &innodbMetrics)
        getLongTransactionCount(dbConn, 1, &innodbMetrics)
        getHistoryListLength(dbConn, &innodbMetrics)
        getCheckPointAge(dbConn, &innodbMetrics)
        addDataToExport(mysqldInlineMetricsTlbName + `,podID=` +
            dbConn.GetPodID() + `,containerID=` + dbConn.GetContainerID() +
            `,port=` + strconv.Itoa(dbConn.GetPort()) + `,comm=mysqld ` + 
            common.Struct2String(innodbMetrics),
            common.GetAppInstanceCnt())
        time.Sleep(60 * time.Second)
    }
}

func StartSqlMonitor(dbConn *DBConnect) {
    if dbConn.DBConnIsVaild() {
        go startEngineMonitor(dbConn)
    }
}

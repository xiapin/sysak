package analyzer

import (
    "fmt"
    "sql-obs/common"
    //"os"
    "time"
    "strconv"
)

func startMonitorSlowLog(slowQueryLogFile string, A *ssAnalyzer) {
    fw := common.NewFileWriteWatcher(slowQueryLogFile, 0)
    fw.StartWatch()
    for {
            status := <-fw.Status()
            if status == common.Has_data {
                A.CopyDataToAnalyzer(fw.Data(), fw.ChangeLines())
            } else if status == common.Watcher_Exited {
                A.ExitAnalyzer()
                common.PrintDefinedErr(ErrorCode(common.Fail_File_Watcher_Exit))
                return
            }
    }
}

func getSlowQueryLogFile(dbConn *DBConnect) string {
    var slowQueryLog string
    var slowQueryLogFile string
    match := map[string]interface{} {
        "Comm": "mysqld",
        "Ip": dbConn.GetIp(),
        "Port": dbConn.GetPort(),
    }

    if dbConn.DBConnIsVaild() {
        sqlCmd := "SHOW VARIABLES LIKE '%slow_query_log%'"
        data, err := dbConn.GetRowsByQueryCmd(sqlCmd)
        if err != nil {
            common.PrintDefinedErr(ErrorCode(common.Fail_Get_DB_Variables))
            return ""
        }

        for _, m := range data {
            if value, ok := m["slow_query_log"].(string); ok {
                slowQueryLog = value
            } else if value, ok := m["slow_query_log_file"].(string); ok {
                slowQueryLogFile = value
            }
        }

        if slowQueryLog == "OFF" {
            common.PrintDefinedErr(ErrorCode(common.Fail_Slow_Log_Is_OFF))
            slowQueryLogFile = ""
        }
        if len(slowQueryLogFile) > 0 {
            ajust := map[string]interface{} {
                "Slowlog": slowQueryLogFile,
            }
            common.AppInstancesAjustMember(match, ajust)
        }
    }
    return common.GetAppInstanceInfo(match, "Slowlog").(string)
}

/**
 * Slow SQL is achieved by listening to slow log files
 */
 func detectSlowSqlBySlowLog(dbConn *DBConnect) {
    slowQueryLogFile := getSlowQueryLogFile(dbConn)
    if len(slowQueryLogFile) > 0 {
        startMonitorSlowLog(slowQueryLogFile, NewSlowSqlAnalyzer(dbConn))
    }
 }

func detectSlowSqlBySlowQuerys(dbConn *DBConnect) {
    var slowQueries int
    retry := 0
    sqlCmd := "SHOW GLOBAL STATUS LIKE 'Slow_queries'"
    for {
        data, err := dbConn.GetRowsByQueryCmd(sqlCmd)
        if err != nil {
            common.PrintDefinedErr(ErrorCode(common.Fail_Get_DB_Variables))
            break
        }
        if value, ok := data[0]["Slow_queries"].(string); ok {
            slowQueries, _ = strconv.Atoi(value)
            fmt.Printf("Slow_queries is %v\n", slowQueries)
            retry = 0
            goto loop
        }
        retry++
        if retry >= 5 {
            common.PrintDefinedErr(ErrorCode(common.Fail_Get_DB_Variables))
            break
        }
    loop:
        time.Sleep(600 * time.Second)
    }
}

func detectSlowSql(dbConn *DBConnect) {
    go detectSlowSqlBySlowLog(dbConn)
    if dbConn.DBConnIsVaild() {
        go detectSlowSqlBySlowQuerys(dbConn)
    }
}

/**
 * Obtaining SQL latency by tracking SQL requests
 */
/*
func detectBadSql(data ...interface{}) {
    fmt.Println("check bad sql events")
    for _, d := range data {
        fmt.Println(d)
    }
}*/

func DetectSlowOrBadSql(dbConn *DBConnect) {
    detectSlowSql(dbConn)
}

func RegisterSqlExceptChkNotify() {
    //RegisterNotify(Notify_Process_RT_Type, detectBadSql)
}

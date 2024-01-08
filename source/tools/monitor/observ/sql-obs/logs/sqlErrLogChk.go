package logs

import (
    "fmt"
    "sql-obs/common"
    "sql-obs/events/analyzer"
    "strings"
    //"os"
    "time"
)

/**
 * detect mysql err_log file
 */
 func detectErrorLog(dbConn *DBConnect) {
    var logErrorFileName string
    match := map[string]interface{} {
        "Comm": "mysqld",
        "Ip": dbConn.GetIp(),
        "Port": dbConn.GetPort(),
    }

    if dbConn.DBConnIsVaild() {
        sqlCmd := "SHOW VARIABLES LIKE 'log_error'"
        data, err := dbConn.GetRowsByQueryCmd(sqlCmd)
        if err != nil {
            common.PrintDefinedErr(ErrorCode(common.Fail_Get_DB_Variables))
            return
        }
        if value, ok := data[0]["log_error"].(string); ok {
            logErrorFileName = value
            ajust := map[string]interface{} {
                "Errlog": logErrorFileName,
            }
            common.AppInstancesAjustMember(match, ajust)
        }
    }
    logErrorFileName = 
        common.GetAppInstanceInfo(match, "Errlog").(string)
    if len(logErrorFileName) > 0 {
        fw := common.NewFileWriteWatcher(logErrorFileName, 0)
        fw.StartWatch()
        for {
			status := <-fw.Status()
			if status == common.Has_data {
				lines := fw.ChangeLines()
				for i := 0; i < lines; i++ {
                    data := fw.Data()[i]
                    if len(data) > 0 {
                        level := ""
                        if strings.Contains(data, "ERROR") {
                            level = "error"
                        } else if strings.Contains(data, "CRITICAL") {
                            level = "fatal"
                        }
                        if len(level) > 0 {
                            nowFormat := time.Unix(
                                time.Now().Unix(), 0).Format(common.TIME_FORMAT)
                            pid := common.GetAppInstanceInfo(
                                map[string]interface{}{
                                    "ContainerId": dbConn.GetContainerID(),
                                    "Port": dbConn.GetPort(),
                                    "Comm": "mysqld"},
                                "Pid").(int)
                            extra := fmt.Sprintf(`{"level":"%s"` +
                                `,"value":"%s"` +
                                `,"ts":"%s"` +
                                `,"tag_set":"%s"` +
                                `,"pid":"%d"` +
                                `,"podId":"%s"` +
                                `,"containerId":"%s"}`,
                                level, data, nowFormat, "mysqld", pid,
                                dbConn.GetPodID(), dbConn.GetContainerID())
                            analyzer.SubmitAlarm(analyzer.GetLogEventsDesc(
                                analyzer.Notify_Process_Mysql_Error_Type,
                                level, "", "Error occurred in mysqld", extra))
                        }
                    }
				}
			} else if status == common.Watcher_Exited {
                common.PrintDefinedErr(ErrorCode(common.Fail_File_Watcher_Exit))
                return
            }
		}
    } else {
        common.PrintOnlyErrMsg("not found logErrorFile for %s in %s", 
            match["Comm"], match["Ip"])
        return
    }
}

func DetectMysqlErrorLog(dbConn *DBConnect) {
	go detectErrorLog(dbConn)
}

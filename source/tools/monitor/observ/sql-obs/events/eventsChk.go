package events

import (
	"fmt"
	"sql-obs/common"
	"sql-obs/events/analyzer"
)

type DBConnect = common.DBConnect
type ErrorCode = common.ErrorCode

var dbConnList []*DBConnect

func ForeachDBConnect(f func(*DBConnect)) {
	for _, dbConn := range dbConnList {
		f(dbConn)
	}
}

func startMysqlExceptCheck() {
	ForeachDBConnect(analyzer.DetectSlowOrBadSql)
	analyzer.RegisterSqlExceptChkNotify()
}

func startOSExceptCheck() {
	analyzer.OsChkStart()
}

func StartEventsCheck() {
	fmt.Println("start events check")
	analyzer.StartEventNotify()
	analyzer.InitAlarmManage()
	startOSExceptCheck()
	startMysqlExceptCheck()
}

func CreateDBConnections() error {
	userInvaild := false
	user, passwd := common.GetUsersInfo()
	if user == "" && passwd == "" {
		userInvaild = true
	}
	common.ForeachAppInstances("mysqld", []string{"Ip", "Port", "ContainerId", "PodId"},
		func(values []interface{}) {
			dbConn, err := common.NewDBConnection("mysql",
				values[0].(string),
				values[1].(int),
				values[2].(string),
				values[3].(string))
			if err != nil {
				common.PrintDefinedErr(ErrorCode(common.Fail_Create_DB_Connect))
				return
			}
			if !userInvaild {
				common.ConnectToDB(dbConn, user, passwd)
			}
			dbConnList = append(dbConnList, dbConn)
		})
	return nil
}

func destroyDBConnection() {
	for _, dbConn := range dbConnList {
		dbConn.CloseDBConnection()
	}
}

func DestroyResource() {
	destroyDBConnection()
	analyzer.DestroyAlarmResource()
}

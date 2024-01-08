package analyzer

import (
    //"fmt"
    "sql-obs/common"
    //"strings"
    //"os"
)

type DBConnect = common.DBConnect
type ErrorCode = common.ErrorCode

type handler func(data *[]string, dataLen int, pri *interface{})
type Analyzer struct {
    status chan int
    data []string
    dataLen int
    handleData handler
    private *interface{}
}

const (
    Analyzer_WakeUp = 1
    Analyzer_Exited
)

func (A *Analyzer) defaultAnalyzerHandler() {
    for {
        status := <-A.status
        if status == Analyzer_WakeUp {
            A.handleData(&A.data, A.dataLen, A.private)
        } else if status == Analyzer_Exited {
            common.PrintDefinedErr(
				ErrorCode(common.Fail_Slow_Sql_Analyzer_Exit))
            return
        }
    }
}

func (A *Analyzer) wakeUpAnalyzer() {
    A.status <- Analyzer_WakeUp
}

func (A *Analyzer) ExitAnalyzer() {
    A.status <- Analyzer_Exited
}

/*
func (A *Analyzer) MatchDBConnectTOAnalyzer(dbConn *DBConnect) {
    A.dbConn = dbConn
}*/

func (A *Analyzer) CopyDataToAnalyzer(data []string, dataLen int) {
	if A.data == nil {
        A.data = make([]string, len(data))
	}
    copy(A.data, data[:dataLen])
    A.dataLen = dataLen
    A.wakeUpAnalyzer()
}

func NewAnalyzer(h handler) (*Analyzer) {
    A := Analyzer{
        status: make(chan int),
        data: nil,
        dataLen: 0,
        handleData: h,
    }
    go A.defaultAnalyzerHandler()
    return &A
}

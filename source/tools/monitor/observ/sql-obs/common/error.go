package common

import (
    "errors"
    "fmt"
    "runtime"
    "strings"
)

type ErrorCode int

type DefinedErr struct {
    err ErrorCode
    errMsg string
}

const (
    Success = 0
    Fail_Create_DB_Connect = iota - 258
    Fail_Get_DB_Variables
    Fail_Slow_Log_Is_OFF
    Fail_File_Watcher_Exit
    Fail_Slow_Sql_Analyzer_Exit
    Fail_Unrecognized_Slow_Log_Format
    Fail_Analyzer_Data_Formatting_JSON
	Fail_Get_MySQLD_Proc
    Fail_Upload_Data
    Fail_Notify_Not_Register
    Fail_Init_Data_Export
    Fail_Init_Mysqld_Instances_Info
)

var dErrTlb = []DefinedErr {
    {err: Fail_Create_DB_Connect, errMsg: "create mysql connection fail"},
    {err: Fail_Get_DB_Variables, errMsg: "get mysql variable fail"},
    {err: Fail_Slow_Log_Is_OFF, errMsg: "slow_query_log is OFF, please turn on"},
    {err: Fail_File_Watcher_Exit, errMsg: "file watcher is exited"},
    {err: Fail_Unrecognized_Slow_Log_Format, errMsg: "unrecognized slow log format"},
    {err: Fail_Analyzer_Data_Formatting_JSON, errMsg: "analyzer data formatting JSON failed"},
	{err: Fail_Get_MySQLD_Proc, errMsg: "mysqld process doesn't exist"},
    {err: Fail_Upload_Data, errMsg: "fail to send data by unix.sock"},
    {err: Fail_Notify_Not_Register, errMsg: "notify not register"},
    {err: Fail_Init_Data_Export, errMsg: "fail to init data export"},
    {err: Fail_Init_Mysqld_Instances_Info, errMsg: "fail to get Mysqld Instances Info"},
}

type MyError struct {
    err error
    fun string
    file string
    line int
}

const Max_Print_Error_Cnt = 50
var printErrorLimit int = 0

func (e *MyError) Error() string {
    return fmt.Sprintf("[%s:%d] %s: %s", e.file, e.line, e.fun, e.err.Error())
}

func checkPrintLimit() bool {
    if printErrorLimit >= (Max_Print_Error_Cnt + 1) {
        if printErrorLimit == (Max_Print_Error_Cnt + 1) {
            PrintOnlyErrMsg("start limit error log output")
        }
        return true
    }
    return false
}

func newMyError(err error, skip ...int) error {
    printErrorLimit += 1
    skipTrace := 2
    if len(skip) > 0 {
        skipTrace = skip[0]
    }
    funPc, file, line, _ := runtime.Caller(skipTrace)
    moduleName := "sql-obs"
    s := strings.Split(file, moduleName)
    relativePath := moduleName + s[1]
    return &MyError{
        err: err,
        fun: runtime.FuncForPC(funPc).Name(),
        file: relativePath,
        line: line,
    }
}

func PrintOnlyErrMsg(errMsg string, a ...interface{}) error {
    err := newMyError(errors.New("error: " + fmt.Sprintf(errMsg, a...)), 2)
    if !checkPrintLimit() {
        fmt.Println(err)
    }
    return err
}

func PrintDefinedErr(err ErrorCode, msg ...string) {
    errMsg := "unkown error"
    if checkPrintLimit() {
        return
    }
    for i := 0; i < len(dErrTlb); i++ {
        if err == dErrTlb[i].err {
            errMsg = dErrTlb[i].errMsg
            break
        }
    }
    if len(msg) > 0 {
        errMsg += "\n"
        for _, m := range msg {
            errMsg += m
        }
    }
    fmt.Println(newMyError(errors.New("error: "+errMsg)))
}

func PrintSysError(err error) {
    if checkPrintLimit() {
        return
    }
    fmt.Println(newMyError(err))
}

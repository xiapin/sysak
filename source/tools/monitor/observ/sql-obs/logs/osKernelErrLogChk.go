package logs

import (
    "sql-obs/common"
    "sql-obs/events/analyzer"
    // "fmt"
    // "os"
    // "time"
    // "strconv"
    "strings"
)

type MetricsType = analyzer.MetricsType

//kernel log err Events:
//          OOM log
//          IO error log: IO timeout、Scsi/nvme error
//          Net error log: network link up/down、syn overflow
//          Filesystem readonly/error log
func osChkKernelLogErrEvents() {
    //fmt.Println("start OS kernel error log events check...")
    fw := common.NewFileWriteWatcher("/dev/kmsg", 0)
    fw.StartWatch()
    for {
        status := <-fw.Status()
        if status == common.Has_data {
            lines := fw.ChangeLines()
            for i := 0; i < lines; i++ {
                d := fw.Data()[i]
                errType := ""
                mType := 0
                // Usually, when these kmsg errors occur,
                // it requires people to fix them
                if (strings.Contains(d, "blk_update_request") &&
                    strings.Contains(d, "error")){
                    errType = "block I/O error"
                    mType = analyzer.Notify_IO_Error_Type
                } else if (
                    (strings.Contains(d, "reset controller") || 
                     strings.Contains(d, "disable controller")) && 
                     strings.Contains(d, "nvme")) ||
                    ((strings.Contains(d, "exception Emask") || 
                      strings.Contains(d, "failed command") ||
                      strings.Contains(d, "limiting SATA link")) &&
                      strings.Contains(d, "ata")) {
                    errType = "hardwaer I/O error"
                    mType = analyzer.Notify_IO_Error_Type
                } else if strings.Contains(d, "read-only") &&
                        (strings.Contains(d, "filesystem") ||
                        strings.Contains(d, "mode")) {
                    errType = "FS read-only error"
                    mType = analyzer.Notify_FS_Error_Type
                } else if strings.Contains(d, "Link") && 
                          strings.Contains(d, "Down") &&
                          strings.Contains(d, "eth") {
                    errType = "Net hardware error"
                    mType = analyzer.Notify_Net_Link_Down_Type
                } else if strings.Contains(d, "Killed process") {
                    errType = "OOM"
                    mType = analyzer.Notify_Process_OOM_Type
                }
                if len(errType) > 0 {
                    analyzer.MarkEventsNotify(MetricsType(mType), errType, d)
                    analyzer.TriggerNotify()
                }
            }
        } else if status == common.Watcher_Exited {
            common.PrintDefinedErr(ErrorCode(common.Fail_File_Watcher_Exit))
            return
        }
    }
}

func OsKernelLogChkStart() {
    go osChkKernelLogErrEvents()
}

package analyzer

import (
    "fmt"
    "sql-obs/common"
)

type MetricsType int

const (
    Notify_IO_Burst_Type = iota
    Notify_IO_Delay_Type
    Notify_IO_Wait_Type
    Notify_IO_Except_Type
    Notify_Long_Time_D_Type
    Notify_Direct_Reclaim_Type
    Notify_Memleak_Type
    Notify_IO_Error_Type
    Notify_FS_Error_Type
    Notify_Net_Link_Down_Type
    Notify_Process_OOM_Type
    Notify_IO_Hang_Type
    Notify_OS_Lockup_Type
    Notify_Process_RT_Type
    Notify_Process_Sched_Delay_Type
    Notify_Process_Net_Drops_Type
    Notify_Process_Mysql_Slow_Sql_Type
    Notify_Process_Mysql_Error_Type
    Notify_Process_CPU_HIGH_Type

    Notify_Type_Max
)

var mTypeStrTlb = []string{
    "Notify_IO_Burst_Type",
    "Notify_IO_Delay_Type",
    "Notify_IO_Wait_Type",
    "Notify_IO_Except_Type",
    "Notify_Long_Time_D_Type",
    "Notify_Direct_Reclaim_Type",
    "Notify_Memleak_Type",
    "Notify_IO_Error_Type",
    "Notify_FS_Error_Type",
    "Notify_Net_Link_Down_Type",
    "Notify_Process_OOM_Type",
    "Notify_IO_Hang_Type",
    "Notify_OS_Lockup_Type",
    "Notify_Process_RT_Type",
    "Notify_Process_Sched_Delay_Type",
    "Notify_Process_Net_Drops_Type",
    "Notify_Process_Mysql_Slow_Sql_Type",
    "Notify_Process_Mysql_Error_Type",
    "Notify_Process_CPU_HIGH_Type",
}

type notifyHandler func(data []interface{})
type eventsNotify struct {
    trigger chan bool
    hList   []notifyHandler
    data    []interface{}
    notify  []bool
}

var eNotfiy *eventsNotify

func MarkEventsNotify(mType MetricsType, data ...interface{}) {
    eNotfiy.notify[mType] = true
    eNotfiy.data[mType] = data
}

func TriggerNotify() {
    eNotfiy.trigger <- true
}

func RegisterNotify(mType MetricsType, nh notifyHandler) {
    eNotfiy.hList[mType] = nh
}

func (e *eventsNotify) runNotifyHandler() {
    fmt.Println("run events notify handler")
    for {
        trigger := <-e.trigger
        if trigger {
            for mType, notify := range e.notify {
                if notify {
                    e.notify[mType] = false
                    if e.hList[mType] == nil {
                        common.PrintDefinedErr(
                            ErrorCode(common.Fail_Notify_Not_Register),
                            "events notify type:"+mTypeStrTlb[int(mType)])
                        continue
                    }
                    e.hList[mType](e.data[mType].([]interface{}))
                }
            }
        }
    }
}

func StartEventNotify() {
    eNotfiy = &eventsNotify{
        trigger: make(chan bool),
        hList:   make([]notifyHandler, Notify_Type_Max),
        data:    make([]interface{}, Notify_Type_Max),
        notify:  make([]bool, Notify_Type_Max),
    }
    fmt.Println("start events notify")
    go eNotfiy.runNotifyHandler()
}

package analyzer

import (
    "fmt"
    //"unsafe"
    //"encoding/json"
    "sql-obs/common"
    //"os"
    "strings"
    //"strconv"
)

func AnalyzOSEvents(podId string, containerId string,
    pid int) map[string]string {
    getAlarmMetrics := func(alarmType int, podId string, containerId string,
        pid int) (map[string]string, bool) {
        alarm := GetAlarmDescs(alarmType)
        if alarm != nil {
            for _, a := range alarm {
                if podId == a["podId"] &&
                    containerId == a["containerId"] &&
                    strings.Contains(a["tag_set"], "mysqld") {
                    return a, true
                }
            }
        }
        return nil, false
    }
    getAlarmLogIO := func(alarmType int, podId string, containerId string,
        pid int) (map[string]string, bool) {
        alarm := GetAlarmDescs(alarmType)
        if alarm != nil {
            diskList := common.GetAppInstanceMemberByPid(
                pid, "PvDevice")
            if diskList != "" {
                for _, disk := range diskList.([]string) {
                    for _, a := range alarm {
                        if _, ok := a["disk"]; ok && strings.Contains(disk, a["disk"]) {
                            return a, true
                        }
                    }
                }
            }
        }
        return nil, false
    }
    getAlarmLog := func(alarmType int, podId string, containerId string,
        pid int) (map[string]string, bool) {
        alarm := GetAlarmDescs(alarmType)
        if alarm != nil {
            for _, a := range alarm {
                return a, true
            }
        }
        return nil, false
    }

    //rtDesc, rtLarge := getAlarmMetrics(
    //    Notify_Process_RT_Type, podId, containerId, pid)
    sdDesc, sdLarge := getAlarmMetrics(
        Notify_Process_Sched_Delay_Type, podId, containerId, pid)
    dtDesc, dtLarge := getAlarmMetrics(
        Notify_Long_Time_D_Type, podId, containerId, pid)
    dropDesc, dropLarge := getAlarmMetrics(
        Notify_Process_Net_Drops_Type, podId, containerId, pid)
    ioHDesc, ioHLarge := getAlarmLogIO(
        Notify_IO_Hang_Type, podId, containerId, pid)
    ioBDesc, ioBLarge := getAlarmLogIO(
        Notify_IO_Burst_Type, podId, containerId, pid)
    ioDDesc, ioDLarge := getAlarmLogIO(
        Notify_IO_Delay_Type, podId, containerId, pid)
    ioWDesc, ioWLarge := getAlarmLog(
        Notify_IO_Wait_Type, podId, containerId, pid)
    drDesc, drDLarge := getAlarmLog(
        Notify_Direct_Reclaim_Type, podId, containerId, pid)

    if ioHLarge {
        return ioHDesc
    } else if ioDLarge {
        if ioBLarge {
            return ioBDesc
        }
        return ioDDesc
    } else if ioBLarge {
        return ioBDesc
    } else if drDLarge {
        return drDesc
    } else if ioWLarge {
        return ioWDesc
    } else if sdLarge {
        return sdDesc
    } else if dtLarge {
        return dtDesc
    } else if dropLarge {
        return dropDesc
    } else {
        return nil
    }
}

func NewOSExceptAnalyzer() {
    fmt.Println("new OS except analyzer...")
}

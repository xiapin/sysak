package analyzer

import (
    "fmt"
    "math"
    "sql-obs/common"
    "time"

    //"os"
    //"time"
    "regexp"
    "strconv"
    "strings"
    "encoding/json"
)

var osEventsTable string = "mysql_observ_osEvents"
var aggregator map[string]*common.DataItem = map[string]*common.DataItem{}
var config OsCheckConfig

type defineDataExport func(args ...interface{})

func initSlideWindow(configPath string) {
    setOsCheckConfig(&config, configPath)
    common.AddItem(aggregator, "iowait", config.Window)
}

// data is [containerId, comm, pid, data1, data2, ...]
// if process not in container, containerId should be ""
func chkProcessMetricsGeneral(alarmType int, metricsName string,
    msgFormat string, thresh float32, data []interface{}, 
    exportHandler ...defineDataExport) {
    cidIdx := 0
    commIdx := 1
    pidIdx := 2
    dataStartIndex := 3
    if len(data) < (dataStartIndex + 1) {
        return
    }
    metrics := data[dataStartIndex].(float64)
    if float32(metrics) > thresh {
        comm := data[commIdx].(string)
        pid := data[pidIdx].(string)
        format := "%s(%s) " + msgFormat
        desc := fmt.Sprintf(format, comm, pid, metrics)
        extraContainerId := "NULL"
        extraPodId := "NULL"
        if len(data[cidIdx].(string)) > 0 {
            extraContainerId = data[cidIdx].(string)
            pidInt, _ := strconv.Atoi(pid)
            extraPodId = common.GetAppInstanceMemberByPid(
                pidInt, "PodId").(string)
            if len(extraPodId) == 0 {
                extraPodId = "NULL"
            }
        }
        // add container ID
        if len(exportHandler) > 0 {
            for _, handler := range exportHandler {
                handler(alarmType, comm, pid, extraPodId, extraContainerId,
                    metricsName, desc)
            }
        } else {
            SubmitAlarm(GetMetricsEventsDesc(alarmType, comm,
                pid, extraPodId, extraContainerId, metricsName,
                desc))
        }
    }
}

// IO Events:
// IO Wait High, data should be [global iowait]
func osChkIOWaitEvents(data []interface{}) {
    if len(data) > 0 {
        iowait := data[0].(float64)
        if iowait >= float64(config.Iowait) {
            common.DisableThreshComp(aggregator, "iowait")
        }
        // detect iowait exception
        minThresh := float64(config.Iowait)
        iowaitThresh := math.Max(
            common.GetDynThresh(aggregator, "iowait"), minThresh)
        if iowait > iowaitThresh {
            SubmitAlarm(GetLogEventsDesc(Notify_IO_Wait_Type,
                "warning", "os", "IO wait high"))
        }
        common.UpdateDynThresh(aggregator, "iowait", iowait)
    }
}

// IO Burst & IO Hang & IO Delay
// data should be [dev, await, util, iops, bps, qusize]
// [devname, busy, reads, writes, rmsec, wmsec, rkb, wkb, backlog]
func osChkIOExceptEvents(data []interface{}) {
    data = data[0].([]interface{})
    if len(data) >= 1 {
        for _, v := range data {
            value := v.([]interface{})
            devname := value[0].(string)
            util := value[1].(float64)
            iops := value[2].(float64) + value[3].(float64)
            await := (value[4].(float64) + value[5].(float64))
            if iops > 0 {
                await /= iops
            }
            bps := value[6].(float64) + value[7].(float64)
            qusize := value[8].(float64) / 1000.0
            fieldSlice := []string{"util", "await", "iops", "bps"}
            for _, item := range fieldSlice {
                if _, ok := aggregator[devname+item]; !ok {
                    common.AddItem(aggregator, devname+item, config.Window)
                }
            }
            osChkIOUtilEvents(devname, util, iops, bps, qusize)
            osChkIODelayEvents(devname, await)
        }
    }
}

// func debugIOburstDyn(devname string, iops float64, bps float64) {
//     data := ""
//     LowW := map[string]uint32{"iops": config.Iops, "bps": config.Bps}
//     for str, val := range map[string]float64{"iops": iops, "bps": bps} {
//         HighW := math.Max(
//             common.GetDynThresh(aggregator, devname+str), float64(LowW[str]))
//         fieldCurr := "curr" + str
//         currValue := uint64(val)
//         fieldThresh := str + "Thresh"
//         currThresh := uint64(HighW)
//         Basethresh := uint64(common.GetBaseThresh(aggregator, devname+str))
//         fieldBasethresh := str + "BaseThresh"
//         MoveAvg := uint64(common.GetMoveAvg(aggregator, devname+str))
//         fieldMoveAvg := str + "MoveAvg"
//         ComThresh := uint64(common.GetComThresh(aggregator, devname+str))
//         fieldComThresh := str + "ComThresh"
//         data += fmt.Sprintf(`%s=%d,%s=%d,%s=%d,%s=%d,%s=%d,`,
//             fieldCurr, currValue, fieldThresh, currThresh, fieldBasethresh,
//             Basethresh, fieldMoveAvg, MoveAvg, fieldComThresh, ComThresh)
//     }
//     data = strings.TrimRight(data, ",")
//     d := (`debugDynThreshTlb,disk=` + devname + ` ` + data)
//     common.ExportData(d)
// }

// IO Burst & IO Hang
// data should be [dev, util, iops, bps, qusize]
func osChkIOUtilEvents(
    devname string, util float64, iops float64, bps float64, qusize float64) {
    ioburst := false
    utilMinThresh := config.Util

    utilThresh := math.Max(
        common.GetDynThresh(aggregator, devname+"util"), float64(utilMinThresh))
    if util > utilThresh {
        ioburst = osChkIOBurstEvents(devname, bps, iops)
        if !ioburst {
            osChkIOHangEvents(devname, util, iops, qusize)
        }
    }
    // debugIOburstDyn(devname, iops, bps)
    common.UpdateDynThresh(aggregator, devname+"util", util)
    common.UpdateDynThresh(aggregator, devname+"iops", iops)
    common.UpdateDynThresh(aggregator, devname+"bps", bps)
}

// IO Burst, data should be [dev, util, iops, bps]
func osChkIOBurstEvents(devname string, bps float64, iops float64) bool {
    bpsLowW := config.Bps
    bpsHighW := math.Max(common.GetDynThresh(aggregator, devname+"bps"), float64(bpsLowW))
    bpsMidW := math.Max(float64(bpsLowW), bpsHighW/2)

    iopsLowW := config.Iops
    iopsHighW := math.Max(common.GetDynThresh(aggregator, devname+"iops"), float64(iopsLowW))
    iopsMidW := math.Max(float64(iopsLowW), iopsHighW/2)

    // fmt.Println(devname, bpsHighW, bpsMidW, iopsLowW, iopsHighW, iopsMidW)
    ioburst := false
    if iops >= iopsMidW || bps >= bpsMidW {
        ioburst = true
    }

    iopsOver := false
    if iops >= iopsHighW {
        iopsOver = true
    }

    bpsOver := false
    if bps >= bpsHighW {
        bpsOver = true
    }

    if iopsOver || bpsOver {
        fieldCurr := "currIops"
        currValue := iops
        if bpsOver {
            fieldCurr = "currBps"
            currValue = bps
        }
        // ioburst, put data: dev
        desc := fmt.Sprintf("IO burst in disk %s", devname)
        extra := fmt.Sprintf(`{"level":"warning"`+
            `,"value":"%s"`+
            `,"ts":"%s"`+
            `,"disk":"%s"`+
            `,"%s":"%d"`+
            `,"tag_set":"os"}`,
            desc, time.Unix(time.Now().Unix(), 0).Format(common.TIME_FORMAT), devname,
            fieldCurr, uint64(currValue))
        SubmitAlarm(GetLogEventsDesc(
            Notify_IO_Burst_Type, "", "", desc, extra))
    }
    return ioburst
}

// IO Hang, data should be [dev, util, iops, bps, qusize]
func osChkIOHangEvents(devname string, util float64, iops float64, qusize float64) {
    if util >= 99 && qusize >= 1 && iops < 50 {
        desc := fmt.Sprintf("IO hang in disk %s", devname)
        extra := fmt.Sprintf(`{"level":"fatal"`+
            `,"value":"%s"`+
            `,"ts":"%s"`+
            `,"disk":"%s"`+
            `,"tag_set":"os"}`,
            desc, time.Unix(time.Now().Unix(), 0).Format(common.TIME_FORMAT), devname)
        SubmitAlarm(GetLogEventsDesc(
            Notify_IO_Hang_Type, "", "", desc, extra))
    }
}

// IO Delay High, data should be [dev, await]
func osChkIODelayEvents(devname string, await float64) {
    awaitMinThresh := config.Await
    awaitThresh := math.Max(
        common.GetDynThresh(aggregator, devname+"await"), float64(awaitMinThresh))
    if await >= awaitThresh {
        desc := fmt.Sprintf("IO delay high in disk %s", devname)
        extra := fmt.Sprintf(`{"level":"warning"`+
            `,"value":"%s"`+
            `,"ts":"%s"`+
            `,"disk":"%s"`+
            `,"curr":"%d"`+
            `,"threshold":"%d"`+
            `,"tag_set":"os"}`,
            desc, time.Unix(time.Now().Unix(), 0).Format(common.TIME_FORMAT), devname,
                int32(await), int32(awaitThresh))
        SubmitAlarm(GetLogEventsDesc(
            Notify_IO_Delay_Type, "", "", desc, extra))
    }
    common.UpdateDynThresh(aggregator, devname+"await", await)
}

func chkKmsgErrorLogGenaral(alarmType int, data []interface{}) {
    if len(data) > 1 {
        SubmitAlarm(GetLogEventsDesc(alarmType,
            "warning", "os", data[0].(string), data[1].(string)))
    }
}

// IO error, data should be dmesg errlog about io
// contains "timeout error","I/O error"
// contains
func osChkIOErrEvents(data []interface{}) {
    chkKmsgErrorLogGenaral(Notify_IO_Error_Type, data)
}

// FS error, data should be dmesg errlog about FS
func osChkFilesystemErrEvents(data []interface{}) {
    chkKmsgErrorLogGenaral(Notify_FS_Error_Type, data)
}

// Net Events:
// pkg droped, data should be[containerId, comm, pid, process drops/retran]
func osChkNetDropEvents(data []interface{}) {
    chkProcessMetricsGeneral(Notify_Process_Net_Drops_Type, "pkgDrops",
        "pkg drops %.1f packet loss", 0, data)
}

// net link down, data should be dmesg for net
func osChkNetLinkDownEvents(data []interface{}) {
    if len(data) > 1 {
        re := regexp.MustCompile("ethd")
        ethx := re.FindString(data[1].(string))
        if strings.Contains(ethx, "eth") {
            SubmitAlarm(GetLogEventsDesc(Notify_Net_Link_Down_Type,
                "warning", "os", data[0].(string), ethx+` link down`))
        }
    }
}

// process RT check, data should be[containerId, comm, pid, process RT95, process RT99]
// if process not in container, containerId should be ""
func osChkNetProcessRTEvents(data []interface{}) {
    rtEventExportHandler := func(argvs ...interface{}) {
        appLog := "{}"
        jOSEve := "{}"
        reason := "-"
        nowFormat := time.Unix(time.Now().Unix(), 0).Format(common.TIME_FORMAT)
        pid,_ := strconv.Atoi(argvs[2].(string))
        osEve := AnalyzOSEvents(argvs[3].(string), argvs[4].(string), pid)
        if osEve != nil {
            if _, ok := osEve["value"]; ok {
                reason = osEve["value"]
            }
            j, err := json.Marshal(osEve)
            if err == nil {
                jOSEve = string(j)
            } else {
                common.PrintSysError(err)
            }
        }
        pidInt, _ := strconv.Atoi(argvs[2].(string))
        port := common.GetAppInstanceMemberByPid(pidInt, "Port")
        if port == "" {
            return
        }
        portStr := strconv.Itoa(port.(int))
        extra := fmt.Sprintf(`{"metrics":"%s"`+
            `,"value":"%s"`+
            `,"ts":"%s"`+
            `,"app_log":%s`+
            `,"reason":"%s"`+
            `,"os_log":%s`+
            `,"tag_set":"%s"`+
            `,"pid":"%s"`+
            `,"podId":"%s"`+
            `,"port":"%s"`+
            `,"containerId":"%s"}`,
            argvs[5].(string), argvs[6].(string), nowFormat, appLog, reason,
            jOSEve, argvs[1].(string), argvs[2].(string), argvs[3].(string),
            portStr, argvs[4].(string))
        SubmitAlarm(GetMetricsEventsDesc(argvs[0].(int), "",
            "", "", "", "", argvs[6].(string), extra))
    }
    data[3] = (data[3].(float64) / 1000.0)
    chkProcessMetricsGeneral(Notify_Process_RT_Type, "responseTimeAvg",
        " server RT over 100ms(%.2fms)", 100, data, rtEventExportHandler)
}

// Mem Events:
// direct reclaim, data should be
//
//    [containerId, comm, pid, process pgfault,
//     pgscan_direct, direct_reclaim_delay]
//
// if process not in container, containerId should be ""
func osChkMemDirectReclaimEvents(data []interface{}) {
    commIdx := 1
    pidIdx := 2
    dataStartIndex := 3
    if len(data) >= (2 + dataStartIndex) {
        pgScanDirect := data[dataStartIndex+1].(float64)
        if pgScanDirect > 0 {
            events := "direct reclaim occurs"
            pgFault := data[dataStartIndex].(float64)
            var reclaimDelay float32 = -1.0
            if len(data) >= (3 + dataStartIndex) {
                reclaimDelay = float32(data[dataStartIndex+2].(float64))
            }
            if reclaimDelay >= 10 {
                events = fmt.Sprintf(
                    "%s and delay %.2fms", events, reclaimDelay)
            }

            containerId := data[0].(string)
            containerRegex := `[0-9a-f]{12}`
            reContainer := regexp.MustCompile(containerRegex)
            if pgFault > 0 || reContainer.MatchString(containerId) {
                comm := data[commIdx].(string)
                pid := data[pidIdx].(string)
                events = fmt.Sprintf(
                    "%s when %s(%s) alloc mem", events, comm, pid)
            } else if reclaimDelay < 10 {
                events = ""
            }

            if len(events) > 0 {
                SubmitAlarm(GetLogEventsDesc(Notify_Direct_Reclaim_Type,
                    "warning", "os", events))
            }
        }
    }
}

// (mysqld)OOM killer, data should be dmesg errlog about oom
func osChkMemProcessOOMEvents(data []interface{}) {
    if len(data) > 1 {
        if strings.Contains(data[1].(string), "mysqld") {
            extraContainerId := "NULL"
            extraPodId := "NULL"
            process := "mysqld"
            pid := "-1"
            nowFormat := time.Unix(
                time.Now().Unix(), 0).Format(common.TIME_FORMAT)
            re := regexp.MustCompile(`Killed process (\d+) \(([^)]+)\)`)
            result := re.FindStringSubmatch(data[1].(string))
            if len(result) > 0 {
                pidInt, _ := strconv.Atoi(result[1])
                extraPodId = common.GetAppInstanceMemberByPid(
                    pidInt, "PodId").(string)
                if len(extraPodId) == 0 {
                    extraPodId = "NULL"
                }
                extraContainerId = common.GetAppInstanceMemberByPid(
                    pidInt, "ContainerId").(string)
                if len(extraContainerId) == 0 {
                    extraPodId = "NULL"
                }
                pid = result[1]
                process = result[2]
            }
            pidInt, _ := strconv.Atoi(pid)
            port := common.GetAppInstanceMemberByPid(pidInt, "Port")
            if port == "" {
                return
            }
            portStr := strconv.Itoa(port.(int))
            extra := fmt.Sprintf(`{"level":"fatal"`+
            `,"value":"mysqld exited by OOM killer"`+
            `,"details":"%s"`+
            `,"ts":"%s"`+
            `,"tag_set":"%s"`+
            `,"pid":"%s"`+
            `,"podId":"%s"`+
            `,"port":"%s"`+
            `,"containerId":"%s"}`,
            data[1].(string), nowFormat, process, pid,
            extraPodId, portStr, extraContainerId)
            SubmitAlarm(GetLogEventsDesc(Notify_Process_OOM_Type,
                "", "", "mysqld exited by OOM killer", extra))
        }
    }
}

// memleak check, data should be[unreclaim, ]
func osChkMemleakEvents(data []interface{}) {
    fmt.Println("Check mem events")
}

// Sched Events:
// sched delay, data should be [containerId, comm, pid, process sched delay]
// if process not in container, containerId should be ""
func osChkSchedProcessDelayEvents(data []interface{}) {
    chkProcessMetricsGeneral(Notify_Process_Sched_Delay_Type, "schedDelay",
        "sched delay over 100ms(%.2fms)", 50, data)
}

// (mysqld)long time D-status check, data should be
// [containerId, comm, pid, D-status time]
// if process not in container, containerId should be ""
func osChkSchedProcessDStatusEvents(data []interface{}) {
    chkProcessMetricsGeneral(Notify_Long_Time_D_Type, "UNStatusTime",
        "Time of UN-status over 300ms(%.2fms)", 300, data)
}

// Assess os lockup(soft/hard) risks, data should be
// [kernel nosched, irq off]
func osChkSchedOSLockupEvents(data []interface{}) {
    fmt.Println("check schedule events")
}

// process CPU check, data should be[containerId, comm, pid, cpu_total, cpu_user, cpu_sys]
// if process not in container, containerId should be ""
func osChkCpuHighEvents(data []interface{}) {
    if data[3].(float64) > 60 {
        desc := " CPU High Total over 60%%(%.2f%%) "
        details := fmt.Sprintf("due to sys(%.2f%%) user(%.2f%% is high)",
            data[5].(float64), data[4].(float64))
        if data[5].(float64) > 15 {
            details = fmt.Sprintf("due to sys(%.2f%% is high) user(%.2f%%)",
                data[5].(float64), data[4].(float64))
        }
        desc += strings.Replace(details, "%", "%%", -1)
        chkProcessMetricsGeneral(Notify_Process_CPU_HIGH_Type, "cpuTotal",
            desc, 60, data)
    } else if data[5].(float64) > 15 {
        chkProcessMetricsGeneral(Notify_Process_CPU_HIGH_Type, "cpuSys",
            " CPU High SYS over 15%%(%.2f%%)", 15, data)
    }
}

func OsChkStart() {
    fmt.Println("start OS events check")
    initSlideWindow("")
    //register some check handler for IO events
    RegisterNotify(Notify_IO_Wait_Type, osChkIOWaitEvents)
    RegisterNotify(Notify_IO_Except_Type, osChkIOExceptEvents)
    // RegisterNotify(Notify_IO_Burst_Type, osChkIOBurstEvents)
    // RegisterNotify(Notify_IO_Delay_Type, osChkIODelayEvents)
    // RegisterNotify(Notify_IO_Hang_Type, osChkIOHangEvents)
    RegisterNotify(Notify_IO_Error_Type, osChkIOErrEvents)
    RegisterNotify(Notify_FS_Error_Type, osChkFilesystemErrEvents)

    //register some check handler for net events
    RegisterNotify(Notify_Process_Net_Drops_Type, osChkNetDropEvents)
    RegisterNotify(Notify_Process_RT_Type, osChkNetProcessRTEvents)
    RegisterNotify(Notify_Net_Link_Down_Type, osChkNetLinkDownEvents)

    //register some check handler for mem events
    RegisterNotify(Notify_Direct_Reclaim_Type, osChkMemDirectReclaimEvents)
    RegisterNotify(Notify_Process_OOM_Type, osChkMemProcessOOMEvents)
    RegisterNotify(Notify_Memleak_Type, osChkMemleakEvents)

    //register some check handler for sched events
    RegisterNotify(Notify_Process_Sched_Delay_Type, osChkSchedProcessDelayEvents)
    RegisterNotify(Notify_OS_Lockup_Type, osChkSchedOSLockupEvents)
    RegisterNotify(Notify_Long_Time_D_Type, osChkSchedProcessDStatusEvents)
    RegisterNotify(Notify_Process_CPU_HIGH_Type, osChkCpuHighEvents)
}

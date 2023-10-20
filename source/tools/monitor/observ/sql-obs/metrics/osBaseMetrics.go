package metrics

import (
    //"os"
    // "fmt"
    "sql-obs/common"
    "sql-obs/events/analyzer"
    "strconv"
    "time"
    "strings"
    //"math"
)

type ErrorCode = common.ErrorCode
type MetricsType = analyzer.MetricsType

type MetricsFuncFloat32 func(table string, metrics string) (float32, error)
type MultiMetricsFuncFloat32 func(table string, metrics []string) []float32
type MetricsFuncFloat64 func(table string, metrics string) (float64, error)
type MultiMetricsFuncFloat64 func(table string, metrics []string) []float64

var GetMetricsFloat32 MetricsFuncFloat32 = common.GetSingleMetrics[float32]
var GetMultiMetricsFloat32 MultiMetricsFuncFloat32 = common.GetMultiMetrics[float32]
var GetMetricsFloat64 MetricsFuncFloat64 = common.GetSingleMetrics[float64]
var GetMultiMetricsFloat64 MultiMetricsFuncFloat64 = common.GetMultiMetrics[float64]

var (
    osGlobalMetricsTlbName string = "sysom_obser_metrics_mysqld_os"
    osMysqldMetricsTlbName string = "sysom_obser_metrics_mysqld_process"
)

type appOsMetrics struct {
    CpuTotal               float64 `json:"cpuTotal"`
    CpuUser                float64 `json:"cpuUser"`
    CpuSys                 float64 `json:"cpuSys"`
    CpuGiveup              float64 `json:"cpuGiveup"`
    SchedDelay             float64 `json:"schedDelay"`
    UNStatusTime           float64 `json:"UNStatusTime"`
    IoWriteBps             float64 `json:"ioWriteBps"`
    IoReadBps              float64 `json:"ioReadBps"`
    Iowait                 float64 `json:"iowait"`
    IoDelay                float64 `json:"ioDelay"`
    CgroupDirtyBlockThresh float64 `json:"cgroupDirtyBlockThresh"`
    CgroupDirtyPages       float64 `json:"cgroupDirtyPages"`
    CgroupFlushPages       float64 `json:"cgroupFlushPages"`
    // MemUsedLayout          float64 `json:"memUsedLayout"`
    CgroupMemUsedAnon       float64 `json:"cgroupMemUsedAnon"`
    CgroupMemUsedCache      float64 `json:"cgroupMemUsedCache"`
    CgroupMemUsedSh         float64 `json:"cgroupMemUsedSh"`
    MemReclaimLatency       float64 `json:"memReclaimLatency"`
    PkgDrops                float64 `json:"pkgDrops"`
    RequestCount            float64  `json:"requestCount"`
    NetSendTraffic          float64  `json:"netSendTraffic"`
    NetRecTraffic           float64  `json:"netRecTraffic"`
    ResponseTimeMax         uint64  `json:"responseTimeMax"`
    ResponseTimeAvg         uint64  `json:"responseTimeAvg"`
}

type globalOsMetrics struct {
    // cpuPercent float32
    //loadThresh float64 //0.7, 1.0, 5.0 * cpu core
    LoadD      float64 `json:"loadD"`
    LoadR      float64 `json:"loadR"`
    SoftLockup float64 `json:"softLockup"`
    HardLockup float64 `json:"hardLockup"`
    //memUsedLayout float64 //use memgraph
    //memReclaimLayout float64
    MemFrag          float64 `json:"memFrag"`
    NetAccepetCnt    float64 `json:"netAccepetCnt"`
    NetAccepetThresh float64 `json:"netAccepetThresh"`
    NetSYNCnt        float64 `json:"netSYNCnt"`
    NetSYNCntThresh  float64 `json:"netSYNCntThresh"`
    ResponseTime     float64 `json:"responseTime"`
}

var labelsRT = []string{
    "Requests", "InBytes", "OutBytes", "AvgRT", "MaxRT"}
var metricsName = []string{
        "cpu_total", "cpu_user", "cpu_sys", "nr_voluntary_switches",
        "nr_involuntary_switches", "delay", "write_bytes", "read_bytes", "IOwait",
        "majflt",
    }
    
func updateAppMetrics(mList *[]*appOsMetrics) ([][]string, error) {
    info := make([][]string, 0)
    set := make(map[string]struct{})
    labelName := []string{
        "cgroup", "pid",
    }
    rtMap := common.GetAppLatency("sysom_metrics_ntopo_node", labelsRT)
    for index, line := range common.GetAppMetrics(
        "observe", "mysqld", labelName, metricsName) {
        metric := line.(map[string]interface{})
        pid, _ := strconv.Atoi(metric["pid"].(string))
        containerId := "NULL"
        if val, ok := metric["cgroup"]; ok && (val != nil) {
            containerId = metric["cgroup"].(string)
            if len(containerId) < 1 {
                containerId = "NULL"
            }
        }
        app := []string{
            metric["pid"].(string),
            common.GetAppInstanceMemberByPid(pid, "PodId").(string),
            containerId,
        }
        _, ok := set[strings.Join(app, ",")]
        if !ok {
            info = append(info, app)
            set[strings.Join(app, ",")] = struct{}{}
        } else {
            /* repeated data for same app instance */
            continue
        }
        var m appOsMetrics
        m.CpuTotal, _ = metric["cpu_total"].(float64)
        m.CpuUser, _ = metric["cpu_user"].(float64)
        m.CpuSys, _ = metric["cpu_sys"].(float64)

        vswitches, _ := metric["nr_voluntary_switches"].(float64)
        invswitches, _ := metric["nr_involuntary_switches"].(float64)
        if (vswitches + invswitches) > 0 {
            m.CpuGiveup = vswitches / (vswitches + invswitches)
        }
        m.SchedDelay, _ = metric["delay"].(float64)
        m.IoWriteBps, _ = metric["write_bytes"].(float64)
        m.IoReadBps, _ = metric["read_bytes"].(float64)
        m.Iowait, _ = metric["IOwait"].(float64)
        // m.iodelay, _ = metric("obIO", "iowait") // null
        // m.UNStatusTime, _ := metric["UNStatusTime"].(float64)
        memInfo := getCgroupInfo(pid, containerId)
        m.MemReclaimLatency = float64(memInfo.DRMemLatencyPS)
        m.CgroupMemUsedAnon = float64(memInfo.AnonMem)
        m.CgroupMemUsedCache = float64(memInfo.CacheMem)
        m.CgroupMemUsedSh = float64(memInfo.ShMem)
        m.CgroupDirtyPages = float64(memInfo.Dirty)
        m.CgroupDirtyBlockThresh = 
             (float64(memInfo.MemLimit) - float64(memInfo.MemUsage)) * 0.4
        // dirty_ratio, _ := metric["mem_available"] // null
        // m.CgroupFlushPages, _ = metric["cg_wb"]    // null
        // m.MemUsedLayout, _ = metric["mem_used"] // null
        // m.PkgDrops, _ := metric["drops"]         // null
        // retrans, _ = metric["retran"]           // null
        if rtMap != nil {
            if _, ok := rtMap[containerId]; ok {
                num, _ := strconv.ParseFloat(rtMap[containerId]["Requests"], 64)
                m.RequestCount = num / 30.0
                num, _ = strconv.ParseFloat(rtMap[containerId]["InBytes"], 64)
                m.NetRecTraffic = num / 30.0
                num, _ = strconv.ParseFloat(rtMap[containerId]["OutBytes"], 64)
                m.NetSendTraffic = num / 30.0
                m.ResponseTimeAvg, _ = 
                    strconv.ParseUint(rtMap[containerId]["AvgRT"], 10, 64)
                m.ResponseTimeMax, _ = 
                    strconv.ParseUint(rtMap[containerId]["MaxRT"], 10, 64)
            }
        }
        if m.CpuTotal > 0 {
            analyzer.MarkEventsNotify(
                MetricsType(analyzer.Notify_Process_CPU_HIGH_Type),
                info[index][2], "mysqld", info[index][0],
                m.CpuTotal, m.CpuUser, m.CpuSys)
        }
        if m.SchedDelay > 0 {
            analyzer.MarkEventsNotify(
                MetricsType(analyzer.Notify_Process_Sched_Delay_Type),
                info[index][2], "mysqld", info[index][0], m.SchedDelay/1000.0)
        }
        if m.ResponseTimeAvg > 0 {
            analyzer.MarkEventsNotify(
                MetricsType(analyzer.Notify_Process_RT_Type),
                info[index][2], "mysqld", info[index][0],
                float64(m.ResponseTimeAvg))
        }
        if m.UNStatusTime > 0 {
            analyzer.MarkEventsNotify(MetricsType(analyzer.Notify_Long_Time_D_Type),
                info[index][2], "mysqld", info[index][0], m.UNStatusTime)
        }
        pgmajfault, _ := metric["majflt"].(float64)
        if m.MemReclaimLatency > 0 {
            analyzer.MarkEventsNotify(
                MetricsType(analyzer.Notify_Direct_Reclaim_Type),
                info[index][2], "mysqld", info[index][0],
                pgmajfault, m.MemReclaimLatency, m.MemReclaimLatency)
        }
        if m.PkgDrops > 0 {
            analyzer.MarkEventsNotify(
                MetricsType(analyzer.Notify_Process_Net_Drops_Type),
                info[index][2], "mysqld", info[index][0], m.PkgDrops)
        }
        analyzer.TriggerNotify()
        *mList = append(*mList, &m)
    }
    return info, nil
}

func updateOsMetrics(m *globalOsMetrics) error {
    // loadinfo := GetMultiMetricsFloat64("sysak_proc_loadavg",
    //     []string{"loadD", "loadR"})
    // m.LoadD = loadinfo[0]
    // m.LoadR = loadinfo[1]

    // schedinfo := GetMultiMetricsFloat64("sysak_proc_sched",
    //     []string{"nosched", "irq_off"})
    // m.SoftLockup = schedinfo[0]
    // m.HardLockup = schedinfo[1]
    // if m.SoftLockup > 0 || m.HardLockup > 0 {
    //     analyzer.MarkEventsNotify(
    //         MetricsType(analyzer.Notify_OS_Lockup_Type),
    //         m.SoftLockup,
    //         m.HardLockup)
    // }

    // // [total_mem, alloc_page, SUnreclaim]
    // meminfo := GetMultiMetricsFloat64("meminfo",
    //     []string{"total", "alloc_page", "SUnreclaim"})
    // if meminfo != nil {
    //     analyzer.MarkEventsNotify(
    //         MetricsType(analyzer.Notify_Memleak_Type),
    //         meminfo[0], meminfo[1], meminfo[2])
    // }
    // m.MemFrag, _ = GetMetricsFloat64("sysak_proc_mem", "memFrag")

    // netinfo := GetMultiMetricsFloat64("sysak_proc_net",
    //     []string{"acc_cnt", "acc_thresh", "syn_cnt", "syn_thresh"})
    // m.NetAccepetCnt = netinfo[0]
    // m.NetAccepetThresh = netinfo[1]
    // m.NetSYNCnt = netinfo[2]
    // m.NetSYNCntThresh = netinfo[3]

    iowait, err := GetMetricsFloat64("cpu_total", "iowait")
    if err == nil {
        analyzer.MarkEventsNotify(
            MetricsType(analyzer.Notify_IO_Wait_Type), iowait)
    }

    ioData := common.GetIOMetrics("disks", []string{
        "busy", "reads", "writes", "rmsec", "wmsec", "rkb", "wkb", "backlog"})
    if ioData != nil {
        analyzer.MarkEventsNotify(
            MetricsType(analyzer.Notify_IO_Except_Type), ioData)
    }

    analyzer.TriggerNotify()
    return nil
}

func exportAppMetrics(appMetrics []*appOsMetrics, info [][]string) {
    data := ""
    for index, m := range appMetrics {
        if len(data) > 0 {
            data += "\n"
        }
        data += (osMysqldMetricsTlbName + `,pid=` + info[index][0] +
            `,podID=` + info[index][1] + `,containerID=` + info[index][2] +
            `,comm=mysqld ` + common.Struct2String(m))
    }
    if len(data) > 0 {
        common.ExportData(data)
    }
}

func exportOSMetrics(m *globalOsMetrics) {
    common.ExportData(osGlobalMetricsTlbName +
        `,comm=mysqld ` + common.Struct2String(m))
}

func exportAlarmStatics() {
    analyzer.ExportAlarmStatics()
}

// Monitoring resource availability
func StartOsBaseMonitor() {
    var osMetrics globalOsMetrics
    for {
        duration := 30 * time.Second
        startTime := time.Now()
        var appMetrics []*appOsMetrics
        info, _ := updateAppMetrics(&appMetrics)
        updateOsMetrics(&osMetrics)
        exportAppMetrics(appMetrics, info)
        exportOSMetrics(&osMetrics)
        exportAlarmStatics()
        costTime := time.Now().Sub(startTime)
        if costTime < duration {
            duration -= costTime
        }
        time.Sleep(duration)
    }
}

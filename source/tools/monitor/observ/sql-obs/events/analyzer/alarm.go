package analyzer

import (
    "encoding/json"
    "fmt"
    "time"
    // "math"
    "sql-obs/common"
    // "os"
    "strconv"
    // "regexp"
    // "unsafe"
    "reflect"
    "strings"
    "sync"
)

type eventsDesc struct {
    ts    int64
    desc  string
    extra string //some other desc...json format
    alarm bool
    descLock sync.Mutex
}

type alarmManage struct {
    agingTime      int
    desc           []eventsDesc
    cacheDescExtra []map[string]string
}

type alarmStatics struct {
    alarmStr string
    count    int
}

const Default_Aging_Time = 120 //120 seconds
var appAlarmEventsStaticsTlbName = "sysom_obser_mysqld_alarm"
var osAlarmEventsStaticsTlbName = "sysom_obser_os_alarm"
var alarmManageTlb = make([]*alarmManage, Notify_Type_Max)
var alarmStatMap = make(map[string][]*alarmStatics)

type diagnoseApiDsec struct {
    defaultApiName string
    titleDesc string
    allowAnotherDiag bool
}

// According to the Notify_Type table order correspondence
// The order must be consistent with the Notify_Type table in notify.go
var generalDiagnoseApiTable = []diagnoseApiDsec {
    {"diagnose/storage/iofsstat?disk=$disk$&", "IO burst", false},   //Notify_IO_Burst_Type is the first
    {"diagnose/storage/iolatency?disk=$disk$&threshold=$threshold$&", "IO delay", false},
    {"diagnose/storage/iowait?", "IO wait", false},
    {"null", "null", false},
    {"diagnose/cpu/loadtask?", "UN-status", false},
    {"diagnose/memory/memgraph?", "direct reclaim", false},
    {"diagnose/memory/memgraph?", "memleak", false},
    {"journal/node?", "null", false},
    {"journal/node?", "null", false},
    {"journal/node?", "null", false},
    {"diagnose/memory/oomcheck?time=$ts$&", "null", false},
    {"diagnose/storage/iohang?disk=$disk$&threshold=3000&", "IO hang", false},
    {"diagnose/cpu/cpuhigh?time=$ts$&", "lockup", false},
    {"diagnose/link/rtdelay?pid=$pid$&", "RT", true},
    {"diagnose/cpu/schedmoni?threshold=100&", "sched delay", false},
    {"diagnose/net/packetdrop?", "pkg drops", false},
    {"journal/node?", "slow SQL", true},
    {"journal/node?", "null", false},
    {"diagnose/cpu/cpuhigh?time=$ts$&", "CPU High", false}, //Notify_Process_CPU_HIGH_Type is the last
}

func getApiNameFromDesc(desc string) (string, int) {
    apiName := "null"
    tIdx := Notify_Type_Max
    for typeIdx, diag := range generalDiagnoseApiTable {
        if strings.Contains(desc, diag.titleDesc) {
            apiName = diag.defaultApiName
            tIdx = typeIdx
            break
        }
    }
    return apiName, tIdx
}

func getDiagnoseApiName(alarmType int, desc string) (name string) {
    if (alarmType < len(generalDiagnoseApiTable)) {
        data := fromStringToMapInterface(desc)
        apiName := generalDiagnoseApiTable[alarmType].defaultApiName
        if (generalDiagnoseApiTable[alarmType].allowAnotherDiag) {
            description := data["value"].(string)
            if _, exist := data["os_log"]; exist {
                _, ok := data["os_log"].(map[string]interface{})["value"]
                if ok {
                    description = data["os_log"].(
                        map[string]interface{})["value"].(string)
                } else {
                    description = "default diagnose"
                }
            }
            newApiName, aType := getApiNameFromDesc(description)
            if newApiName != "null" && aType != alarmType {
                apiName = newApiName
            }
        }
        if strings.Count(apiName, "$") > 0 {
            d := data
            if _, exist := data["os_log"]; exist {
                _, ok := data["os_log"].(map[string]interface{})["value"]
                if ok {
                    d = data["os_log"].(map[string]interface{})
                }
            }
            for key, value := range d {
                apiName = strings.ReplaceAll(apiName,
                    fmt.Sprintf("$%s$", key), fmt.Sprintf("%v", value))
            }
        }
        return apiName
    } else {
        return "null"
    }
}

func addAlarmStat(alarmType int, extraVal map[string]string, add int) {
    getAlarmStat := func(alarmType int, extraVal map[string]string) *alarmStatics {
        key := "os"
        if _, ok := extraVal["containerId"]; ok {
            key = extraVal["podId"] + ":" + extraVal["containerId"] + ":" +
                extraVal["tag_set"] + ":" + extraVal["port"]
        }
        if _, ok := alarmStatMap[key]; !ok {
            alarmStatMap[key] = make([]*alarmStatics, Notify_Type_Max)
        }
        if alarmStatMap[key][alarmType] == nil {
            alarmStatMap[key][alarmType] = &alarmStatics{
                alarmStr: strings.ReplaceAll(
                    mTypeStrTlb[alarmType], "Notify", "Alarm"),
                count: 0,
            }
        }
        return alarmStatMap[key][alarmType]
    }
    getAlarmStat(alarmType, extraVal).count += (add)
}

func alarmEventAging(alarmType int, descIdx int) (bool, bool) {
    alarm := alarmManageTlb[alarmType]
    desc := &alarm.desc[descIdx]
    agingTime := alarm.agingTime
    hasAlarm := false
    now := time.Now().Unix()
    desc.descLock.Lock()
    if desc.alarm {
        hasAlarm = true
        start := desc.ts
        if int(now-start) >= agingTime {
            desc.alarm = false
            desc.desc = ""
            desc.extra = ""
            addAlarmStat(alarmType, alarm.cacheDescExtra[descIdx], (-1))
            alarm.cacheDescExtra[descIdx] = map[string]string{}
            desc.descLock.Unlock()
            return true, hasAlarm
        }
    }
    desc.descLock.Unlock()
    return false, hasAlarm
}

func getAlarmEventIdx(alarmType int) int {
    lenDesc := len(alarmManageTlb[alarmType].desc)
    descIdx := lenDesc
    for idx := 0; idx < lenDesc; idx++ {
        aging, alarm := alarmEventAging(alarmType, idx)
        if aging || !alarm {
            if descIdx >= lenDesc {
                descIdx = idx
            }
        }
    }
    return descIdx
}

func fromStringToMapInterface(extra string) map[string]interface{} {
    var extraVal map[string]interface{}
    err := json.Unmarshal([]byte(extra), &extraVal)
    if err != nil {
        return nil
    }
    return extraVal
}

func fromStringToMapString(extra string) map[string]string {
    var extraValString, extraVal map[string]string
    var extraValInterface map[string]interface{}
    err := json.Unmarshal([]byte(extra), &extraValString)
    if err != nil {
        err = json.Unmarshal([]byte(extra), &extraValInterface)
        if err != nil {
            return nil
        }
        extraVal = make(map[string]string)
        for k, v := range extraValInterface {
            if reflect.TypeOf(v).Kind() != reflect.String {
                v = fmt.Sprintf("%v", v)
            }
            extraVal[k] = v.(string)
        }
    } else {
        extraVal = extraValString
    }
    return extraVal
}

func alarmEvent(alarmType int, ts int64, desc string, extra string) {
    extraVal := fromStringToMapString(extra)
    if extraVal == nil {
        return
    }
    addAlarmStat(alarmType, extraVal, 1)
    descIdx := getAlarmEventIdx(alarmType)
    if descIdx >= len(alarmManageTlb[alarmType].desc) {
        alarmManageTlb[alarmType].desc = append(
            alarmManageTlb[alarmType].desc, eventsDesc{})
        alarmManageTlb[alarmType].cacheDescExtra = append(
            alarmManageTlb[alarmType].cacheDescExtra,
            map[string]string{})
    }
    eDsec := &alarmManageTlb[alarmType].desc[descIdx]
    eDsec.alarm = true
    eDsec.ts = ts
    eDsec.desc = desc
    eDsec.extra = extra
    alarmManageTlb[alarmType].cacheDescExtra[descIdx] = extraVal
}

func GetAlarmDescs(alarmType int) []map[string]string {
    now := time.Now().Unix()
    lenDesc := len(alarmManageTlb[alarmType].desc)
    for idx := 0; idx < lenDesc; idx++ {
        alarmInvalid := false
        start := alarmManageTlb[alarmType].desc[idx].ts
        if int(now-start) > Default_Aging_Time {
            alarmInvalid = true
        }
        if alarmManageTlb[alarmType].agingTime > 0 || alarmInvalid {
            alarmEventAging(alarmType, idx)
        }
    }
    isAllElementsEmpty := func(s []map[string]string) bool {
        for _, v := range s {
            if len(v) != 0 {
                return false
            }
        }
        return true
    }
    if isAllElementsEmpty(alarmManageTlb[alarmType].cacheDescExtra) {
        return nil
    }
    return alarmManageTlb[alarmType].cacheDescExtra
}

func addFieldToExtra(extra string, field string) string {
    str := strings.TrimSuffix(extra, "}")
    str += ("," + field + "}")
    return str
}

func makeAlarmBody(alarmType int, desc string, descExtra string) string {
    ts := time.Now().Unix()

    alarmEvent(alarmType, ts, desc, descExtra)
    // return fmt.Sprintf(`node_event event_type="log_exception",`+
    //     `description="%s",extra=%s,ts="%s"`,
    //     desc, strconv.Quote(addFieldToExtra(descExtra,
    //         "\"root_analyz_flag\":\"" + getDiagnoseApiName(alarmType,
    //         descExtra) + "\"")), now)
    alarmItem := strings.ReplaceAll(mTypeStrTlb[alarmType], "Type", "Alarm")
    alarmItem = strings.ReplaceAll(alarmItem, "Notify", "Sqlobs")
    return fmt.Sprintf(
        `{"alert_item":"%s",`+
        `"alert_category":"APPLICATION",`+
        `"status":"FIRING",`+
        `"alert_source_type":"sysak",`+
        `"labels":%s}`,
        alarmItem,
        strconv.Quote(addFieldToExtra(descExtra,
            "\"root_analyz_flag\":\"" + getDiagnoseApiName(alarmType,
            descExtra) + "\"")))
}

func GetLogEventsDesc(alarmType int, level string, tag_set string,
    desc string, extra ...string) string {
    extraVal := desc
    ts := time.Now().Unix()
    nowFormat := time.Unix(ts, 0).Format(common.TIME_FORMAT)
    if len(extra) > 0 {
        extraVal = extra[0]
    }
    descExtra := extraVal
    if !json.Valid([]byte(descExtra)) {
        descExtra = fmt.Sprintf(`{"level":"%s"`+
            `,"value":"%s"`+
            `,"ts":"%s"`+
            `,"tag_set":"%s"}`,
            level, extraVal, nowFormat, tag_set)
    }
    return makeAlarmBody(alarmType, desc, descExtra)
}

func GetMetricsEventsDesc(alarmType int, comm string, pid string,
    extraPodId string, extraContainerId string, metricsName string,
    desc string, extra ...string) string {
    extraVal := desc
    ts := time.Now().Unix()
    nowFormat := time.Unix(ts, 0).Format(common.TIME_FORMAT)
    if len(extra) > 0 {
        extraVal = extra[0]
    }
    descExtra := extraVal
    if !json.Valid([]byte(descExtra)) {
        pidInt, _ := strconv.Atoi(pid)
        portStr := strconv.Itoa(
            common.GetAppInstanceMemberByPid(pidInt, "Port").(int))
        descExtra = fmt.Sprintf(`{"metrics":"%s"`+
            `,"value":"%s"`+
            `,"ts":"%s"`+
            `,"tag_set":"%s"`+
            `,"pid":"%s"`+
            `,"port":"%s"`+
            `,"podId":"%s"`+
            `,"containerId":"%s"}`,
            metricsName, extraVal, nowFormat,
            comm, pid, portStr, extraPodId, extraContainerId)
    }
    return makeAlarmBody(alarmType, desc, descExtra)
}

func SubmitAlarm(data string) error {
    //fmt.Println(data)
    _, err := common.PostReqToUnity("/api/alert", data)
    if err != nil {
        //fmt.Println(bodyBytes)
        common.PrintSysError(err)
        return err
    }
    //fmt.Println(string(bodyBytes))
    return nil
}

func ExportAlarmStatics() {
    data := ""
    for key, value := range alarmStatMap {
        prefix := osAlarmEventsStaticsTlbName + ",tag_set=os"
        if key != "os" {
            prefix = appAlarmEventsStaticsTlbName + ",tag_set=app"
            s := strings.Split(key, ":")
            podIp := common.GetAppInstanceInfo(map[string]interface{}{
                "Comm": s[2], "PodId": s[0], "ContainerId": s[1]}, "Ip").(string)
            prefix += ",podId=" + s[0] + ",containerId=" + s[1] +
                ",comm=" + s[2] + ",podIp=" + podIp + ",port=" + s[3]
        }
        fields := ""
        for t, stat := range value {
            field := ""
            if stat == nil {
                f := strings.ReplaceAll(mTypeStrTlb[t], "Notify", "Alarm")
                field = fmt.Sprintf("%s=0,", f)
            } else {
                field = fmt.Sprintf("%s=%d,", stat.alarmStr, stat.count)
            }
            fields += field
        }
        if len(fields) > 0 {
            if len(data) > 0 {
                data += "\n"
            }
            fields = fields[:len(fields)-1]
            data += (prefix + " " + fields)
        }
    }
    if len(data) > 0 {
        common.ExportData(data)
    }
}

func SetAlarmAgingTime(eventType int, time int) {
    if eventType < Notify_Type_Max {
        alarmManageTlb[eventType].agingTime = time
    }
}

func initAppAlarm(values []interface{}) {
    podId := values[0].(string)
    containerId := values[1].(string)
    comm := values[2].(string)
    port := strconv.Itoa(values[3].(int))
    for i := 0; i < Notify_Type_Max; i++ {
        addAlarmStat(i, map[string]string{
            "podId" : podId, "containerId": containerId,
            "tag_set": comm, "port": port}, 0)
    }
}

var agingAlarmTicker *time.Ticker
func agingAlarmTimer() {
    ticker := time.NewTicker(time.Second * Default_Aging_Time)
    agingAlarmTicker = ticker
    for {
        <-ticker.C
        now := time.Now().Unix()
        for i := 0; i < Notify_Type_Max; i++ {
            lenDesc := len(alarmManageTlb[i].desc)
            for idx := 0; idx < lenDesc; idx++ {
                alarmInvalid := false
                start := alarmManageTlb[i].desc[idx].ts
                if int(now-start) > Default_Aging_Time {
                    alarmInvalid = true
                }
                if alarmManageTlb[i].agingTime > 0 || alarmInvalid {
                    alarmEventAging(i, idx)
                }
            }
        }
    }
}

func DestroyAlarmResource() {
    if agingAlarmTicker != nil {
        agingAlarmTicker.Stop()
    }
}

func InitAlarmManage() {
    for i := 0; i < Notify_Type_Max; i++ {
        alarmManageTlb[i] = &alarmManage{
            agingTime:      Default_Aging_Time,
            desc:           make([]eventsDesc, 32),
            cacheDescExtra: make([]map[string]string, 32),
        }
        addAlarmStat(i, map[string]string{"tag_set" : "os"}, 0)
    }

    common.ForeachAppInstances("mysqld", []string{
        "PodId", "ContainerId", "Comm", "Port"}, initAppAlarm)
    go agingAlarmTimer()
    SetAlarmAgingTime(Notify_IO_Wait_Type, 0)
    SetAlarmAgingTime(Notify_Direct_Reclaim_Type, 0)
    SetAlarmAgingTime(Notify_Memleak_Type, 0)
    SetAlarmAgingTime(Notify_IO_Error_Type, 0)
    SetAlarmAgingTime(Notify_FS_Error_Type, 0)
    SetAlarmAgingTime(Notify_Net_Link_Down_Type, 0)
    SetAlarmAgingTime(Notify_OS_Lockup_Type, 0)
}

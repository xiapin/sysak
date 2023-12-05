package analyzer

import (
    "encoding/json"
    "fmt"
    "sql-obs/common"
    "unsafe"
    //"os"
    "strconv"
    "strings"
    "time"
)

type ssAnalyzer struct {
    A         *Analyzer
    dbConn    *DBConnect
    mSlowData []map[string]interface{}
}

var mysqlEventsTable string = "mysql_observ_appEvents"
var osChkStrFlag string = "OS events also should be chk"

func convertLogToSlowData(log []string, logLen int,
    mSlowData *[]map[string]interface{}, budget int) (int, int) {
    retInfoCnt := 0
    leftLen := logLen
    /** slow log:
     * # Time: 2023-05-23T03:13:35.902947Z
     * # User@Host: junran[junran] @  [140.205.118.245]  Id:   618
     * # Query_time: 2.680824  Lock_time: 0.000003 Rows_sent: 5322  Rows_examined: 5314419
     * SET timestamp=1684811613;
     * SELECT * FROM orders WHERE user_id=752;
     */
    for ; leftLen > 0; leftLen-- {
        i := logLen - leftLen
        //get slow query parameters
        if !strings.HasSuffix(log[i], ";") {
            validDataStartPos := 0
            if strings.HasPrefix(log[i], "# ") {
                validDataStartPos = 2
            }
            s := strings.Split(log[i][validDataStartPos:], ": ")
            if len(s)%2 != 0 {
                var sTmp []string
                for _, str := range s {
                    index := strings.LastIndex(str, " ")
                    if index >= 0 {
                        //non-standard like this: "junran[junran] @  [140.205.118.245]  Id"
                        nonStandard := []string{str[:index], str[index+1:]}
                        for _, str1 := range nonStandard {
                            str1 := strings.ReplaceAll(str1, " ", "")
                            if len(str1) != 0 {
                                sTmp = append(sTmp, str1)
                            }
                        }
                    } else {
                        sTmp = append(sTmp, str)
                    }
                }
                if len(sTmp)%2 != 0 {
                    common.PrintDefinedErr(
                        ErrorCode(common.Fail_Unrecognized_Slow_Log_Format),
                        "log info: "+log[i])
                    return retInfoCnt, -1
                }
                s = sTmp
            }
            for n, str := range s {
                if n%2 == 0 {
                    (*mSlowData)[retInfoCnt][str] = s[n+1]
                }
            }
        } else {
            //get sql statement and start_timestamp
            if strings.HasPrefix(log[i], "SET timestamp=") {
                s := strings.Split(log[i], "timestamp=")
                (*mSlowData)[retInfoCnt]["timestamp"] =
                    strings.ReplaceAll(s[1], ";", "")
            } else {
                if _, ok := (*mSlowData)[retInfoCnt]["timestamp"]; ok {
                    (*mSlowData)[retInfoCnt]["statement"] = log[i]
                    retInfoCnt++
                    if retInfoCnt == budget {
                        break
                    }
                }
            }
            continue
        }
    }
    return retInfoCnt, leftLen
}

func (ssA *ssAnalyzer) getQueryExplain(sqlCmd string) []map[string]interface{} {
    cmd := "EXPLAIN " + sqlCmd
    if ssA.dbConn.DBConnIsVaild() {
        data, err := ssA.dbConn.GetRowsByQueryCmd(cmd)
        if err != nil {
            common.PrintDefinedErr(ErrorCode(common.Fail_Get_DB_Variables))
            return nil
        }
        return data
    } else {
        return nil
    }
}

func selectTypeAnalyze(sType string) string {
    // select_type: simple to complex:
    //              SIMPLE
    //              PRIMARY
    //              DERIVED
    //              SUBQUERY
    //              UNION
    //              DEPENDENT UNION
    //              DEPENDENT SUBQUERY
    //              UNION RESULT (UNQ.E)
    //              DEPENDENT UNION RESULT
    if sType == "SIMPLE" {
        return "A simple select query"
    } else if sType == "PRIMARY" {
        return "Include subqueries in query"
    } else if sType == "SUBQUERY" {
        return "Include subqueries in the SELECT or WHERE"
    } else if sType == "DERIVED" {
        return "A subquery included in the From"
    } else if sType == "UNION" {
        return "A union query"
    } else if sType == "UNION RESULT" {
        return "SELECT the result from the UNION table"
    } else if sType == "DEPENDENT SUBQUERY" {
        return "Dependent subquery in the SELECT or WHERE"
    }
    return ""
}

func extraInfoAnalyze(extra string) string {
    // Extra: good to bad:
    //        Using index
    //        Using where
    //        Using temporary
    //        Using filesort
    //        Using join buffer (Block Nested Loop)
    //        Range checked for each record
    //        Full scan on NULL key
    //        Impossible WHERE
    //        No matching min/max row
    if strings.Contains(extra, "Using index") {
        return "Using index, efficient query"
    } else if strings.Contains(extra, "Using where") {
        return "Where filter condition is not an index"
    } else if strings.Contains(extra, "Using temporary") {
        return "Using temporary tables"
    } else if strings.Contains(extra, "Using filesort") {
        return "Using non indexed fields for the 'order by'"
    }
    return ""
}

/**
 * TODO: independent thread processing or blocking processing is required?
 * analyzing the root cause
 */
func analyzeSlowLog(data map[string]interface{}, ssA *ssAnalyzer) string {
    _, ok := data["explain0"]
    rowsSend, _ := strconv.Atoi(data["Rows_sent"].(string))
    rowsExamined, _ := strconv.Atoi(data["Rows_examined"].(string))
    //can not get explain
    if rowsSend > 0 && (rowsSend*100 < rowsExamined) {
        if !ok {
            return fmt.Sprintf(
                "Index Invalidation, too many examined rows, examined(%s) sent(%s)",
                data["Rows_examined"].(string), data["Rows_sent"].(string))
        }
    } else {
        lockTime, _ := strconv.ParseFloat(data["Lock_time"].(string), 64)
        queryTime, _ := strconv.ParseFloat(data["Query_time"].(string), 64)
        if lockTime >= queryTime*0.2 {
            return fmt.Sprintf(
                "Wait lock too long, LockTime(%vs) exceeds 20%% of queryTime(%vs)",
                lockTime,
                queryTime)
        }
        if rowsSend == 0 && rowsExamined > 100000 && !ok {
            return fmt.Sprintf("%d examined rows maybe Optimize query statements(%s), %s",
                rowsExamined, data["statement"], osChkStrFlag)
        }
    }
    if !ok {
        ret := ssA.dbConn.ParseSQL(data["statement"].(string))
        if ret == "complex" {
            return fmt.Sprintf("Complex SQL statement, "+
                "please Optimize query statements(%s), %s",
                data["statement"], osChkStrFlag)
        }
        return "unkown, please chk explain for more information, " + osChkStrFlag
    }

    // possible_keys: indexes that can use to accelerate queries
    // key: The actual index used,
    //      if not in possible_keys, maybe poor index used
    // rows: scan rows
    var result []string
    for k, m := range data {
        if !strings.Contains(k, "explain") {
            continue
        }
        explain := m.(map[string]interface{})

        r := selectTypeAnalyze(explain["select_type"].(string))
        if r != "" {
            r += ", "
        }
        if explain["key"].(string) == "NULL" {
            //no use index
            r += "index not used"
        } else if explain["possible_keys"].(string) != "NULL" &&
            !strings.Contains(explain["possible_keys"].(string),
                explain["key"].(string)) {
            //Unexpected index used
            r += fmt.Sprintf(
                "unexpected index used, expected(%s) but (%s)",
                explain["possible_keys"].(string),
                explain["key"].(string))
        }
        //Extra info
        extraInfo := extraInfoAnalyze(explain["Extra"].(string))
        // type: good to bad:
        //       system
        //       const
        //       eq_ref
        //       ref
        //       range
        //       index
        //       all
        //full table scan
        if explain["type"].(string) == "ALL" {
            if explain["key"].(string) != "NULL" {
                if len(r) > 0 {
                    r += ", "
                }
                r += fmt.Sprintf(
                    "Index %s Invalidation", explain["key"].(string))
            }
            if len(r) > 0 {
                r += ", "
            }
            r += "full table scan"
            if rowsExamined < 100000 {
                r += fmt.Sprintf(", %d examined rows, %s",
                        rowsExamined, osChkStrFlag)
            }
        } else {
            indexStr := explain["key"].(string)
            indexOptimal := false
            optimalIdxReason := ""
            //the field after where is index，and in effect
            if strings.Contains(extraInfo, "Using index") {
                indexOptimal = true
            }
            if rowsSend > 0 && (rowsSend*100 < rowsExamined) {
                optimalIdxReason = fmt.Sprintf(
                    "Index %s Invalidation, too many examined rows," + 
                    " examined(%s) sent(%s)", indexStr,
                    data["Rows_examined"].(string),
                    data["Rows_sent"].(string))
            } else {
                if rowsSend == 0 {
                    optimalIdxReason = ", " + osChkStrFlag
                } else {
                    r += ", " + osChkStrFlag
                }
            }
            if !strings.Contains(r, osChkStrFlag) {
                if indexOptimal {
                    if len(optimalIdxReason) > 0 {
                        r += optimalIdxReason
                    }
                } else {
                    if len(extraInfo) > 0 {
                        r += (", " + extraInfo + ", ")
                    }
                    r += fmt.Sprintf("Not optimal index(%s) used, %s",
                        indexStr, osChkStrFlag)
                }
            }
        }
        result = append(result, r)
    }
    return strings.Join(result, "\n")
}

func analyzAndReportEvent(ssA *ssAnalyzer,
    joinMap map[string]interface{}) error {
    //joinMap["explain"] = ""
    podId := ssA.dbConn.GetPodID()
    containerId := ssA.dbConn.GetContainerID()
    for k := range joinMap {
        if strings.HasPrefix(k, "explain") {
            delete(joinMap, k)
        }
    }
    mExplainData := ssA.getQueryExplain(joinMap["statement"].(string))
    for idx, m := range mExplainData {
        joinMap["explain"+strconv.Itoa(idx)] = m
    }
    joinMap["app_analyz"] = analyzeSlowLog(joinMap, ssA)
    reason := joinMap["app_analyz"]
    j, err := json.Marshal(joinMap)
    if err != nil {
        common.PrintDefinedErr(
            ErrorCode(common.Fail_Analyzer_Data_Formatting_JSON))
        return err
    }
    jApp := string(j)
    jOSEve := "{}"
    pid := common.GetAppInstanceInfo(
        map[string]interface{}{
            "ContainerId": containerId,
            "Port":        ssA.dbConn.GetPort(),
            "Comm":        "mysqld"},
        "Pid").(int)
    if strings.Contains(joinMap["app_analyz"].(string), osChkStrFlag) {
        //reason = "unknow"
        osEve := AnalyzOSEvents(podId, containerId, pid)
        if osEve != nil {
            reason = osEve["value"]
            j, err = json.Marshal(osEve)
            if err != nil {
                common.PrintDefinedErr(
                    ErrorCode(common.Fail_Analyzer_Data_Formatting_JSON))
                return err
            }
            jOSEve = string(j)
        } else {
            reason = strings.ReplaceAll(
                reason.(string), ", " + osChkStrFlag, "")
        }
    }
    portStr := strconv.Itoa(ssA.dbConn.GetPort())
    desc := "slow SQL occurs"
    extra := fmt.Sprintf(`{"level":"warning"`+
        `,"value":"%s"`+
        `,"ts":"%s"`+
        `,"app_log":%s`+
        `,"reason":"%s"`+
        `,"os_log":%s`+
        `,"pid":"%s"`+
        `,"port":"%s"`+
        `,"podId":"%s"`+
        `,"containerId":"%s"`+
        `,"tag_set":"mysqld"}`,
        desc, time.Unix(time.Now().Unix(), 0).Format(common.TIME_FORMAT), jApp, reason,
        jOSEve, strconv.Itoa(pid), portStr, podId, containerId)
        SubmitAlarm(GetLogEventsDesc(
            Notify_Process_Mysql_Slow_Sql_Type, "", "", desc, extra))
    return nil
}

func handleData(data *[]string, dataLen int, p *interface{}) {
    ssA := (*ssAnalyzer)(unsafe.Pointer(p))
    budget := cap(ssA.mSlowData)
    leftLen := dataLen
    retInfoCnt := budget
    for {
        startPos := dataLen - leftLen
        endPos := startPos + leftLen
        retInfoCnt, leftLen = convertLogToSlowData(
            (*data)[startPos:endPos], leftLen, &ssA.mSlowData, budget)
        for i := 0; i < retInfoCnt; i++ {
            if err := analyzAndReportEvent(ssA, ssA.mSlowData[i]); err != nil {
                break
            }
        }
        if leftLen <= 0 {
            break
        }
    }
}

func (ssA *ssAnalyzer) ExitAnalyzer() {
    ssA.A.ExitAnalyzer()
}

func (ssA *ssAnalyzer) CopyDataToAnalyzer(data []string, dataLen int) {
    ssA.A.CopyDataToAnalyzer(data, dataLen)
}

func NewSlowSqlAnalyzer(dbConn *DBConnect) *ssAnalyzer {
    budget := 10
    ssA := &ssAnalyzer{
        A:         NewAnalyzer(handleData),
        mSlowData: make([]map[string]interface{}, budget),
        dbConn:    dbConn,
    }
    for i := 0; i < budget; i++ {
        ssA.mSlowData[i] = make(map[string]interface{})
    }
    ssA.A.private = (*interface{})(unsafe.Pointer(ssA))
    return ssA
}

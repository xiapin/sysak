package common

import (
    "encoding/json"
    "fmt"
    "io/ioutil"
    "net/http"
    "strconv"
    "strings"
    "unsafe"
    //"reflect"
)

var queryUrl string = "http://127.0.0.1:8400/api/query"
var queryPeriod = 30

func qByTable(table string, timeSecs int) ([]map[string]interface{}, error) {
    var m []map[string]interface{}

    payload := strings.NewReader(fmt.Sprintf(
        "{\"mode\": \"last\", \"time\": \"%ds\", \"table\": [\"%s\"]}",
        timeSecs, table))
    req, err := http.NewRequest("POST", queryUrl, payload)
    if err != nil {
        return nil, err
    }
    req.Header.Add("content-type", "application/json")
    res, err := http.DefaultClient.Do(req)
    if err != nil {
        return nil, err
    }
    defer res.Body.Close()
    bodyBytes, err := ioutil.ReadAll(res.Body)
    if err != nil {
        return nil, err
    }
    err = json.Unmarshal([]byte(string(bodyBytes)), &m)
    if err != nil {
        return nil, err
    }
    return m, nil
}

func getMetricsFromUnity(table string, metrics []string,
    lastNums ...int) []interface{} {
    if len(lastNums) > 0 {
        queryPeriod = lastNums[0] * 30
    }
    m, err := qByTable(table, queryPeriod)
    if err != nil {
        PrintSysError(err)
        return nil
    }
    result := make([]interface{}, len(m))
    for index, value := range m {
        result[index] = make([]interface{}, len(metrics))
        for index2, value2 := range metrics {
            result[index].([]interface{})[index2] =
                value["values"].(map[string]interface{})[value2]
        }
    }
    return result
}

func GetSingleMetricsMulti[T interface{}](table string, metrics string,
    lastNums ...int) []T {
    met := getMetricsFromUnity(table, []string{metrics}, lastNums...)
    if met == nil {
        return nil
    }
    return *(*[]T)(unsafe.Pointer(&met))
}

func GetSingleMetrics[T interface{}](
    table string, metrics string) (T, error) {
    m := GetSingleMetricsMulti[T](table, metrics)
    if m == nil {
        var zero T
        return zero, PrintOnlyErrMsg("get metrics fail!")
    }
    return m[0], nil
}

func GetMultiMetricsMulti[T interface{}](table string, metrics []string,
    lastNums ...int) [][]T {
    met := getMetricsFromUnity(table, metrics, lastNums...)
    if met == nil {
        return nil
    }
    return *(*[][]T)(unsafe.Pointer(&met))
}

func GetMultiMetrics[T interface{}](table string, metrics []string) []T {
    m := GetMultiMetricsMulti[T](table, metrics)
    if m == nil {
        return nil
    }
    return m[0]
}

func GetIOMetrics(table string, metrics []string,
    lastNums ...int) []interface{} {
    if len(lastNums) > 0 {
        queryPeriod = lastNums[0] * 30
    }
    m, err := qByTable(table, queryPeriod)
    if err != nil {
        PrintSysError(err)
        return nil
    }
    result := make([]interface{}, 0)
    for _, value := range m {
        name := value["labels"].(map[string]interface{})["disk_name"].(string)
        if _, err := strconv.Atoi(name[len(name)-1:]); err == nil {
            continue
        }
        result = append(result, make([]interface{}, len(metrics)+1))
        result[len(result)-1].([]interface{})[0] = name
        for index2, value2 := range metrics {
            result[len(result)-1].([]interface{})[index2+1] =
                value["values"].(map[string]interface{})[value2]
        }
    }
    return result
}

func GetAppMetrics(table string, appName string,
    labels []string, metrics []string) []interface{} {
    m, err := qByTable(table, queryPeriod)
    if err != nil {
        PrintSysError(err)
        return nil
    }
    result := make([]interface{}, 0)
    for _, value := range m {
        name := value["labels"].(map[string]interface{})["comm"].(string)
        if name != appName {
            continue
        }
        result = append(result, make(map[string]interface{}))
        for _, label := range labels {
            result[len(result)-1].(map[string]interface{})[label] =
                value["labels"].(map[string]interface{})[label]
        }
        for _, metric := range metrics {
            result[len(result)-1].(map[string]interface{})[metric] =
                value["values"].(map[string]interface{})[metric]
        }
    }
    return result
}

func GetAppLatency(table string, labels []string) map[string]map[string]string {
    m, err := qByTable(table, queryPeriod)
    if err != nil {
        PrintSysError(err)
        return nil
    }

    result := make(map[string]map[string]string)
    for _, line := range m {
        if line["labels"].(map[string]interface{})["APP"].(string) != "MYSQL" {
            continue
        }
        containerID := line["labels"].(
        map[string]interface{})["ContainerID"].(string)
        if len(containerID) > 0 {
            containerID = containerID[:12]
        } else {
            containerID = "NULL"
        }
        if _, ok := result[containerID]; !ok {
            result[containerID] = map[string]string{}
        }
        for _, label := range labels {
            result[containerID][label] = 
                line["labels"].(map[string]interface{})[label].(string)
        }
    }
    return result
}

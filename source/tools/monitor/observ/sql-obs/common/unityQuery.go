package common

import (
    "encoding/json"
    "errors"
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
        return zero, errors.New("get metrics fail!")
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

func GetAppMetrics(table string,
    labels []string, metrics []string) []interface{} {
    m, err := qByTable(table, queryPeriod)
    if err != nil {
        fmt.Println(err)
        return nil
    }
    result := make([]interface{}, 0)
    for _, value := range m {
        name := value["labels"].(map[string]interface{})["comm"].(string)
        if name != "mysqld" {
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

func GetAppLatency(table string) map[string]*[6]uint64 {
    m, err := qByTable(table, queryPeriod)
    if err != nil {
        PrintOnlyErrMsg("Can't get ntopo rt info.")
        return nil
    }

    result := make(map[string]*[6]uint64)
    buckets := make(map[string]*[]uint64)

    for _, line := range m {
        if line["labels"].(map[string]interface{})["APP"].(string) != "MYSQL" {
            continue
        }
        // fmt.Println(line)
        containerID := line["labels"].(
        map[string]interface{})["ContainerID"].(string)[0:12]
        if _, ok := result[containerID]; !ok {
            result[containerID] = &[6]uint64{}
            buckets[containerID] = BuildBucket()
        }
        result[containerID][0] += 1                                                                           // count
        result[containerID][1] += uint64(line["values"].(
            map[string]interface{})["ReqBytes"].(float64))  // req bytes
        result[containerID][2] += uint64(line["values"].(
            map[string]interface{})["RespBytes"].(float64)) // resp bytes
        InsertBucket(uint64(line["values"].(
            map[string]interface{})["Latency"].(float64)),
            buckets[containerID])
        result[containerID][5] += uint64(line["values"].(
            map[string]interface{})["Latency"].(float64))
    }

    // fmt.Println("========")
    for key, bucket := range buckets {
        // fmt.Println(key, "request count:", result[key][0])
        if result[key][0] > 0 {
            result[key][1] /= result[key][0] // avg request bytes
            result[key][2] /= result[key][0] // avg response bytes
            result[key][5] /= result[key][0]
        }
        result[key][3] = GetPercentile(
            bucket, 95, result[key][0]) // P95: ns
        result[key][4] = GetPercentile(
            bucket, 99, result[key][0]) // P99: ns
        // result[key][0] =
        //     (result[key][0] + uint64(queryPeriod)) / uint64(queryPeriod) // req count
    }
    // fmt.Println("========")

    // for key, value := range result {
    //     fmt.Println(key)
    //     fmt.Println(*value)
    // }
    return result
}

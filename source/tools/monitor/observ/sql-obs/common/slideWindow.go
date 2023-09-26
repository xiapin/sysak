package common

import (
    "bufio"
    "fmt"
    "math"
    "os"
)

type DataItem struct {
    window       uint32
    usedWin      uint32
    nrSample     uint32
    movWinData   []float64
    curWinMinVal float64
    curWinMaxVal float64
    moveAvg      float64
    baseThresh   float64

    comThresh            float64
    shouldUpdThreshComp  bool
    decRangeThreshAvg    float64
    decRangeCnt          uint32
    minStableThresh      float64
    maxStableThresh      float64
    stableThreshAvg      float64
    nrStableThreshSample uint32

    dynThresh float64
}

// type DataAggregator struct {
//     DataDic map[string]*DataItem
// }

func AddItem(dataAggregator map[string]*DataItem, name string, _window uint32) {
    dataAggregator[name] = new(DataItem)
    dataAggregator[name].window = _window
    dataAggregator[name].usedWin = 0
    dataAggregator[name].nrSample = 0
    dataAggregator[name].movWinData = []float64{}
    dataAggregator[name].curWinMinVal = math.MaxFloat64
    dataAggregator[name].curWinMaxVal = 0
    dataAggregator[name].moveAvg = 0
    dataAggregator[name].baseThresh = 0
    dataAggregator[name].comThresh = 0
    dataAggregator[name].shouldUpdThreshComp = true
    dataAggregator[name].decRangeThreshAvg = 0
    dataAggregator[name].decRangeCnt = 0
    dataAggregator[name].minStableThresh = math.MaxFloat64
    dataAggregator[name].maxStableThresh = 0
    dataAggregator[name].stableThreshAvg = 0
    dataAggregator[name].nrStableThreshSample = 0
    dataAggregator[name].dynThresh = math.MaxFloat64
}

func calBaseThresh(dataAggregator map[string]*DataItem, key string, value float64) float64 {
    data := dataAggregator[key]
    thresh := -1.0
    data.nrSample += 1
    // fmt.Println(key, data.nrSample, data.window)
    if data.nrSample >= data.window {
        if len(data.movWinData) < int(data.window) {
            data.movWinData = append(data.movWinData, value)
        } else {
            data.movWinData[data.usedWin%data.window] = value
        }
        moveAvg := avg(data.movWinData, data.window)
        nrThreshSample := data.nrSample + 1 - data.window
        thresh = math.Max(data.curWinMaxVal-moveAvg, moveAvg-data.curWinMinVal)
        threshAvg := (data.baseThresh*float64(nrThreshSample-1) + thresh) / float64(nrThreshSample)
        data.baseThresh = threshAvg
        data.moveAvg = moveAvg
        data.curWinMaxVal = math.Max(data.curWinMaxVal, value)
        data.curWinMinVal = math.Min(data.curWinMinVal, value)

        data.usedWin += 1
        if data.usedWin >= data.window {
            data.curWinMaxVal = 0
            data.curWinMinVal = math.MaxFloat64
            data.usedWin = 0
        }
    } else {
        data.movWinData = append(data.movWinData, value)
        data.curWinMaxVal = math.Max(data.curWinMaxVal, value)
        data.curWinMinVal = math.Min(data.curWinMinVal, value)
        data.usedWin += 1
    }
    return thresh
}

func calStableThresh(dataAggregator map[string]*DataItem, key string, curBaseThresh float64, curThresh float64) {
    data := dataAggregator[key]
    avg := data.decRangeThreshAvg
    if curThresh-avg < ((curBaseThresh - avg) / 10.0) {
        tSum := data.stableThreshAvg*float64(data.nrStableThreshSample) + curThresh
        data.nrStableThreshSample += 1
        data.stableThreshAvg = tSum / float64(data.nrStableThreshSample)
        data.minStableThresh = math.Min(data.minStableThresh, curThresh)
        data.maxStableThresh = math.Max(data.maxStableThresh, curThresh)
        if data.nrStableThreshSample*2 >= data.window*3 {
            data.comThresh = math.Max(data.stableThreshAvg-data.minStableThresh, data.maxStableThresh-data.stableThreshAvg)
            data.shouldUpdThreshComp = false
            data.minStableThresh = math.MaxFloat64
            data.maxStableThresh = 0.0
            data.stableThreshAvg = 0
            data.decRangeThreshAvg = 0
            data.nrStableThreshSample = 0
            data.decRangeCnt = 0
        }
    }
}

func CalCompThresh(dataAggregator map[string]*DataItem, key string, lastBaseThresh float64, curThresh float64) {
    data := dataAggregator[key]
    curBaseThresh := data.baseThresh
    if data.shouldUpdThreshComp && (data.comThresh < curBaseThresh || data.usedWin == 0) {
        data.comThresh = curBaseThresh
    }
    if curBaseThresh < lastBaseThresh {
        tSum := data.decRangeThreshAvg*float64(data.decRangeCnt) + curThresh
        data.decRangeCnt += 1
        data.decRangeThreshAvg = tSum / float64(data.decRangeCnt)
        if data.decRangeCnt*2 >= data.window*3 {
            calStableThresh(dataAggregator, key, curBaseThresh, curThresh)
        }
    } else {
        data.minStableThresh = math.MaxFloat64
        data.maxStableThresh = 0
        data.stableThreshAvg = 0
        data.decRangeThreshAvg = 0
        data.nrStableThreshSample = 0
        data.decRangeCnt = 0
    }
}

func UpdateDynThresh(dataAggregator map[string]*DataItem, key string, value float64) {
    // fmt.Println(key, len(dataAggregator), len(dataAggregator[key].movWinData))
    data := dataAggregator[key]
    lastBaseThresh := data.baseThresh

    curThresh := calBaseThresh(dataAggregator, key, value)
    if curThresh != -1.0 {
        CalCompThresh(dataAggregator, key, lastBaseThresh, curThresh)
        data.dynThresh = data.baseThresh + data.moveAvg + data.comThresh
    }
}

func DisableThreshComp(dataAggregator map[string]*DataItem, key string) {
    data := dataAggregator[key]
    if data.shouldUpdThreshComp == true {
        data.shouldUpdThreshComp = false
        data.dynThresh = data.baseThresh + data.moveAvg
        data.comThresh = 0.000001
    }
}

func GetNrDataSample(dataAggregator map[string]*DataItem, key string) uint32 {
    return dataAggregator[key].nrSample
}

func GetDynThresh(dataAggregator map[string]*DataItem, key string) float64 {
    return dataAggregator[key].dynThresh
}

func avg(list []float64, count uint32) float64 {
    sum := 0.0
    for _, v := range list {
        sum += v
    }
    return sum / float64(count)
}

func Sum(list []float64) float64 {
    sum := 0.0
    for _, v := range list {
        sum += v
    }
    return sum
}

func ReadFileToList(path string) []string {
    f, err := os.Open(path)
    if err != nil {
        fmt.Printf("Open file %s error.", path)
        return nil
    }
    defer f.Close()

    var result []string
    scanner := bufio.NewScanner(f)
    for scanner.Scan() {
        result = append(result, scanner.Text())
    }

    if err := scanner.Err(); err != nil {
        fmt.Printf("Open file %s error.", path)
        return nil
    }
    return result
}

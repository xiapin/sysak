package main

import "C"

import (
    "fmt"
	"time"
	"strings"
	"strconv"
	"github.com/gogo/protobuf/proto"
	"github.com/prometheus/prometheus/prompb"
	"github.com/golang/snappy"
	"regexp"
)

//export metricSnappy
func metricSnappy(prome_ptr *string, ret *[]byte) int {
	/*
		input:
			prome_ptr, *string, prometheus data
			ret, *[]byte, receive return byte data
		output:
			int, length of return data
	*/
	// initialize
	var prome = strings.Split(*prome_ptr, "\n")
	timeSeries := []prompb.TimeSeries{};
	timestamp := time.Now().UnixNano()

	for i:=0;i<len(prome);i=i+1{
		metric := prome[i];

		// seperate data
		sep := "[{} ]"
		result := regexp.MustCompile(sep).Split(metric, -1)
		fmt.Println(result)
		if (len(result)!=4){
			continue
		}
		name := result[0]
		labels_ := result[1]
		value, _ := strconv.ParseFloat(result[2], 64)
		re := regexp.MustCompile(`([^=]+)=["]*([^,"]+)["]*[,]*`)
		labelTotal := re.FindAllStringSubmatch(labels_, -1)

		// start to construct
		var labels = []prompb.Label{
			{Name: "__name__", Value: name},
		}
		for j:=0;j<len(labelTotal);j=j+1{
			labelName := labelTotal[j][1]
			labelValue := labelTotal[j][2]
			var label = prompb.Label{
				Name: labelName, Value: labelValue,
			}
			labels = append(labels, label)
		}
		var samples = []prompb.Sample{
			{Timestamp: timestamp / 1000000, Value: value},
		}
		timeMetric := prompb.TimeSeries{
			Labels:labels,
			Samples:samples,
		};
		timeSeries = append(timeSeries,timeMetric)

	}
	if (len(timeSeries)==0){
		return 0
	}

	// do proto and snappy
	data, _ := proto.Marshal(&prompb.WriteRequest{Timeseries: timeSeries})
	bufBody := snappy.Encode(nil, data)
	*ret = bufBody
	return len(bufBody)
}


func main() {
	var s = "sysak_proc_cpu_total{mode=\"user\",instance=\"i-wz9d3tqjhpb8esj8ps4z\"} 0.8\nsysak_proc_cpu_total{mode=\"total\",instance=\"i-wz9d3tqjhpb8esj8ps4z\"} 3960.0\n"
	var s_ptr = &s
	var prom []byte
	p := &prom;
	metricSnappy(s_ptr, p) 
	fmt.Println(*p)

}
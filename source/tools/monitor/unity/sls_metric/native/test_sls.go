package main
import (
	"bytes"
	"flag"
	"fmt"
	"github.com/gogo/protobuf/proto"
	"github.com/prometheus/prometheus/prompb"
	"io/ioutil"
	"net/http"
	"github.com/golang/snappy"
	"time"
	"strings"
	"strconv"
	"regexp"
)

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


func MockRemoteWrite() {
	project := flag.String("project", "***", "")
	metricStore := flag.String("metricstore", "***", "")
	endpoint := flag.String("endpoint", "***", "")
	akId := flag.String("akid", "***", "") // AccessKey信息。
	akKey := flag.String("aksecret", "***", "")
	flag.Parse()

	Url := fmt.Sprintf("https://%s.%s/prometheus/%s/%s/api/v1/write", *project, *endpoint, *project, *metricStore)

	var data []byte;
	ret := &data;
	var s = "sysak_proc_cpu_total{mode=\"user\",instance=\"i-wz9d3tqjhpb8esj8ps4z\"} 0.8\nsysak_proc_cpu_total{mode=\"total\",instance=\"i-wz9d3tqjhpb8esj8ps4z\"} 3960.0\nsysak_proc_cpu_total{mode=\"user2\",instance=\"i-wz9d3tqjhpb8iesj8ps4z\"} 0.9\n"
	prome_ptr := &s
	r := metricSnappy(prome_ptr, ret)
	bufBody := *ret
	if (r==0){
		fmt.Println("len(data)=",len(data))
		fmt.Println("len=0")
		return
	}

	// data, _ := proto.Marshal(&prompb.WriteRequest{Timeseries: timeSeries})
	// bufBody := snappy.Encode(nil, data)
	fmt.Println(bufBody)
	rwR, err := http.NewRequest("POST", Url, ioutil.NopCloser(bytes.NewReader(bufBody)))
	rwR.Header.Add("Content-Encoding", "snappy")
	rwR.Header.Set("Content-Type", "application/x-protobuf")
	rwR.SetBasicAuth(*akId, *akKey) // 设置basic auth信息。
	if err != nil {
		fmt.Println(err.Error())
		return
	}

	start := time.Now().UnixNano() / 1000000 //ms
	do, err := http.DefaultClient.Do(rwR)
	end := time.Now().UnixNano() / 1000000 // ms
	if err != nil {
		panic(err)
	}
	status, result := parseResp(do)

	fmt.Println("status:", status, "result:", result, "duration:", end-start)
}

func parseResp(resp *http.Response) (status, data string) {
	defer resp.Body.Close()
	body, err := ioutil.ReadAll(resp.Body) // 需要读完body内容。
	if err != nil {
		panic(err)
	}
	return resp.Status, string(body)
}

func main(){
	MockRemoteWrite()
}
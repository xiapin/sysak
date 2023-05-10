package exporter

import (
	"github.com/chentao-kernel/cloud_ebpf/ebpf"
	"github.com/chentao-kernel/cloud_ebpf/ebpf/proto"
	"github.com/chentao-kernel/cloud_ebpf/k8s"
	"github.com/chentao-kernel/cloud_ebpf/util"
	"github.com/prometheus/client_golang/prometheus"
	"github.com/prometheus/client_golang/prometheus/promhttp"
	"net/http"
	"strconv"
)

const namespace = "cloud_ebpf"

type protoExporter struct {
	proto     string
	protoDesc *prometheus.Desc
}

func NewProtoExporter() *protoExporter {
	e := &protoExporter{
		protoDesc: prometheus.NewDesc(
			"proto_request_time_total",
			"Count of time distribution",
			// label
			[]string{"proto", "time"},
			nil,
		),
	}
	return e
}

func (e *protoExporter) GetProtoDatas() []ebpf.ProtoHist {
	data := make([]ebpf.ProtoHist, 4)
	protos := []string{"http", "mysql", "dns", "redis"}
	for i, val := range protos {
		data[i] = ebpf.CgoGetProtoHist(val)
	}
	return data
}

/*
# HELP proto_request_time_total Count of time distribution
# TYPE proto_request_time_total gauge
proto_request_time_total{proto="dns",time="100ms"} 0
proto_request_time_total{proto="dns",time="10ms"} 5
proto_request_time_total{proto="dns",time="1s"} 0
proto_request_time_total{proto="dns",time="1s+"} 0
proto_request_time_total{proto="dns",time="500ms"} 0
proto_request_time_total{proto="http",time="100ms"} 7
proto_request_time_total{proto="http",time="10ms"} 0
proto_request_time_total{proto="http",time="1s"} 0
proto_request_time_total{proto="http",time="1s+"} 0
proto_request_time_total{proto="http",time="500ms"} 0
*/
func (e *protoExporter) setProtoValue(ch chan<- prometheus.Metric, hist *ebpf.ProtoHist) {
	timeLabels := []string{"10ms", "100ms", "500ms", "1s", "1s+"}
	for i, count := range hist.Hist {
		ch <- prometheus.MustNewConstMetric(
			e.protoDesc,
			prometheus.GaugeValue,
			float64(count),
			hist.Proto,
			timeLabels[i],
		)
	}
}

func (e *protoExporter) Collect(ch chan<- prometheus.Metric) {
	hists := e.GetProtoDatas()
	for _, hist := range hists {
		e.setProtoValue(ch, &hist)
	}
}

func (e *protoExporter) Describe(ch chan<- *prometheus.Desc) {
	ch <- e.protoDesc
}

type eventExporter struct {
	proto     string
	eventDesc *prometheus.Desc
}

/*
proto_event_total{ContainerName="",DstIp="100.100.2.136",DstPort="53",Id="1",Method="",Pid="1765",Proto="dns",ReponseStatus="domain name error",Url="bbb."} 0.35
proto_event_total{ContainerName="",DstIp="100.100.2.136",DstPort="53",Id="2",Method="",Pid="1765",Proto="dns",ReponseStatus="domain name error",Url="bbb."} 0.3
*/
func NewEventExporter() *eventExporter {
	e := &eventExporter{
		eventDesc: prometheus.NewDesc(
			"proto_event_total",
			"Timeout or error response event report",
			// label
			[]string{"Id", "Proto", "DstPort", "DstIp", "Pid", "Comm", "ContainerName", "Url", "Method", "ReponseStatus"},
			nil,
		),
	}
	return e
}

var Id int = 0

func (e *eventExporter) setEventValue(ch chan<- prometheus.Metric) {
	var container string
	util.MQueue.Mutex.Lock()
	len := util.MQueue.Queue.Len()
	for i := 0; i < len; i++ {
		obj := util.MQueue.Queue.Front()
		event := obj.Value.(*proto.MsgParser)
		Id++
		container = k8s.ContainerManagerPr.GetContainerNameWithPid(strconv.Itoa(int(event.ParserRst.Pid)))
		if container == "" {
			container = "NA"
		}
		ch <- prometheus.MustNewConstMetric(
			e.eventDesc,
			prometheus.GaugeValue,
			float64(event.ParserRst.ResponseTimeMs),
			strconv.Itoa(Id),
			event.ParserRst.Proto,
			strconv.Itoa(int(event.ParserRst.Dport)),
			event.ParserRst.Dip,
			strconv.Itoa(int(event.ParserRst.Pid)),
			event.ParserRst.Comm,
			container,
			event.ParserRst.Url,
			event.ParserRst.Method,
			event.ParserRst.ResStatus,
		)
		util.MQueue.Queue.Remove(obj)
	}
	util.MQueue.Mutex.Unlock()
}

func (e *eventExporter) Collect(ch chan<- prometheus.Metric) {
	e.setEventValue(ch)
}

func (e *eventExporter) Describe(ch chan<- *prometheus.Desc) {
	ch <- e.eventDesc
}

func ExporterInit() {

	protoExp := NewProtoExporter()
	reg := prometheus.NewPedanticRegistry()
	reg.MustRegister(protoExp)

	eventExp := NewEventExporter()
	reg.MustRegister(eventExp)

	// 暴露自定义的指标
	http.Handle("/metrics", promhttp.HandlerFor(reg, promhttp.HandlerOpts{}))
	http.ListenAndServe(":9111", nil)
}

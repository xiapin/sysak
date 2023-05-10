package ebpf

/*
#include "cfunc.h"
extern void CgoDataCallback(void *custom_data, struct conn_data_event_t *event);
extern void CgoStatCallback(void *custom_data, struct conn_stats_event_t *event);
extern void CgoEventCallback(void *custom_data, struct conn_ctrl_event_t *event);
#cgo LDFLAGS: -ldl
*/
import "C"
import (
	"errors"
	"fmt"
	"github.com/chentao-kernel/cloud_ebpf/config"
	"github.com/chentao-kernel/cloud_ebpf/ebpf/proto"
	"github.com/chentao-kernel/cloud_ebpf/util"
	"log"
	"unsafe"
)

const (
	PROTOCOL_FILTER int = 0 // 默认值-1。协议类型过滤器，为-1时代表Trace所有协议，其他只允许某一协议
	TGID_FILTER     int = 1 // 默认值-1。进程过滤器，为-1时代表Trace所有进程，其他只允许某一进程
	SELF_FILTER     int = 2 // 默认值-1。是否Disable自身的Trace，为-1代表不Disable，其他情况会传入本进程的ID，这时需要过滤掉该进程所有的数据
	PORT_FILTER     int = 3 // 默认值-1。端口过滤器，为-1时代表Trace所有端口，其他只允许某一端口
	DATA_SAMPLING   int = 4
)

const (
	AF_INET  int = 2
	AF_INET6 int = 10
)

type ProtoHist struct {
	Proto string
	Hist  [5]uint64
}

func msgDataInit(msg *proto.MsgData, event *C.struct_conn_data_event_t) {
	msg.Offset = 0
	msg.Header.Pid = uint32(event.attr.conn_id.tgid)
	msg.Header.Fd = uint32(event.attr.conn_id.fd)
	msg.Header.RspTimeNs = uint64(event.attr.ts)
	msg.Header.Proto = proto.PROTO_TYPE(event.attr.protocol)
	msg.Header.Type = proto.MSG_TYPE(event.attr._type)
	msg.Header.Recode = uint32(event.attr.proto_rescode)
	msg.Header.Comm = string(C.GoBytes(unsafe.Pointer(&event.attr.comm), 16))
	msg.Data = C.GoBytes(unsafe.Pointer(&event.msg), C.int(event.attr.org_msg_size))
	ipInfo := C.cgo_ebpf_addr2ip_info(&event.attr.addr)
	msg.Header.Dport = uint16(ipInfo.port)
	msg.Header.Dip = string(C.GoBytes(unsafe.Pointer(&ipInfo.ip), C.int(ipInfo.ip_len)))
}

//export CgoDataCallback
func CgoDataCallback(custom_data unsafe.Pointer, event *C.struct_conn_data_event_t) {
	var msgData proto.MsgData
	var err error
	var parser *proto.MsgParser

	msgDataInit(&msgData, event)
	pro := proto.PROTO_TYPE(event.attr.protocol)
	switch pro {
	case proto.PROTO_HTTP:
		parser = proto.NewProtoParser("http", &msgData, proto.HttpParse)
		err = parser.Parser(&msgData)
	//case proto.PROTO_MYSQL:
	case proto.PROTO_DNS:
		parser = proto.NewProtoParser("dns", &msgData, proto.DnsParse)
		err = parser.Parser(&msgData)
		/* test
		log.Printf("url:%v,err:%v,restatuc:%v, port:%v, ip:%v", parser.ParserRst.Url, err,
					parser.ParserRst.ResStatus, parser.ParserRst.Dport, parser.ParserRst.Dip)
		*/
	//case proto.PROTO_REDIS:
	default:
	}
	if err == nil {
		// test
		fmt.Println("", msgData)
	}

	log.Printf("data handle")
	util.MQueue.Mutex.Lock()
	util.MQueue.Queue.PushBack(parser)
	util.MQueue.Mutex.Unlock()
}

func cgoSetDataCallback() {
	C.cgo_ebpf_setup_net_data_process_func(C.net_data_process_func_t(unsafe.Pointer(C.CgoDataCallback)))
}

//export CgoStatCallback
func CgoStatCallback(custom_data unsafe.Pointer, event *C.struct_conn_stats_event_t) {
	fmt.Println("stat handle")
}

func cgoSetStatCallback() {
	C.cgo_ebpf_setup_net_statistics_process_func(C.net_statistics_process_func_t(unsafe.Pointer(C.CgoDataCallback)))
}

//export CgoEventCallback
func CgoEventCallback(custom_data unsafe.Pointer, event *C.struct_conn_ctrl_event_t) {
	fmt.Println("event handle")
}

func cgoSetEventCallback() {
	C.cgo_ebpf_setup_net_event_process_func(C.net_ctrl_process_func_t(unsafe.Pointer(C.CgoEventCallback)))
}

func cgoEnvInit() {
	C.cgo_env_init()
}

func cgoSetConfig(opt int, value int) {
	C.cgo_ebpf_set_config(C.int(opt), C.int(value))
}

func cgoConfigInit() error {
	config := config.ConfigGet()
	if config == nil {
		return errors.New("Config is nil")
	}
	for _, proto := range config.Netobserv.Proto {
		cgoSetConfig(PROTOCOL_FILTER, protoNameToNum(proto))
	}
	cgoSetConfig(TGID_FILTER, config.Netobserv.Pid)
	cgoSetConfig(SELF_FILTER, -1)
	cgoSetConfig(PORT_FILTER, config.Netobserv.Port)
	cgoSetConfig(DATA_SAMPLING, config.Netobserv.Sample)
	return nil
}

func CgoEbpfEnvInit() {
	cgoEnvInit()
	cgoSetDataCallback()
	cgoSetStatCallback()
	cgoSetEventCallback()
	cgoConfigInit()
}

func CgoPollEvents(maxEvent int) {
	log.Printf("Start poll events...")
	stopFlag := C.int(0)
	for {
		C.cgo_poll_events(C.int(maxEvent), &stopFlag)
	}
}

// return value
func cgoBpfGetMapValue(fd C.int, key C.int) uint64 {
	return uint64(C.cgo_ebpf_get_map_value(fd, key))
}

func protoNameToNum(proto string) int {
	switch proto {
	case "http":
		return 1
	case "mysql":
		return 2
	case "dns":
		return 3
	case "redis":
		return 4
	case "kafaka":
		return 5
	case "pgsql":
		return 6
	case "mongo":
		return 7
	case "dubbo":
		return 8
	case "hsf":
		return 9
	default:
		log.Printf("Protocol not support:%s\n", proto)
		return -1
	}
}

func CgoGetProtoHist(proto string) ProtoHist {
	fd := C.cgo_ebpf_get_proto_fd(C.int(protoNameToNum(proto)))
	var hist ProtoHist
	hist.Proto = proto
	for i := 0; i < len(hist.Hist); i++ {
		value := cgoBpfGetMapValue(fd, C.int(i))
		hist.Hist[i] = value
	}
	return hist
}

/* test
func main() {
	CgoEnvInit()
	CgoSetDataCallback()
	CgoPollEvents(100)
}
*/

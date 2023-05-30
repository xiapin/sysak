package nginx

import (
	"context"
	_ "embed"
	"flag"
	"fmt"
	"log"
	"net/http"
	"os"
	"os/signal"
	"syscall"

	bpf "github.com/aquasecurity/libbpfgo"
	"github.com/prometheus/client_golang/prometheus"
	"github.com/prometheus/client_golang/prometheus/promhttp"
	"golang.org/x/sys/unix"
)

//#include "../ebpf/src/nginx.h"
import "C"

var (
	bind, configFile string
)

func NginxInit() {
	flag.StringVar(&bind, "web.listen-address", ":9988", "Address to listen on for the web interface and API.")
	flag.StringVar(&configFile, "config.file", "config.yml", "Nginx log exporter configuration file name.")
	flag.Parse()

	go NginxEbpfInit()

	cfg, err := LoadFile(configFile)
	if err != nil {
		log.Panic(err)
	}

	for _, app := range cfg.App {
		go NewCollector(app).Run()
	}

	fmt.Printf("running HTTP server on address %s\n", bind)

	http.Handle("/metrics", promhttp.HandlerFor(
		prometheus.DefaultGatherer,
		promhttp.HandlerOpts{
			EnableOpenMetrics: true,
		},
	))
	if err := http.ListenAndServe(bind, nil); err != nil {
		log.Fatalf("start server with error: %v\n", err)
	}
}

type Trace struct {
	ts    uint64
	pid   uint32
	cpu1  uint32
	cpu2  uint32
	sport uint16
	dport uint16
}

func showTrace(data []byte) {
	fmt.Printf("hello nginx\n")
}

func attachProgs(bpfModule *bpf.Module) error {
	progIter := bpfModule.Iterator()
	for {
		prog := progIter.NextProgram()
		if prog == nil {
			break
		}
		if _, err := prog.AttachGeneric(); err != nil {
			return err
		}
	}
	return nil
}

func pollData(bpfModule *bpf.Module) {
	dataChan := make(chan []byte)
	lostChan := make(chan uint64)

	pb, err := bpfModule.InitPerfBuf("events_map", dataChan, lostChan, 1)
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
	}
	pb.Start()
	ctx, stop := signal.NotifyContext(context.Background(), os.Interrupt, syscall.SIGTERM)
	defer func() {
		pb.Stop()
		pb.Close()
		stop()
	}()
	fmt.Printf("%-8s %-6s %-6s %-5s %-5s %-5s %-5s\n",
		"TIME", "PID", "CPU1", "CPU2", "SPORT", "DPORT", "DELAY(us)")
loop:
	for {
		select {
		case data := <-dataChan:
			showTrace(data)
		case e := <-lostChan:
			fmt.Printf("Events lost:%d\n", e)
		case <-ctx.Done():
			break loop
		}
	}
}

func NginxEbpfInit() error {
	var err error
	var module *bpf.Module
	var prog *bpf.BPFProg
	var offset uint32

	if err = unix.Setrlimit(unix.RLIMIT_MEMLOCK, &unix.Rlimit{
		Cur: unix.RLIM_INFINITY,
		Max: unix.RLIM_INFINITY,
	}); err != nil {
		return err
	}
	args := bpf.NewModuleArgs{BPFObjBuff: nginxBpf}
	if module, err = bpf.NewModuleFromBufferArgs(args); err != nil {
		return err
	}
	if prog, err = module.GetProgram("ngx_close_connection_fn"); err != nil {
		return fmt.Errorf("Ebpf ngx_close_connection_fn get failed:%v\n", err)
	}
	offset = uint32(C.ngx_close_connection_offset())
	fmt.Printf("Nginx eBPF start poll data, offset:%d, prog:%p\n", offset, prog)
	/*
			if _, err = prog.AttachUprobe(-1, "/usr/sbin/nginx", offset); err != nil {
				return fmt.Errorf("Ebpf ngx_close_connection_fn attach failed:%v\n", err)
			}

		if prog, err = module.GetProgram("ngx_http_create_request_fn"); err != nil {
			return fmt.Errorf("Ebpf ngx_http_create_request_fn get failed:%v\n", err)
		}
		offset = uint32(C.ngx_http_create_request_offset())
		if _, err = prog.AttachUprobe(-1, "/usr/sbin/nginx", offset); err != nil {
			return fmt.Errorf("Ebpf ngx_http_create_request_fn attach failed:%v\n", err)
		}
	*/
	//pollData(module)
	return nil
}

//go:embed nginx.bpf.o
var nginxBpf []byte

package main

import (
	"github.com/chentao-kernel/cloud_ebpf/config"
	"github.com/chentao-kernel/cloud_ebpf/ebpf"
	"github.com/chentao-kernel/cloud_ebpf/exporter"
	"github.com/chentao-kernel/cloud_ebpf/k8s"
	"github.com/chentao-kernel/cloud_ebpf/util"
	"log"
	"os"
	"os/signal"
	"syscall"
	"time"
)

func waitSignal(signal chan os.Signal) {
	select {
	case sig := <-signal:
		log.Printf("Received signal [%v], and will exit", sig)
		os.Exit(-1)
	}
}

// for test
func protoHistTest() {
	for {
		hist := ebpf.CgoGetProtoHist("http")
		log.Printf("http:%v", hist)
		hist = ebpf.CgoGetProtoHist("dns")
		log.Printf("dns:%v", hist)
		time.Sleep(1 * time.Second)
	}
}

func kubernetsInit() {
	err := k8s.KubernetsInit()
	if err != nil {
		log.Printf("Get pod info failed:%v", err)
	}
}

func configInit() {
	err := config.ConfigInit()
	if err != nil {
		log.Printf("Config init failed:%v", err)
	}
}

func main() {
	configInit()
	util.MetricQueueInit()
	ebpf.CgoEbpfEnvInit()
	sigCh := make(chan os.Signal, 1)
	signal.Notify(sigCh, os.Interrupt, syscall.SIGTERM)
	go waitSignal(sigCh)
	//go protoHistTest()
	go exporter.ExporterInit()
	go kubernetsInit()
	time.Sleep(1 * time.Second)
	ebpf.CgoPollEvents(100)
}

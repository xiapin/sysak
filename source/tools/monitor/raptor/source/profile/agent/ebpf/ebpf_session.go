package ebpf

import "C"
import (
	"bytes"
	"context"
	_ "embed"
	"encoding/binary"
	"fmt"
	"strconv"
	"sync"
	"syscall"
	"unsafe"

	"github.com/chentao-kernel/cloud_ebpf/profile/sd"
	"github.com/chentao-kernel/cloud_ebpf/profile/spy"
	"github.com/pyroscope-io/pyroscope/pkg/agent/ebpfspy/cpuonline"
	"github.com/pyroscope-io/pyroscope/pkg/agent/log"
	"golang.org/x/sys/unix"

	bpf "github.com/aquasecurity/libbpfgo"
)

//#cgo CFLAGS: -I./bpf/
//#include <linux/types.h>
//#include "profile.bpf.h"
import "C"

type TaskStack struct {
	pid        uint32
	kern_stack int64
	user_stack int64
}

type TaskCounter struct {
	counts uint64
	delta  uint64
}

type NetArgs struct {
	Dport int
	Sport int
	Delay int
}

type Session struct {
	logger           log.Logger
	pid              int
	cpu              int
	space            int
	sampleRate       uint32
	symbolCacheSize  int
	serviceDiscovery sd.ServiceDiscovery
	onlyServices     bool
	netArgs          NetArgs

	perfEventFds []int

	symCache *symbolCache

	module        *bpf.Module
	mapCounts     *bpf.BPFMap
	mapStacks     *bpf.BPFMap
	mapArgs       *bpf.BPFMap
	prog          *bpf.BPFProg
	link          *bpf.BPFLink
	modMutex      sync.Mutex
	roundNumber   int
	ProfileTypes  []spy.ProfileType
	lastTaskInfos map[TaskStack]TaskCounter
}

const btf = "should not be used" // canary to detect we got relocations

func NewSession(netArgs NetArgs, logger log.Logger, profileTypes []spy.ProfileType, pid int, cpu int, space int, sampleRate uint32, symbolCacheSize int, serviceDiscovery sd.ServiceDiscovery, onlyServices bool) *Session {
	return &Session{
		logger:           logger,
		ProfileTypes:     profileTypes,
		pid:              pid,
		cpu:              cpu,
		space:            space,
		sampleRate:       sampleRate,
		symbolCacheSize:  symbolCacheSize,
		serviceDiscovery: serviceDiscovery,
		onlyServices:     onlyServices,
		netArgs:          netArgs,
	}
}

func (s *Session) Start() error {
	var err error
	if err = unix.Setrlimit(unix.RLIMIT_MEMLOCK, &unix.Rlimit{
		Cur: unix.RLIM_INFINITY,
		Max: unix.RLIM_INFINITY,
	}); err != nil {
		return err
	}

	s.modMutex.Lock()
	defer s.modMutex.Unlock()

	if s.symCache, err = newSymbolCache(s.symbolCacheSize); err != nil {
		return err
	}
	args := bpf.NewModuleArgs{BPFObjBuff: onCpuBpf,
		BTFObjPath: btf}
	if s.module, err = bpf.NewModuleFromBufferArgs(args); err != nil {
		return err
	}
	if err = s.module.BPFLoadObject(); err != nil {
		return err
	}
	if s.prog, err = s.module.GetProgram("do_perf_event"); err != nil {
		return err
	}
	if err = s.findMaps(); err != nil {
		return err
	}
	if err = s.initArgs(); err != nil {
		return err
	}
	if err = s.attachPerfEvent(); err != nil {
		return err
	}
	return nil
}

func (s *Session) Reset(cb func(labels *spy.Labels, name []byte, value uint64, pid uint32) error) error {
	s.logger.Debugf("tao ebpf session reset")
	s.modMutex.Lock()
	defer s.modMutex.Unlock()

	s.roundNumber += 1

	refreshResult := make(chan error)
	go func() {
		refreshResult <- s.serviceDiscovery.Refresh(context.TODO())
	}()

	keys, values, batch, err := s.getCountsMapValues()
	if err != nil {
		return err
	}

	err = <-refreshResult
	if err != nil {
		return err
	}

	type sf struct {
		pid    uint32
		count  uint32
		kStack []byte
		uStack []byte
		comm   string
		labels *spy.Labels
	}
	var sfs []sf
	knownStacks := map[uint32]bool{}
	for i, key := range keys {
		ck := (*C.struct_profile_key_t)(unsafe.Pointer(&key[0]))
		value := values[i]

		pid := uint32(ck.pid)
		kStackID := int64(ck.kern_stack)
		uStackID := int64(ck.user_stack)
		count := binary.LittleEndian.Uint32(value)
		var comm string = C.GoString(&ck.comm[0])
		if uStackID >= 0 {
			knownStacks[uint32(uStackID)] = true
		}
		if kStackID >= 0 {
			knownStacks[uint32(kStackID)] = true
		}
		// 这里的labels是k8s的标签
		labels := s.serviceDiscovery.GetLabels(pid)
		if labels == nil && s.onlyServices {
			continue
		}
		uStack := s.getStack(uStackID)
		kStack := s.getStack(kStackID)
		sfs = append(sfs, sf{pid: pid, uStack: uStack, kStack: kStack, count: count, comm: comm, labels: labels})
	}
	for _, it := range sfs {
		buf := bytes.NewBuffer(nil)
		buf.Write([]byte(it.comm))
		buf.Write([]byte{';'})
		s.walkStack(buf, it.uStack, it.pid, true)
		s.walkStack(buf, it.kStack, 0, false)
		err = cb(it.labels, buf.Bytes(), uint64(it.count), it.pid)
		if err != nil {
			return err
		}
		//fmt.Printf("tao =====%s || %d\n", string(buf.Bytes()), it.count);
	}
	if err = s.clearCountsMap(keys, batch); err != nil {
		return err
	}
	if err = s.clearStacksMap(knownStacks); err != nil {
		return err
	}
	return nil
}

func (s *Session) StopData() error {
	var zero uint32
	args := C.struct_profile_bss_args_t{
		tgid_filter:  C.uint(^uint32(0)),
		cpu_filter:   C.int(-1),
		space_filter: C.int(-1),
	}
	err := s.mapArgs.UpdateValueFlags(unsafe.Pointer(&zero), unsafe.Pointer(&args), 0)
	if err != nil {
		return err
	}
	return nil
}

func (s *Session) Restart() error {
	return s.initArgs()
}

func (s *Session) Stop() {
	s.symCache.clear()
	// 执行perf close程序会退出
	for fd := range s.perfEventFds {
		_ = syscall.Close(fd)
	}
	s.module.Close()
}

func (s *Session) findMaps() error {
	var err error
	if s.mapArgs, err = s.module.GetMap("args"); err != nil {
		return err
	}
	if s.mapCounts, err = s.module.GetMap("counts"); err != nil {
		return err
	}
	if s.mapStacks, err = s.module.GetMap("stacks"); err != nil {
		return err
	}
	return nil
}

func (s *Session) initArgs() error {
	var zero uint32
	var err error
	var tgidFilter uint32
	if s.pid <= 0 {
		tgidFilter = 0
	} else {
		tgidFilter = uint32(s.pid)
	}
	args := C.struct_profile_bss_args_t{
		tgid_filter:  C.uint(tgidFilter),
		cpu_filter:   C.int(s.cpu),
		space_filter: C.int(s.space),
	}
	err = s.mapArgs.UpdateValueFlags(unsafe.Pointer(&zero), unsafe.Pointer(&args), 0)
	if err != nil {
		return err
	}
	return nil
}

func (s *Session) initNetArgs() error {
	var zero uint32
	var err error

	args := C.struct_net_args{
		dport: C.int(s.netArgs.Dport),
		sport: C.int(s.netArgs.Sport),
		delay: C.int(s.netArgs.Delay),
	}
	err = s.mapArgs.UpdateValueFlags(unsafe.Pointer(&zero), unsafe.Pointer(&args), 0)
	if err != nil {
		return err
	}
	return nil
}

func (s *Session) attachPerfEvent() error {
	var cpus []uint
	var err error
	if cpus, err = cpuonline.Get(); err != nil {
		return err
	}
	for _, cpu := range cpus {
		attr := unix.PerfEventAttr{
			Type:   unix.PERF_TYPE_SOFTWARE,
			Config: unix.PERF_COUNT_SW_CPU_CLOCK,
			Bits:   unix.PerfBitFreq,
			Sample: uint64(s.sampleRate),
		}
		fd, err := unix.PerfEventOpen(&attr, -1, int(cpu), -1, unix.PERF_FLAG_FD_CLOEXEC)
		if err != nil {
			return err
		}
		s.perfEventFds = append(s.perfEventFds, fd)
		if _, err = s.prog.AttachPerfEvent(fd); err != nil {
			return err
		}
	}
	return nil
}

func (s *Session) attachProgs() error {
	progIter := s.module.Iterator()
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

func (s *Session) StartPingSlow() error {
	var err error
	if err = unix.Setrlimit(unix.RLIMIT_MEMLOCK, &unix.Rlimit{
		Cur: unix.RLIM_INFINITY,
		Max: unix.RLIM_INFINITY,
	}); err != nil {
		return err
	}

	s.modMutex.Lock()
	defer s.modMutex.Unlock()
	s.lastTaskInfos = make(map[TaskStack]TaskCounter)
	if s.symCache, err = newSymbolCache(s.symbolCacheSize); err != nil {
		return err
	}

	args := bpf.NewModuleArgs{BPFObjBuff: pingSlowBpf}
	//BTFObjPath: btf} panic
	if s.module, err = bpf.NewModuleFromBufferArgs(args); err != nil {
		return err
	}
	if err = s.module.BPFLoadObject(); err != nil {
		return err
	}
	if s.prog, err = s.module.GetProgram("do_perf_event"); err != nil {
		return err
	}
	if err = s.findMaps(); err != nil {
		return err
	}
	if err = s.initNetArgs(); err != nil {
		return err
	}
	if err = s.attachPerfEvent(); err != nil {
		return err
	}
	return nil
}

func (s *Session) StartUserSlow() error {
	var err error
	if err = unix.Setrlimit(unix.RLIMIT_MEMLOCK, &unix.Rlimit{
		Cur: unix.RLIM_INFINITY,
		Max: unix.RLIM_INFINITY,
	}); err != nil {
		return err
	}

	s.modMutex.Lock()
	defer s.modMutex.Unlock()
	s.lastTaskInfos = make(map[TaskStack]TaskCounter)
	if s.symCache, err = newSymbolCache(s.symbolCacheSize); err != nil {
		return err
	}

	// s.logger.Debugf("tao offcpuBpf:%d\n", len(offCpuBpf))
	args := bpf.NewModuleArgs{BPFObjBuff: userSlow}
	//BTFObjPath: btf} panic
	if s.module, err = bpf.NewModuleFromBufferArgs(args); err != nil {
		return err
	}
	if err = s.module.BPFLoadObject(); err != nil {
		return err
	}

	if s.prog, err = s.module.GetProgram("tcp_probe_hook"); err != nil {
		s.logger.Errorf("Ebpf tcp_probe_hook attach failed:%v\n", err)
	}

	if _, err := s.prog.AttachGeneric(); err != nil {
		s.logger.Errorf("Ebpf userslow attach failed:%v\n", err)
		return err
	}

	if s.prog, err = s.module.GetProgram("tcp_space_adjust_hook"); err != nil {
		s.logger.Errorf("Ebpf tcp_space_adjust_hook attach failed:%v\n", err)
	}

	if _, err := s.prog.AttachGeneric(); err != nil {
		s.logger.Errorf("Ebpf userslow attach failed:%v\n", err)
		return err
	}

	if s.prog, err = s.module.GetProgram("do_perf_event"); err != nil {
		return err
	}

	if err = s.findMaps(); err != nil {
		return err
	}

	if err = s.initNetArgs(); err != nil {
		return err
	}

	if err = s.attachPerfEvent(); err != nil {
		return err
	}

	return nil
}

func (s *Session) StartOffCpu() error {
	var err error
	if err = unix.Setrlimit(unix.RLIMIT_MEMLOCK, &unix.Rlimit{
		Cur: unix.RLIM_INFINITY,
		Max: unix.RLIM_INFINITY,
	}); err != nil {
		return err
	}

	s.modMutex.Lock()
	defer s.modMutex.Unlock()
	s.lastTaskInfos = make(map[TaskStack]TaskCounter)
	if s.symCache, err = newSymbolCache(s.symbolCacheSize); err != nil {
		return err
	}

	// s.logger.Debugf("tao offcpuBpf:%d\n", len(offCpuBpf))
	args := bpf.NewModuleArgs{BPFObjBuff: offCpuBpf}
	//BTFObjPath: btf} panic
	if s.module, err = bpf.NewModuleFromBufferArgs(args); err != nil {
		return err
	}
	if err = s.module.BPFLoadObject(); err != nil {
		return err
	}
	if s.prog, err = s.module.GetProgram("sched_switch_fn"); err != nil {
		return err
	}

	if _, err := s.prog.AttachGeneric(); err != nil {
		s.logger.Errorf("Ebpf offcpu attach failed:%v\n", err)
		return err
	}

	if err = s.findMaps(); err != nil {
		return err
	}
	if err = s.initArgs(); err != nil {
		return err
	}
	return nil
}

func (s *Session) ResetOffCpu(cb func(labels *spy.Labels, name []byte, value uint64, pid uint32) error) error {
	s.logger.Debugf("ebpf offcpu session reset")
	s.modMutex.Lock()
	defer s.modMutex.Unlock()

	s.roundNumber += 1

	refreshResult := make(chan error)
	go func() {
		refreshResult <- s.serviceDiscovery.Refresh(context.TODO())
	}()

	keys, values, batch, err := s.getCountsMapValues()
	if err != nil {
		return err
	}

	err = <-refreshResult
	if err != nil {
		return err
	}

	type sf struct {
		pid    uint32
		count  uint64
		delta  uint64
		kStack []byte
		uStack []byte
		comm   string
		labels *spy.Labels
	}
	var sfs []sf
	knownStacks := map[uint32]bool{}
	var taskStack TaskStack
	var taskCounter TaskCounter
	var buf bytes.Buffer
	for i, key := range keys {
		ck := (*C.struct_profile_key_t)(unsafe.Pointer(&key[0]))
		value := values[i]

		pid := uint32(ck.pid)
		kStackID := int64(ck.kern_stack)
		uStackID := int64(ck.user_stack)
		//count := binary.LittleEndian.Uint64(value)
		//delta := binary.LittleEndian.Uint64(value)
		val := (*C.struct_profile_value_t)(unsafe.Pointer(&value[0]))
		count := uint64(val.counts)
		delta := uint64(val.deltas)
		//s.logger.Debugf("tao pid:%d, count:%d, time delta:%d\n", pid, count, delta)
		var comm string = C.GoString(&ck.comm[0])
		if uStackID >= 0 {
			knownStacks[uint32(uStackID)] = true
		}
		if kStackID >= 0 {
			knownStacks[uint32(kStackID)] = true
		}
		labels := s.serviceDiscovery.GetLabels(pid)
		if labels == nil && s.onlyServices {
			continue
		}
		buf.WriteString(comm)
		buf.WriteString("/")
		buf.WriteString(strconv.Itoa(int(pid)))
		uStack := s.getStack(uStackID)
		kStack := s.getStack(kStackID)

		taskCounter.counts = count
		taskCounter.delta = delta
		taskStack.pid = pid
		taskStack.kern_stack = kStackID
		taskStack.user_stack = uStackID
		lastCounter := s.lastTaskInfos[taskStack]
		if lastCounter.counts > 0 && lastCounter.delta > 0 {
			count -= lastCounter.counts // calculate the delta of sched_switch counts in this period
			delta -= lastCounter.delta
		}

		s.lastTaskInfos[taskStack] = taskCounter

		if count <= 0 {
			continue
		}

		delta = delta / 1000000
		sfs = append(sfs, sf{pid: pid, uStack: uStack, kStack: kStack, count: count, delta: delta, comm: buf.String(), labels: labels})
	}
	for _, it := range sfs {
		buf := bytes.NewBuffer(nil)
		buf.Write([]byte(it.comm))
		buf.Write([]byte{';'})
		s.walkStack(buf, it.uStack, it.pid, true)
		s.walkStack(buf, it.kStack, 0, false)
		err = cb(it.labels, buf.Bytes(), uint64(it.delta), it.pid)
		if err != nil {
			return err
		}
	}
	if err = s.clearCountsMap(keys, batch); err != nil {
		return err
	}
	if err = s.clearStacksMap(knownStacks); err != nil {
		return err
	}
	return nil
}

func (s *Session) ResetUserSlow(cb func(labels *spy.Labels, name []byte, value uint64, pid uint32) error) error {
	s.logger.Debugf("tao ebpf session reset")
	s.modMutex.Lock()
	defer s.modMutex.Unlock()

	s.roundNumber += 1

	refreshResult := make(chan error)
	go func() {
		refreshResult <- s.serviceDiscovery.Refresh(context.TODO())
	}()

	keys, values, batch, err := s.getCountsMapValues()
	if err != nil {
		return err
	}

	err = <-refreshResult
	if err != nil {
		return err
	}

	type sf struct {
		pid    uint32
		count  uint32
		kStack []byte
		uStack []byte
		comm   string
		labels *spy.Labels
	}
	var sfs []sf
	knownStacks := map[uint32]bool{}
	for i, key := range keys {
		ck := (*C.struct_profile_key_t)(unsafe.Pointer(&key[0]))
		value := values[i]

		pid := uint32(ck.pid)
		kStackID := int64(ck.kern_stack)
		uStackID := int64(ck.user_stack)
		count := binary.LittleEndian.Uint32(value)
		var comm string = C.GoString(&ck.comm[0])
		if uStackID >= 0 {
			knownStacks[uint32(uStackID)] = true
		}
		if kStackID >= 0 {
			knownStacks[uint32(kStackID)] = true
		}
		// 这里的labels是k8s的标签
		labels := s.serviceDiscovery.GetLabels(pid)
		if labels == nil && s.onlyServices {
			continue
		}
		uStack := s.getStack(uStackID)
		kStack := s.getStack(kStackID)
		sfs = append(sfs, sf{pid: pid, uStack: uStack, kStack: kStack, count: count, comm: comm, labels: labels})
	}
	for _, it := range sfs {
		buf := bytes.NewBuffer(nil)
		buf.Write([]byte(it.comm))
		buf.Write([]byte{';'})
		s.walkStack(buf, it.uStack, it.pid, true)
		s.walkStack(buf, it.kStack, 0, false)
		err = cb(it.labels, buf.Bytes(), uint64(it.count), it.pid)
		if err != nil {
			return err
		}
	}
	if err = s.clearCountsMap(keys, batch); err != nil {
		return err
	}
	if err = s.clearStacksMap(knownStacks); err != nil {
		return err
	}
	return nil
}

func (s *Session) ResetPingSlow(cb func(labels *spy.Labels, name []byte, value uint64, pid uint32) error) error {
	s.logger.Debugf("tao ebpf pingslow session reset")
	s.modMutex.Lock()
	defer s.modMutex.Unlock()

	s.roundNumber += 1

	refreshResult := make(chan error)
	go func() {
		refreshResult <- s.serviceDiscovery.Refresh(context.TODO())
	}()

	keys, values, batch, err := s.getCountsMapValues()
	if err != nil {
		return err
	}

	err = <-refreshResult
	if err != nil {
		return err
	}

	type sf struct {
		pid    uint32
		count  uint32
		kStack []byte
		uStack []byte
		comm   string
		labels *spy.Labels
	}
	var sfs []sf
	knownStacks := map[uint32]bool{}
	for i, key := range keys {
		ck := (*C.struct_profile_key_t)(unsafe.Pointer(&key[0]))
		value := values[i]

		pid := uint32(ck.pid)
		kStackID := int64(ck.kern_stack)
		uStackID := int64(ck.user_stack)
		count := binary.LittleEndian.Uint32(value)
		var comm string = C.GoString(&ck.comm[0])
		if uStackID >= 0 {
			knownStacks[uint32(uStackID)] = true
		}
		if kStackID >= 0 {
			knownStacks[uint32(kStackID)] = true
		}
		labels := s.serviceDiscovery.GetLabels(pid)
		if labels == nil && s.onlyServices {
			continue
		}
		uStack := s.getStack(uStackID)
		kStack := s.getStack(kStackID)
		sfs = append(sfs, sf{pid: pid, uStack: uStack, kStack: kStack, count: count, comm: comm, labels: labels})
	}
	for _, it := range sfs {
		buf := bytes.NewBuffer(nil)
		buf.Write([]byte(it.comm))
		buf.Write([]byte{';'})
		s.walkStack(buf, it.uStack, it.pid, true)
		s.walkStack(buf, it.kStack, 0, false)
		err = cb(it.labels, buf.Bytes(), uint64(it.count), it.pid)
		if err != nil {
			return err
		}
	}
	if err = s.clearCountsMap(keys, batch); err != nil {
		return err
	}
	if err = s.clearStacksMap(knownStacks); err != nil {
		return err
	}
	return nil
}

func (s *Session) getStack(stackId int64) []byte {
	if stackId < 0 {
		return nil
	}
	stackIdU32 := uint32(stackId)
	key := unsafe.Pointer(&stackIdU32)
	stack, err := s.mapStacks.GetValue(key)
	if err != nil {
		return nil
	}
	return stack

}
func (s *Session) walkStack(line *bytes.Buffer, stack []byte, pid uint32, userspace bool) {
	if len(stack) == 0 {
		return
	}
	var stackFrames []string
	for i := 0; i < 127; i++ {
		it := stack[i*8 : i*8+8]
		ip := binary.LittleEndian.Uint64(it)
		if ip == 0 {
			break
		}
		// tt pid != 0 user stack , pid == 0 kernel stack
		sym := s.symCache.bccResolve(pid, ip, s.roundNumber)
		if !userspace && sym.Name == "" {
			continue
		}
		name := sym.Name
		if sym.Name == "" {
			if sym.Module != "" {
				name = fmt.Sprintf("%s+0x%x", sym.Module, sym.Offset)
			} else {
				name = "[unknown]"
			}
		}
		//fmt.Printf("==%x, name:%s\n", ip, name)
		stackFrames = append(stackFrames, name+";")
	}
	reverse(stackFrames)
	for _, s := range stackFrames {
		line.Write([]byte(s))
	}
}

func reverse(s []string) {
	for i, j := 0, len(s)-1; i < j; i, j = i+1, j-1 {
		s[i], s[j] = s[j], s[i]
	}
}

//go:embed bpf/oncpu.bpf.o
var onCpuBpf []byte

//go:embed bpf/offcpu.bpf.o
var offCpuBpf []byte

//go:embed bpf/user_slow.bpf.o
var userSlow []byte

//go:embed bpf/ping_slow.bpf.o
var pingSlowBpf []byte

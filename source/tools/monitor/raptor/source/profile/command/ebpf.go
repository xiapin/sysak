package command

import (
	"github.com/chentao-kernel/cloud_ebpf/profile/agent/ebpf"
	"github.com/fatih/color"
	"github.com/sirupsen/logrus"
	"github.com/spf13/cobra"

	"context"
	"errors"
	"fmt"
	"os"
	"os/signal"
	"os/user"
	"syscall"
	"time"

	"github.com/chentao-kernel/cloud_ebpf/profile/config"
	sd "github.com/chentao-kernel/cloud_ebpf/profile/sd"
	"github.com/pyroscope-io/pyroscope/pkg/cli"

	"github.com/chentao-kernel/cloud_ebpf/profile/agent"
	"github.com/chentao-kernel/cloud_ebpf/profile/spy"
	"github.com/chentao-kernel/cloud_ebpf/profile/upstream"
	"github.com/chentao-kernel/cloud_ebpf/profile/upstream/direct"
	"github.com/chentao-kernel/cloud_ebpf/profile/upstream/remote"
	"github.com/chentao-kernel/cloud_ebpf/sls"
	"github.com/pyroscope-io/pyroscope/pkg/agent/types"
	"github.com/pyroscope-io/pyroscope/pkg/util/process"
)

type TRACETYPE int32

const (
	TraceONCPU  TRACETYPE = 0
	TraceOFFCPU TRACETYPE = 1
)

func NewContextWithExitTime(exitTime int) (context.Context, context.CancelFunc) {
	return context.WithTimeout(context.Background(), time.Duration(exitTime)*time.Minute)
}

func SetExitTime(exitTime int, up upstream.Upstream, cfg *config.CPU, log *logrus.Logger) {
	log.Debug("After %d minutes profiling exit\n", exitTime)
	ctx, cancel := NewContextWithExitTime(exitTime)
	defer cancel()
	handleExit(ctx, up, cfg)
}

func handleExit(ctx context.Context, up upstream.Upstream, cfg *config.CPU) {
	for i := 0; i < 10000000; i++ {
		time.Sleep(time.Minute * 1)
		select {
		case <-ctx.Done():
			if cfg.Server == "local" && cfg != nil {
				up.DumpMetaData()
			}
			os.Exit(0)
		default:
		}
	}
}

func timerTrigger(cfg *config.CPU, pfs *agent.ProfileSession, ch chan os.Signal) {
	ticker := time.NewTicker(time.Duration(cfg.Timer) * 60 * time.Second)
	defer ticker.Stop()
	for {
		select {
		case <-ch:
			return
		case <-ticker.C:
			if err := pfs.RestartSpies(); err != nil {
				fmt.Errorf("Restart spy failed:%v\n", err)
			}
			time.Sleep(10 * time.Second)
			pfs.StopSpies()
		}
	}
}

func cpuUsageTrigger(cfg *config.CPU, pfs *agent.ProfileSession, ch chan os.Signal) {
	cpu, err := agent.NewCPUCollector()
	if err != nil {
		fmt.Printf("Failed to new cpu collector:%v\n", err)
	}
	timeLast := time.Now().Unix()
	for {
		select {
		case <-ch:
			return
		}
		cpu.UpdateCpuStats()
		cpu.MertricCalculate()
		for _, cs := range cpu.CpuMetric {
			if int(cs.User) > cfg.Usage || int(cs.System) > cfg.Usage {
				timeCurrent := time.Now().Unix()
				if timeCurrent-timeLast < 5*60 {
					continue
				}
				timeLast = timeCurrent
				if err := pfs.RestartSpies(); err != nil {
					fmt.Errorf("Restart spy failed:%v\n", err)
				}
				time.Sleep(10 * time.Second)
				pfs.StopSpies()
				break
			}
		}
	}
}

// https://blog.csdn.net/xmcy001122/article/details/124616967 cobra库使用
func oncpuSpy(cfg *config.CPU) *cobra.Command {
	vpr := newViper()
	connectCmd := &cobra.Command{
		Use:   "oncpu [flags]",
		Short: "eBPF oncpu sampling profiler",
		Args:  cobra.NoArgs,

		RunE: cli.CreateCmdRunFn(cfg, vpr, func(_ *cobra.Command, _ []string) error {
			return RunONCPU(cfg)
		}),
	}

	cli.PopulateFlagSet(cfg, connectCmd.Flags(), vpr)
	return connectCmd
}

func newONCPUSpyCmd(cfg *config.CPU) *cobra.Command {
	return oncpuSpy(cfg)
}

func newOFFCPUSpyCmd(cfg *config.CPU) *cobra.Command {
	vpr := newViper()
	connectCmd := &cobra.Command{
		Use:   "offcpu [flags]",
		Short: "eBPF offcpu sampling profiler",
		Args:  cobra.NoArgs,

		RunE: cli.CreateCmdRunFn(cfg, vpr, func(_ *cobra.Command, _ []string) error {
			return RunOFFCPU(cfg)
		}),
	}

	cli.PopulateFlagSet(cfg, connectCmd.Flags(), vpr)
	return connectCmd
}

func newUSERSLOWSpyCmd(cfg *config.NET) *cobra.Command {
	vpr := newViper()
	connectCmd := &cobra.Command{
		Use:   "userslow [flags]",
		Short: "eBPF sampling user takes packets slow",
		Args:  cobra.NoArgs,

		RunE: cli.CreateCmdRunFn(cfg, vpr, func(_ *cobra.Command, _ []string) error {
			return RunUserSlow(cfg)
		}),
	}

	cli.PopulateFlagSet(cfg, connectCmd.Flags(), vpr)
	return connectCmd
}

func newPINGSLOWSpyCmd(cfg *config.NET) *cobra.Command {
	vpr := newViper()
	connectCmd := &cobra.Command{
		Use:   "pingslow [flags]",
		Short: "eBPF sampling ping slow",
		Args:  cobra.NoArgs,

		RunE: cli.CreateCmdRunFn(cfg, vpr, func(_ *cobra.Command, _ []string) error {
			return RunPingSlow(cfg)
		}),
	}

	cli.PopulateFlagSet(cfg, connectCmd.Flags(), vpr)
	return connectCmd
}

func RunOFFCPU(cfg *config.CPU) error {
	if !isRoot() {
		return errors.New("when using eBPF you're required to run the agent with sudo")
	}
	// todo: create a new command for sls consumer
	if err := sls.SLSInit(cfg.SLS, cfg.Endpoint, cfg.AKID, cfg.AKSE, cfg.Project, cfg.Logstore); err != nil {
		return err
	}
	logger := NewLogger(cfg.LogLevel, false /*cfg.NoLogging*/)
	var isSlsUpload bool
	if cfg.SLS == sls.SLSPRODUCER {
		isSlsUpload = true
	}
	rc := remote.RemoteConfig{
		AuthToken:              "", //cfg.AuthToken,
		UpstreamThreads:        cfg.UploadThreads,
		UpstreamAddress:        cfg.Server,
		UpstreamRequestTimeout: cfg.UploadTimeout,
		SlsUpload:              isSlsUpload,
	}
	up, err := remote.New(rc, logger)
	if err != nil {
		return fmt.Errorf("new remote upstream: %v", err)
	}

	// if the sample rate is zero, use the default value
	sampleRate := uint32(types.DefaultSampleRate)
	if cfg.SampleRate != 0 {
		sampleRate = uint32(cfg.SampleRate)
	}

	appName := CheckApplicationName(logger, cfg.AppName, spy.EBPF, []string{})

	var serviceDiscovery sd.ServiceDiscovery = sd.NoopServiceDiscovery{}
	/*
		if cfg.KubernetesNode != "" {
			serviceDiscovery, err = sd.NewK8ServiceDiscovery(context.TODO(), logger, cfg.KubernetesNode)
			if err != nil {
				return err
			}
		}
	*/

	logger.Debug("starting OFFCPU command")

	// The channel buffer capacity should be sufficient to be keep up with
	// the expected signal rate (in case of Exec all the signals to be relayed
	// to the child process)
	ch := make(chan os.Signal, 10)
	signal.Notify(ch, syscall.SIGINT, syscall.SIGTERM)
	defer func() {
		signal.Stop(ch)
		close(ch)
	}()

	sc := agent.SessionConfig{
		Upstream:       up,
		AppName:        appName,
		Tags:           cfg.Tags,
		ProfilingTypes: []spy.ProfileType{spy.ProfileOFFCPU},
		SpyName:        spy.EBPF,
		SampleRate:     sampleRate,
		//UploadRate:       10 * time.Second,
		UploadRate:       cfg.UploadRate,
		Pid:              cfg.Pid,
		Cpu:              cfg.Cpu,
		Space:            cfg.Space,
		WithSubprocesses: false,
		Logger:           logger,
	}

	var netArgs ebpf.NetArgs
	session, err := agent.NewSessionWithSpyFactory(sc, func(pid int) ([]spy.Spy, error) {
		s := ebpf.NewSession(netArgs, logger, sc.ProfilingTypes, cfg.Pid, cfg.Cpu, cfg.Space, sampleRate, cfg.SymbolCacheSize, serviceDiscovery, false)
		// ebpf session Start
		err := s.StartOffCpu()
		if err != nil {
			return nil, err
		}
		res := ebpf.NewEBPFSpy(s)
		return []spy.Spy{res}, nil
	})
	if err != nil {
		return fmt.Errorf("new session: %w", err)
	}

	if cfg.ExitTime != -1 {
		go SetExitTime(cfg.ExitTime, up, nil, logger)
	}
	// upstream Start
	up.Start()
	defer up.Stop()

	// profileSession start
	if err = session.Start(); err != nil {
		return fmt.Errorf("start session: %w", err)
	}
	defer session.Stop()

	// wait for process to exit
	// pid == -1 means we're profiling whole system
	if cfg.Pid == -1 {
		<-ch
		return nil
	}
	ticker := time.NewTicker(time.Second)
	defer ticker.Stop()
	for {
		select {
		case <-ch:
			return nil
		case <-ticker.C:
			if !process.Exists(cfg.Pid) {
				logger.Debugf("child process exited")
				return nil
			}
		}
	}
}

func RunONCPU(cfg *config.CPU) error {
	if !isRoot() {
		return errors.New("when using eBPF you're required to run the agent with sudo")
	}
	// todo: create a new command for sls consumer
	if err := sls.SLSInit(cfg.SLS, cfg.Endpoint, cfg.AKID, cfg.AKSE, cfg.Project, cfg.Logstore); err != nil {
		return err
	}

	logger := NewLogger(cfg.LogLevel, false /*cfg.NoLogging*/)

	var isSlsUpload bool
	if cfg.SLS == sls.SLSPRODUCER {
		isSlsUpload = true
	}
	rc := remote.RemoteConfig{
		AuthToken:              "", //cfg.AuthToken,
		UpstreamThreads:        cfg.UploadThreads,
		UpstreamAddress:        cfg.Server,
		UpstreamRequestTimeout: cfg.UploadTimeout,
		SlsUpload:              isSlsUpload,
	}
	var up upstream.Upstream
	if cfg.Server == "local" {
		up = direct.NewLocal()
	} else {
		var er error
		up, er = remote.New(rc, logger)
		if er != nil {
			return fmt.Errorf("new remote upstream: %v", er)
		}
	}

	// if the sample rate is zero, use the default value
	sampleRate := uint32(types.DefaultSampleRate)
	if cfg.SampleRate != 0 {
		sampleRate = uint32(cfg.SampleRate)
	}

	appName := CheckApplicationName(logger, cfg.AppName, spy.EBPF, []string{})

	var serviceDiscovery sd.ServiceDiscovery = sd.NoopServiceDiscovery{}
	/*
		if cfg.KubernetesNode != "" {
			serviceDiscovery, err = sd.NewK8ServiceDiscovery(context.TODO(), logger, cfg.KubernetesNode)
			if err != nil {
				return err
			}
		}
	*/

	logger.Debug("starting ONCPU command")

	// The channel buffer capacity should be sufficient to be keep up with
	// the expected signal rate (in case of Exec all the signals to be relayed
	// to the child process)
	ch := make(chan os.Signal, 10)
	signal.Notify(ch, syscall.SIGINT, syscall.SIGTERM)
	defer func() {
		signal.Stop(ch)
		close(ch)
	}()

	sc := agent.SessionConfig{
		Upstream:       up,
		AppName:        appName,
		Tags:           cfg.Tags,
		ProfilingTypes: []spy.ProfileType{spy.ProfileCPU},
		SpyName:        spy.EBPF,
		SampleRate:     sampleRate,
		// 上传时间
		//UploadRate:       10 * time.Second,
		UploadRate:       cfg.UploadRate,
		Pid:              cfg.Pid,
		Cpu:              cfg.Cpu,
		Space:            cfg.Space,
		WithSubprocesses: false,
		Logger:           logger,
	}

	// no use
	var netArgs ebpf.NetArgs
	session, err := agent.NewSessionWithSpyFactory(sc, func(pid int) ([]spy.Spy, error) {
		s := ebpf.NewSession(netArgs, logger, sc.ProfilingTypes, cfg.Pid, cfg.Cpu, cfg.Space, sampleRate, cfg.SymbolCacheSize, serviceDiscovery, false)
		// ebpf session 开始加载ebpf程序
		err := s.Start()
		if err != nil {
			return nil, err
		}

		res := ebpf.NewEBPFSpy(s)
		return []spy.Spy{res}, nil
	})
	if err != nil {
		return fmt.Errorf("new session: %w", err)
	}

	if cfg.ExitTime != -1 {
		go SetExitTime(cfg.ExitTime, up, cfg, logger)
	}
	// upstream Start
	up.Start()
	defer up.Stop()

	// profileSession start
	if err = session.Start(); err != nil {
		return fmt.Errorf("start session: %w", err)
	}
	defer session.Stop()

	if cfg.Usage != -1 {
		if err := session.StopSpies(); err != nil {
			logger.Errorf("Stop spy faield: %v\n", err)
		}
		cpuUsageTrigger(cfg, session, ch)
	}
	if cfg.Timer != -1 {
		if err := session.StopSpies(); err != nil {
			logger.Errorf("Stop spy failed: %v\n", err)
		}
		timerTrigger(cfg, session, ch)
	}
	// wait for process to exit
	// pid == -1 means we're profiling whole system
	if cfg.Pid == -1 {
		<-ch
		return nil
	}

	ticker := time.NewTicker(time.Second)
	defer ticker.Stop()
	for {
		select {
		case <-ch:
			return nil
		case <-ticker.C:
			if !process.Exists(cfg.Pid) {
				logger.Debugf("child process exited")
				return nil
			}
		}
	}
}

func RunPingSlow(cfg *config.NET) error {
	if !isRoot() {
		return errors.New("when using eBPF you're required to run the agent with sudo")
	}
	// todo: create a new command for sls consumer
	if err := sls.SLSInit(cfg.SLS, cfg.Endpoint, cfg.AKID, cfg.AKSE, cfg.Project, cfg.Logstore); err != nil {
		return err
	}
	logger := NewLogger(cfg.LogLevel, false /*cfg.NoLogging*/)

	var isSlsUpload bool
	if cfg.SLS == sls.SLSPRODUCER {
		isSlsUpload = true
	}
	rc := remote.RemoteConfig{
		AuthToken:              "", //cfg.AuthToken,
		UpstreamThreads:        cfg.UploadThreads,
		UpstreamAddress:        cfg.Server,
		UpstreamRequestTimeout: cfg.UploadTimeout,
		SlsUpload:              isSlsUpload,
	}
	up, err := remote.New(rc, logger)
	if err != nil {
		return fmt.Errorf("new remote upstream: %v", err)
	}

	// if the sample rate is zero, use the default value
	sampleRate := uint32(types.DefaultSampleRate)
	if cfg.SampleRate != 0 {
		sampleRate = uint32(cfg.SampleRate)
	}

	appName := CheckApplicationName(logger, cfg.AppName, spy.EBPF, []string{})

	var serviceDiscovery sd.ServiceDiscovery = sd.NoopServiceDiscovery{}

	logger.Debug("starting PingSlow command")

	ch := make(chan os.Signal, 10)
	signal.Notify(ch, syscall.SIGINT, syscall.SIGTERM)
	defer func() {
		signal.Stop(ch)
		close(ch)
	}()

	sc := agent.SessionConfig{
		Upstream:       up,
		AppName:        appName,
		Tags:           cfg.Tags,
		ProfilingTypes: []spy.ProfileType{spy.ProfilePingSlow},
		SpyName:        spy.EBPF,
		SampleRate:     sampleRate,
		//UploadRate:       10 * time.Second,
		UploadRate:       cfg.UploadRate,
		Pid:              cfg.Pid,
		Cpu:              cfg.Cpu,
		WithSubprocesses: false,
		Logger:           logger,
	}
	var netArgs ebpf.NetArgs
	netArgs.Dport = cfg.Dport
	netArgs.Sport = cfg.Sport
	netArgs.Delay = cfg.Delay
	// session 没有使用
	session, err := agent.NewSessionWithSpyFactory(sc, func(pid int) ([]spy.Spy, error) {
		s := ebpf.NewSession(netArgs, logger, sc.ProfilingTypes, cfg.Pid, cfg.Cpu, 0, sampleRate, cfg.SymbolCacheSize, serviceDiscovery, false)
		// ebpf session Start
		err := s.StartPingSlow()
		if err != nil {
			return nil, err
		}
		res := ebpf.NewEBPFSpy(s)
		return []spy.Spy{res}, nil
	})
	if err != nil {
		return fmt.Errorf("new session: %w", err)
	}

	if cfg.ExitTime != -1 {
		go SetExitTime(cfg.ExitTime, up, nil, logger)
	}
	// upstream Start
	up.Start()
	defer up.Stop()

	// profileSession start
	if err = session.Start(); err != nil {
		return fmt.Errorf("start session: %w", err)
	}
	defer session.Stop()

	// wait for process to exit
	// pid == -1 means we're profiling whole system
	if cfg.Pid == -1 {
		<-ch
		return nil
	}
	ticker := time.NewTicker(time.Second)
	defer ticker.Stop()
	for {
		select {
		case <-ch:
			return nil
		case <-ticker.C:
			if !process.Exists(cfg.Pid) {
				logger.Debugf("child process exited")
				return nil
			}
		}
	}
}

func RunUserSlow(cfg *config.NET) error {
	if !isRoot() {
		return errors.New("when using eBPF you're required to run the agent with sudo")
	}
	// todo: create a new command for sls consumer
	if err := sls.SLSInit(cfg.SLS, cfg.Endpoint, cfg.AKID, cfg.AKSE, cfg.Project, cfg.Logstore); err != nil {
		return err
	}
	logger := NewLogger(cfg.LogLevel, false /*cfg.NoLogging*/)
	var isSlsUpload bool
	if cfg.SLS == sls.SLSPRODUCER {
		isSlsUpload = true
	}
	rc := remote.RemoteConfig{
		AuthToken:              "", //cfg.AuthToken,
		UpstreamThreads:        cfg.UploadThreads,
		UpstreamAddress:        cfg.Server,
		UpstreamRequestTimeout: cfg.UploadTimeout,
		SlsUpload:              isSlsUpload,
	}
	up, err := remote.New(rc, logger)
	if err != nil {
		return fmt.Errorf("new remote upstream: %v", err)
	}

	// if the sample rate is zero, use the default value
	sampleRate := uint32(types.DefaultSampleRate)
	if cfg.SampleRate != 0 {
		sampleRate = uint32(cfg.SampleRate)
	}

	appName := CheckApplicationName(logger, cfg.AppName, spy.EBPF, []string{})

	var serviceDiscovery sd.ServiceDiscovery = sd.NoopServiceDiscovery{}

	logger.Debug("starting UserSlow command")

	ch := make(chan os.Signal, 10)
	signal.Notify(ch, syscall.SIGINT, syscall.SIGTERM)
	defer func() {
		signal.Stop(ch)
		close(ch)
	}()

	sc := agent.SessionConfig{
		Upstream:       up,
		AppName:        appName,
		Tags:           cfg.Tags,
		ProfilingTypes: []spy.ProfileType{spy.ProfileUserSlow},
		SpyName:        spy.EBPF,
		SampleRate:     sampleRate,
		//UploadRate:       10 * time.Second,
		UploadRate:       cfg.UploadRate,
		Pid:              cfg.Pid,
		Cpu:              cfg.Cpu,
		WithSubprocesses: false,
		Logger:           logger,
	}
	var netArgs ebpf.NetArgs
	netArgs.Dport = cfg.Dport
	netArgs.Sport = cfg.Sport
	netArgs.Delay = cfg.Delay
	// session 没有使用
	session, err := agent.NewSessionWithSpyFactory(sc, func(pid int) ([]spy.Spy, error) {
		s := ebpf.NewSession(netArgs, logger, sc.ProfilingTypes, cfg.Pid, cfg.Cpu, 0, sampleRate, cfg.SymbolCacheSize, serviceDiscovery, false)
		// ebpf session Start
		err := s.StartUserSlow()
		if err != nil {
			return nil, err
		}
		res := ebpf.NewEBPFSpy(s)
		return []spy.Spy{res}, nil
	})
	if err != nil {
		return fmt.Errorf("new session: %w", err)
	}

	if cfg.ExitTime != -1 {
		go SetExitTime(cfg.ExitTime, up, nil, logger)
	}
	// upstream Start
	up.Start()
	defer up.Stop()

	// profileSession start
	if err = session.Start(); err != nil {
		return fmt.Errorf("start session: %w", err)
	}
	defer session.Stop()

	// wait for process to exit
	// pid == -1 means we're profiling whole system
	if cfg.Pid == -1 {
		<-ch
		return nil
	}
	ticker := time.NewTicker(time.Second)
	defer ticker.Stop()
	for {
		select {
		case <-ch:
			return nil
		case <-ticker.C:
			if !process.Exists(cfg.Pid) {
				logger.Debugf("child process exited")
				return nil
			}
		}
	}
}

func isRoot() bool {
	u, err := user.Current()
	return err == nil && u.Username == "root"
}

func CheckApplicationName(logger *logrus.Logger, applicationName string, spyName string, args []string) string {
	if applicationName == "" {
		applicationName = spyName + "." + "ebpf"
		logger.Infof("app name is null, we chose the name for you and it's \"%s\"", color.GreenString(applicationName))
	}
	return applicationName
}

func NewLogger(logLevel string, noLogging bool) *logrus.Logger {
	level := logrus.PanicLevel
	if l, err := logrus.ParseLevel(logLevel); err == nil && !noLogging {
		level = l
	}
	logger := logrus.StandardLogger()
	logger.SetLevel(level)
	return logger
}

package config

import (
	"time"
)

type Config struct {
	Version bool `mapstructure:"version"`

	ONCPU    CPU `skip:"true" mapstructure:",squash"`
	OFFCPU   CPU `skip:"true" mapstructure:",squash"`
	USERSLOW NET `skip:"true" mapstructure:",squash"`
	PINGSLOW NET `skip:"true" mapstructure:",squash"`
}

type CPU struct {
	LogLevel string `def:"info" desc:"log level: debug|info|warn|error" mapstructure:"log-level"`
	//NoLogging bool   `def:"false" desc:"disables logging" mapstructure:"no-logging"`

	// Spy configuration
	AppName    string `def:"" desc:"application name used when uploading profiling data" mapstructure:"app-name"`
	SampleRate uint   `def:"100" desc:"sample rate for the profiler in Hz. 100 means reading 100 times per second" mapstructure:"sample-rate"`

	// Remote upstream configuration
	Server string `def:"http://localhost:4040" desc:"the server address" mapstructure:"server"`
	//AuthToken              string        `def:"" desc:"authorization token used to upload profiling data" mapstructure:"auth-token"`
	UploadThreads int           `def:"4" desc:"number of upload threads" mapstructure:"upload-threads"`
	UploadTimeout time.Duration `def:"10s" desc:"profile upload timeout" mapstructure:"upload-timeout"`
	UploadRate    time.Duration `def:"10s" desc:"profile upload rate " mapstructure:"upload-rate"`

	Tags     map[string]string `name:"tag" def:"" desc:"tag in key=value form. The flag may be specified multiple times" mapstructure:"tags"`
	Pid      int               `def:"-1" desc:"PID you want to profile. -1 to profile the whole system" mapstructure:"pid"`
	Cpu      int               `def:"-1" desc:"Number of cpu you want to profile. -1 to profile the whole system" mapstructure:"cpu"`
	Space    int               `def:"0" desc:"0 profile user space, 1 user+kernel space" mapstructure:"space"`
	ExitTime int               `def:"2880" desc:"time of minutes unit the profiling to exit, default  means 2 days" mapstructure:"exitTime"`
	//DetectSubprocesses bool   `def:"false" desc:"makes keep track of and profile subprocesses of the main process" mapstructure:"detect-subprocesses"`
	SymbolCacheSize int    `def:"256" desc:"max size of symbols cache" mapstructure:"symbol-cache-size"`
	Usage           int    `def:"-1" desc:"Cpu usage trigger to start the profiling" mapstructure:"usage"`
	Timer           int    `def:"-1" desc:"Timer(min) trigger to start the profiling, minimum timer every 5 minutes" mapstructure:"timer"`
	SLS             string `def:"unuser" desc:"producer/procuderaw/consumer data to/from SLS" mapstructure:"sls"`
	Endpoint        string `def:"endpoint" desc:"SLS Endpoint" mapstructure:"endpoint"`
	AKID            string `def:"akid" desc:"SLS AccessKeyID" mapstructure:"akid"`
	AKSE            string `def:"akse" desc:"SLS AccessKeySecret" mapstructure:"akse"`
	Project         string `def:"akid" desc:"SLS Project" mapstructure:"project"`
	Logstore        string `def:"akid" desc:"SLS Logstore" mapstructure:"logstore"`
	//KubernetesNode     string `def:"" desc:"Set to current k8s Node.nodeName for service discovery and labeling" mapstructure:"kubernetes-node"`
	//OnlyServices       bool   `def:"false" desc:"Ignore processes unknown to service discovery" mapstructure:"only-services"`
}

type NET struct {
	LogLevel string `def:"info" desc:"log level: debug|info|warn|error" mapstructure:"log-level"`

	// Spy configuration
	AppName    string `def:"" desc:"application name used when uploading profiling data" mapstructure:"app-name"`
	SampleRate uint   `def:"100" desc:"sample rate for the profiler in Hz. 100 means reading 100 times per second" mapstructure:"sample-rate"`

	// Remote upstream configuration
	Server        string        `def:"http://localhost:4040" desc:"the server address" mapstructure:"server"`
	UploadThreads int           `def:"4" desc:"number of upload threads" mapstructure:"upload-threads"`
	UploadTimeout time.Duration `def:"10s" desc:"profile upload timeout" mapstructure:"upload-timeout"`
	UploadRate    time.Duration `def:"10s" desc:"profile upload rate " mapstructure:"upload-rate"`

	Tags            map[string]string `name:"tag" def:"" desc:"tag in key=value form. The flag may be specified multiple times" mapstructure:"tags"`
	Pid             int               `def:"-1" desc:"PID you want to profile. -1 to profile the whole system" mapstructure:"pid"`
	Cpu             int               `def:"-1" desc:"Number of cpu you want to profile. -1 to profile the whole system" mapstructure:"cpu"`
	Dport           int               `def:"-1" desc:"Dport you want profile." mapstructure:"dport"`
	Sport           int               `def:"-1" desc:"Sport you want profile." mapstructure:"sport"`
	Delay           int               `def:"100" desc:"User take packet delay(ms)." mapstructure:"delay"`
	ExitTime        int               `def:"2" desc:"time of days the profiling to exit, default 2 days" mapstructure:"exitTime"`
	SymbolCacheSize int               `def:"256" desc:"max size of symbols cache" mapstructure:"symbol-cache-size"`
	SLS             string            `def:"unuser" desc:"producer/consumer data to/from SLS" mapstructure:"sls"`
	Endpoint        string            `def:"endpoint" desc:"SLS Endpoint" mapstructure:"endpoint"`
	AKID            string            `def:"akid" desc:"SLS AccessKeyID" mapstructure:"akid"`
	AKSE            string            `def:"akse" desc:"SLS AccessKeySecret" mapstructure:"akse"`
	Project         string            `def:"akid" desc:"SLS Project" mapstructure:"project"`
	Logstore        string            `def:"logstore" desc:"SLS Logstore" mapstructure:"logstore"`
}

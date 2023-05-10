package sls

import (
	"bytes"
	"encoding/json"
	"strconv"
	"strings"
	"time"

	Sls "github.com/aliyun/aliyun-log-go-sdk"
	"github.com/cespare/xxhash"
	"github.com/chentao-kernel/cloud_ebpf/profile/upstream"
	"github.com/gofrs/uuid"
	"github.com/pyroscope-io/pyroscope/pkg/structs/transporttrie"
	"google.golang.org/protobuf/proto"
)

type Encoder struct {
	RawData []byte
	Format  string
	Logs    *Sls.Log
	meta    Meta
}

func NewEncoder(data []byte, format string) *Encoder {
	return &Encoder{
		RawData: data,
		Format:  format,
	}
}

func getProfileID(meta *Meta) string {
	profileID, _ := uuid.NewV4()
	return profileID.String()
}

func (e *Encoder) extractCallBack(k []byte, v int) {
	profileID := getProfileID(&e.meta)
	name, stack := formatNameAndStacks(k, e.meta.SpyName)
	labels, _ := json.Marshal(e.meta.Tags)
	stackID := strconv.FormatUint(xxhash.Sum64(k), 16)
	var content []*Sls.LogContent
	content = append(content,
		&Sls.LogContent{
			Key:   proto.String("name"),
			Value: proto.String(name),
		},
		&Sls.LogContent{
			Key:   proto.String("stack"),
			Value: proto.String(strings.Join(stack, "\n")),
		},
		&Sls.LogContent{
			Key:   proto.String("stackID"),
			Value: proto.String(stackID),
		},
		&Sls.LogContent{
			Key:   proto.String("language"),
			Value: proto.String(e.meta.SpyName),
		},
		&Sls.LogContent{
			Key:   proto.String("type"),
			Value: proto.String(strconv.Itoa((int(DetectProfileType(e.meta.Units.DetectValueType()))))),
		},
		&Sls.LogContent{
			Key:   proto.String("units"),
			Value: proto.String(string(e.meta.Units)),
		},
		&Sls.LogContent{
			Key:   proto.String("valueTypes"),
			Value: proto.String(e.meta.Units.DetectValueType()),
		},
		&Sls.LogContent{
			Key:   proto.String("aggTypes"),
			Value: proto.String(string(e.meta.AggregationType)),
		},
		&Sls.LogContent{
			Key:   proto.String("dataType"),
			Value: proto.String("CallStack"),
		},
		&Sls.LogContent{
			Key:   proto.String("durationNs"),
			Value: proto.String(strconv.FormatInt(e.meta.EndTime.Sub(e.meta.StartTime).Nanoseconds(), 10)),
		},
		&Sls.LogContent{
			Key:   proto.String("profileID"),
			Value: proto.String(profileID),
		},
		&Sls.LogContent{
			Key:   proto.String("labels"),
			Value: proto.String(string(labels)),
		},
		&Sls.LogContent{
			Key:   proto.String("val"),
			Value: proto.String(strconv.FormatFloat(float64(v), 'f', 2, 64)),
		},
	)

	e.Logs = &Sls.Log{
		Time:     proto.Uint32(uint32(e.meta.StartTime.Unix())),
		Contents: content,
	}
}

func (e *Encoder) Encode(job *upstream.UploadJob, tags map[string]string) error {
	e.meta.StartTime = job.StartTime
	e.meta.EndTime = job.EndTime
	e.meta.Tags = tags
	e.meta.SpyName = job.SpyName
	e.meta.SampleRate = job.SampleRate
	e.meta.Units = Units(job.Units)
	e.meta.AggregationType = AggType(job.AggregationType)
	// stack info
	r := bytes.NewReader(job.Trie.Bytes())
	err := transporttrie.IterateRaw(r, make([]byte, 0, 256), e.extractCallBack)
	if err != nil {
		return err
	}
	return nil
}

/*
func formatNameAndStacks(k []byte, spyName string) (name string, stack []string) {
	slice := strings.Split(string(k), ";")
	if len(slice) > 0 && slice[len(slice)-1] == "" {
		slice = slice[:len(slice)-1]
	}
	if len(slice) == 1 {
		return profile.FormatPositionAndName(slice[len(slice)-1], profile.FormatType(spyName)), []string{}
	}
	name = profile.FormatPositionAndName(slice[len(slice)-1], profile.FormatType(spyName))
	slice = profile.FormatPostionAndNames(slice[:len(slice)-1], profile.FormatType(spyName))
	reverseStringSlice(slice)
	return name, slice
}

func reverseStringSlice(s []string) {
	for i, j := 0, len(s)-1; i < j; i, j = i+1, j-1 {
		s[i], s[j] = s[j], s[i]
	}
}
*/

func formatNameAndStacks(k []byte, spyName string) (name string, stack []string) {
	slice := strings.Split(string(k), ";")
	if len(slice) > 0 && slice[len(slice)-1] == "" {
		slice = slice[:len(slice)-1]
	}
	if len(slice) == 1 {
		return FormatPositionAndName(slice[len(slice)-1], FormatType(spyName)), []string{}
	}
	name = FormatPositionAndName(slice[len(slice)-1], FormatType(spyName))
	slice = FormatPostionAndNames(slice[:len(slice)-1], FormatType(spyName))
	reverseStringSlice(slice)
	return name, slice
}

func reverseStringSlice(s []string) {
	for i, j := 0, len(s)-1; i < j; i, j = i+1, j-1 {
		s[i], s[j] = s[j], s[i]
	}
}

type FormatType string

type Meta struct {
	StartTime       time.Time
	EndTime         time.Time
	Tags            map[string]string
	SpyName         string
	SampleRate      uint32
	Units           Units
	AggregationType AggType
}

type Kind int

const (
	_ Kind = iota
	CPUKind
	MemKind
	MutexKind
	GoRoutinesKind
	ExceptionKind
	UnknownKind
)

type AggType string

const (
	AvgAggType AggType = "avg"
	SumAggType AggType = "sum"
)

type Units string

var sequenceMapping = map[FormatType]SequenceType{
	PyroscopeNodeJs: FunctionFirst,
	PyroscopeGolang: FunctionFirst,
	PyroscopeRust:   PosFirst,
	PyroscopeDotnet: FunctionFirst,
	PyroscopeRuby:   PosFirst,
	PyroscopePython: PosFirst,
	PyroscopeJava:   FunctionFirst,
	PyroscopeEbpf:   FunctionFirst,
	PyroscopePhp:    PosFirst,
	Unknown:         FunctionFirst,
}

const (
	SamplesUnits         Units = "samples"
	NanosecondsUnit      Units = "nanoseconds"
	ObjectsUnit          Units = "objects"
	BytesUnit            Units = "bytes"
	GoroutinesUnits      Units = "goroutines"
	LockNanosecondsUnits Units = "lock_nanoseconds"
	LockSamplesUnits     Units = "local_samples"
)

const (
	PyroscopeNodeJs = "node"
	PyroscopeGolang = "go"
	PyroscopeRust   = "rs"
	PyroscopeDotnet = "dotnet"
	PyroscopeRuby   = "rb"
	PyroscopePython = "py"
	PyroscopeJava   = "java"
	PyroscopeEbpf   = "ebpf"
	PyroscopePhp    = "php"
	Unknown         = "unknown"
)

type SequenceType int

const (
	_ SequenceType = iota
	PosFirst
	FunctionFirst
)

func FormatPositionAndName(str string, t FormatType) string {
	str = strings.TrimSpace(str)
	idx := strings.Index(str, " ")
	if idx < 0 {
		return str // means no position
	}
	joiner := func(name, pos string) string {
		var b strings.Builder
		b.Grow(len(name) + len(pos) + 1)
		b.Write([]byte(name))
		b.Write([]byte{' '})
		b.Write([]byte(pos))
		return b.String()
	}
	name := str[:idx]
	idx = strings.LastIndex(str, " ")
	pos := str[idx+1:]
	sequenceType := sequenceMapping[t]
	switch sequenceType {
	case PosFirst:
		return joiner(pos, name)
	case FunctionFirst:
		return joiner(name, pos)
	default:
		return str
	}
}

func FormatPostionAndNames(strs []string, t FormatType) []string {
	for i := range strs {
		strs[i] = FormatPositionAndName(strs[i], t)
	}
	return strs
}

func (u Units) DetectValueType() string {
	switch u {
	case NanosecondsUnit, SamplesUnits:
		return "cpu"
	case ObjectsUnit, BytesUnit:
		return "mem"
	case GoroutinesUnits:
		return "goroutines"
	case LockSamplesUnits, LockNanosecondsUnits:
		return "mutex"
	}
	return "unknown"
}

func DetectProfileType(valType string) Kind {
	switch valType {
	case "inuse_space", "inuse_objects", "alloc_space", "alloc_objects", "alloc-size", "alloc-samples", "alloc_in_new_tlab_objects", "alloc_in_new_tlab_bytes", "alloc_outside_tlab_objects", "alloc_outside_tlab_bytes":
		return MemKind
	case "samples", "cpu", "itimer", "lock_count", "lock_duration", "wall":
		return CPUKind
	case "mutex_count", "mutex_duration", "block_duration", "block_count", "contentions", "delay", "lock-time", "lock-count":
		return MemKind
	case "goroutines", "goroutine":
		return GoRoutinesKind
	case "exception":
		return ExceptionKind
	default:
		return UnknownKind
	}
}

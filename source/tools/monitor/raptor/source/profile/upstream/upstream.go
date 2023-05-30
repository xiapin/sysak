package upstream

import (
	"time"

	"github.com/pyroscope-io/pyroscope/pkg/storage/metadata"
	"github.com/pyroscope-io/pyroscope/pkg/structs/transporttrie"
)

type UploadJob struct {
	Name            string
	StartTime       time.Time
	EndTime         time.Time
	SpyName         string
	SampleRate      uint32
	Units           metadata.Units
	AggregationType metadata.AggregationType
	Trie            *transporttrie.Trie //实际栈信息放置的地方
}

type Upstream interface {
	Start()
	Stop()
	Upload(u *UploadJob)
	DumpMetaData()
}

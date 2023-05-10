package direct

import (
	"context"
	"fmt"
	"regexp"
	"runtime/debug"
	"sort"
	"strings"
	"sync"

	"github.com/sirupsen/logrus"

	"github.com/chentao-kernel/cloud_ebpf/comm"
	"github.com/chentao-kernel/cloud_ebpf/profile/upstream"
	"github.com/pyroscope-io/pyroscope/pkg/storage"
	"github.com/pyroscope-io/pyroscope/pkg/storage/segment"
	"github.com/pyroscope-io/pyroscope/pkg/storage/tree"
)

const upstreamThreads = 1

const (
	HOTSTACK_APP string = "hotstack in app"
	HOTSTACK_OS  string = "hotstack in os"
)

type Local struct {
	queue    chan *upstream.UploadJob
	stop     chan struct{}
	wg       sync.WaitGroup
	MetaData map[string]uint64
}

type LocalMetaData struct {
	stack string
	count uint64
	total uint64
}

type sortedMap struct {
	m map[string]LocalMetaData
	s []string
}

func (sm *sortedMap) Len() int {
	return len(sm.m)
}

// https://golangnote.com/topic/75.html
func (sm *sortedMap) Less(i, j int) bool {
	a := sm.m[sm.s[i]]
	b := sm.m[sm.s[j]]
	return a.total > b.total
}

func (sm *sortedMap) Swap(i, j int) {
	sm.s[i], sm.s[j] = sm.s[j], sm.s[i]
}

func sortedVals(m map[string]LocalMetaData) []string {
	sm := new(sortedMap)
	sm.m = m
	sm.s = make([]string, len(m))
	i := 0
	for key, _ := range m {
		sm.s[i] = key
		i++
	}
	sort.Sort(sm)
	return sm.s
}

type Direct struct {
	storage  storage.Putter
	exporter storage.MetricsExporter
	queue    chan *upstream.UploadJob
	stop     chan struct{}
	wg       sync.WaitGroup
}

func NewLocal() *Local {
	return &Local{
		queue:    make(chan *upstream.UploadJob, 100),
		stop:     make(chan struct{}),
		MetaData: make(map[string]uint64),
	}
}

func (l *Local) Start() {
	l.wg.Add(upstreamThreads)
	for i := 0; i < upstreamThreads; i++ {
		go l.uploadLoop()
	}
}

func (l *Local) uploadLoop() {
	defer l.wg.Done()
	for {
		select {
		case j := <-l.queue:
			l.safeUpload(j)
		case <-l.stop:
			return
		}
	}
}

func (l *Local) safeUpload(j *upstream.UploadJob) {
	defer func() {
		if r := recover(); r != nil {
			logrus.Errorf("panic recovered: %v; %v", r, string(debug.Stack()))
		}
	}()
	l.uploadProfile(j)
}

func stackCheck(stack string, reg *regexp.Regexp) string {
	res := reg.FindAllStringSubmatch(stack, -1)
	if res == nil {
		return HOTSTACK_APP
	}
	return HOTSTACK_OS
}

func (l *Local) DumpMetaData() {
	data := make(map[string]LocalMetaData)
	for key, val := range l.MetaData {
		//fmt.Printf("meta,val:%d\n", val)
		items := strings.Split(key, ";")
		appName := items[0]
		if data[appName].count < val {
			tmp := data[appName]
			tmp.count = val
			tmp.stack = key
			tmp.total += val
			data[appName] = tmp
		}
	}
	var procTotalHitCount uint64
	sortedDatas := sortedVals(data)
	for _, res := range sortedDatas {
		procTotalHitCount += data[res].total
	}
	jf := comm.NewJsonFormat()
	// pattern is simple,like:finish_task_switch
	reg := regexp.MustCompile(`[a-z]+_[a-z]+`)
	for i, res := range sortedDatas {
		if i < 10 {
			var v comm.TableData
			v.COMM = res
			v.TaskHitRatio = fmt.Sprintf("%2.2f", float32(data[res].total)*100/float32(procTotalHitCount))
			v.HotStack = data[res].stack
			v.HotStackHitRatio = fmt.Sprintf("%2.2f", float32(data[res].count)*100/float32(data[res].total))
			funcs := strings.Split(v.HotStack, ";")
			fn := funcs[len(funcs)-2]
			v.COMMENT = stackCheck(fn, reg)
			jf.Append(v)
		}
	}
	json, err := jf.Marshal()
	if err != nil {
		fmt.Printf("Json marshal failed: %v", err)
	}
	jf.Print(json)
}

/*
* ==metaData:sh;[unknown];[unknown];asm_exc_page_fault;exc_page_fault;
do_user_addr_fault;handle_mm_fault;__handle_mm_fault;do_fault;do_cow_fault;
__anon_vma_prepare;kmem_cache_alloc;
*/

func (l *Local) metaDataInsert(name []byte, val uint64) {
	tmp := string(name)
	count := l.MetaData[tmp]
	count += val
	l.MetaData[tmp] = count
}

func (l *Local) uploadProfile(j *upstream.UploadJob) {
	j.Trie.Iterate(l.metaDataInsert)
}

func (l *Local) Upload(j *upstream.UploadJob) {
	select {
	case l.queue <- j:
	case <-l.stop:
		return
	default:
		logrus.Error("Local upload queue is full, dropping a profile")
	}
}

func (l *Local) Stop() {
	close(l.stop)
	l.wg.Wait()
}

func New(s storage.Putter, e storage.MetricsExporter) *Direct {
	return &Direct{
		storage:  s,
		exporter: e,
		queue:    make(chan *upstream.UploadJob, 100),
		stop:     make(chan struct{}),
	}
}

func (u *Direct) Start() {
	u.wg.Add(upstreamThreads)
	for i := 0; i < upstreamThreads; i++ {
		go u.uploadLoop()
	}
}

func (U *Direct) DumpMetaData() {

}

func (u *Direct) Stop() {
	close(u.stop)
	u.wg.Wait()
}

func (u *Direct) Upload(j *upstream.UploadJob) {
	select {
	case u.queue <- j:
	case <-u.stop:
		return
	default:
		logrus.Error("Direct upload queue is full, dropping a profile")
	}
}

func (u *Direct) uploadProfile(j *upstream.UploadJob) {
	key, err := segment.ParseKey(j.Name)
	if err != nil {
		logrus.WithField("key", key).Error("invalid key:")
		return
	}

	pi := &storage.PutInput{
		StartTime:       j.StartTime,
		EndTime:         j.EndTime,
		Key:             key,
		Val:             tree.New(),
		SpyName:         j.SpyName,
		SampleRate:      j.SampleRate,
		Units:           j.Units,
		AggregationType: j.AggregationType,
	}

	cb := pi.Val.Insert
	if o, ok := u.exporter.Evaluate(pi); ok {
		cb = func(k []byte, v uint64) {
			o.Observe(k, int(v))
			cb(k, v)
		}
	}

	j.Trie.Iterate(cb)
	if err = u.storage.Put(context.TODO(), pi); err != nil {
		logrus.WithError(err).Error("failed to store a local profile")
	}
}

func (u *Direct) uploadLoop() {
	defer u.wg.Done()
	for {
		select {
		case j := <-u.queue:
			u.safeUpload(j)
		case <-u.stop:
			return
		}
	}
}

// do safe upload
func (u *Direct) safeUpload(j *upstream.UploadJob) {
	defer func() {
		if r := recover(); r != nil {
			logrus.Errorf("panic recovered: %v; %v", r, string(debug.Stack()))
		}
	}()
	u.uploadProfile(j)
}

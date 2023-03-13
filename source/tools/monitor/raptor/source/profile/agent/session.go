package agent

import (
	"fmt"
	"os"
	"sync"
	"time"

	logger2 "github.com/pyroscope-io/pyroscope/pkg/agent/log"
	"github.com/pyroscope-io/pyroscope/pkg/util/alignedticker"

	_ "github.com/pyroscope-io/pyroscope/pkg/agent/debugspy"
	"github.com/chentao-kernel/cloud_ebpf/profile/upstream"
	"github.com/pyroscope-io/pyroscope/pkg/flameql"
	"github.com/pyroscope-io/pyroscope/pkg/storage/segment"
	"github.com/pyroscope-io/pyroscope/pkg/util/process"
	"github.com/pyroscope-io/pyroscope/pkg/util/throttle"

	"github.com/chentao-kernel/cloud_ebpf/profile/spy"
	"github.com/pyroscope-io/pyroscope/pkg/structs/transporttrie"
)

const errorThrottlerPeriod = 10 * time.Second

type ProfileSession struct {
	// configuration, doesn't change
	upstream         upstream.Upstream
	spyName          string
	sampleRate       uint32
	profileTypes     []spy.ProfileType
	uploadRate       time.Duration
	disableGCRuns    bool
	withSubprocesses bool
	clibIntegration  bool
	spyFactory       SpyFactory
	noForkDetection  bool
	pid              int
	cpu              int
	space 			 int

	logger    logger2.Logger
	throttler *throttle.Throttler
	stopOnce  sync.Once
	stopCh    chan struct{}
	trieMutex sync.Mutex

	// these things do change:
	appName            string
	startTimeTruncated time.Time

	spies map[int][]spy.Spy // pid, profileType
	// string is appName, int is index in pids
	previousTries map[string][]*transporttrie.Trie
	tries         map[string][]*transporttrie.Trie
}

type SpyFactory func(pid int) ([]spy.Spy, error)

type SessionConfig struct {
	upstream.Upstream
	logger2.Logger
	AppName          string
	Tags             map[string]string
	ProfilingTypes   []spy.ProfileType
	DisableGCRuns    bool
	SpyName          string
	SampleRate       uint32
	UploadRate       time.Duration
	Pid              int
	Cpu 			 int 
	Space 			 int
	Dport 			 int
	Sport 			 int
	Delay			 int
	WithSubprocesses bool
	ClibIntegration  bool
}

func NewSessionWithSpyFactory(c SessionConfig, spyFactory SpyFactory) (*ProfileSession, error) {
	appName, err := mergeTagsWithAppName(c.AppName, c.Tags)
	if err != nil {
		return nil, err
	}

	ps := &ProfileSession{
		upstream:         c.Upstream,
		appName:          appName,
		spyName:          c.SpyName,
		profileTypes:     c.ProfilingTypes,
		disableGCRuns:    c.DisableGCRuns,
		sampleRate:       c.SampleRate,
		uploadRate:       c.UploadRate,
		pid:              c.Pid,
		cpu:			  c.Cpu,
		space:	          c.Space,
		spies:            make(map[int][]spy.Spy),
		stopCh:           make(chan struct{}),
		withSubprocesses: c.WithSubprocesses,
		clibIntegration:  c.ClibIntegration,
		logger:           c.Logger,
		throttler:        throttle.New(errorThrottlerPeriod),
		spyFactory:       spyFactory,

		// string is appName, int is index in pids
		previousTries: make(map[string][]*transporttrie.Trie),
		tries:         make(map[string][]*transporttrie.Trie),
	}

	ps.initializeTries(ps.appName)

	return ps, nil
}
// no used in ebpf
func NewGenericSpyFactory(c SessionConfig) SpyFactory {
	return func(pid int) ([]spy.Spy, error) {
		var res []spy.Spy

		// sf为处理的回调函数 Start
		sf, err := spy.StartFunc(c.SpyName)
		if err != nil {
			return res, err
		}

		for _, pt := range c.ProfilingTypes {
			params := spy.InitParams{
				Pid:           pid,
				ProfileType:   pt,
				SampleRate:    c.SampleRate,
				DisableGCRuns: c.DisableGCRuns,
				Logger:        c.Logger,
			}
			// s是一个Spy
			s, err := sf(params)

			if err != nil {
				return res, err
			}
			res = append(res, s)
		}
		return res, nil
	}
}

func addSuffix(name string, ptype spy.ProfileType) (string, error) {
	k, err := segment.ParseKey(name)
	if err != nil {
		return "", err
	}
	k.Add("__name__", k.AppName()+"."+string(ptype))
	return k.Normalized(), nil
}

func mergeTagsWithAppName(appName string, tags map[string]string) (string, error) {
	k, err := segment.ParseKey(appName)
	if err != nil {
		return "", err
	}
	for tagKey, tagValue := range tags {
		if flameql.IsTagKeyReserved(tagKey) {
			continue
		}
		if err = flameql.ValidateTagKey(tagKey); err != nil {
			return "", err
		}
		k.Add(tagKey, tagValue)
	}
	return k.Normalized(), nil
}

func (ps *ProfileSession) takeSnapshots() {
	var samplingCh <-chan time.Time
	if ps.areSpiesResettable() {
		samplingCh = make(chan time.Time) // will never fire
	} else {
		ticker := time.NewTicker(time.Second / time.Duration(ps.sampleRate))
		defer ticker.Stop()
		samplingCh = ticker.C
	}
	uploadTicker := alignedticker.NewAlignedTicker(ps.uploadRate)
	defer uploadTicker.Stop()
	for {
		select {
		case endTimeTruncated := <-uploadTicker.C:
			ps.resetSpies()
			// 获取ebpf数据
			ps.takeSnapshot()
			// 上传数据
			ps.reset(endTimeTruncated)
		case <-samplingCh:
			ps.takeSnapshot()
		case <-ps.stopCh:
			ps.StopSpies()
			return
		}
	}
}

func (ps *ProfileSession) StopSpies() error {
	for _, sarr := range ps.spies {
		for _, s := range sarr {
			return s.StopData()
		}
	}
	return nil
}

func (ps *ProfileSession) RestartSpies() error {
	for _, sarr := range ps.spies {
		for _, s := range sarr {
			return s.Restart()
		}
	}
	return nil
}

func (ps *ProfileSession) takeSnapshot() {
	ps.trieMutex.Lock()
	defer ps.trieMutex.Unlock()
	pidsToRemove := []int{}
	for pid, sarr := range ps.spies {
		for i, s := range sarr {
			labelsCache := map[string]string{}
			// stack是解析的栈信息，v表示命中次数
			err := s.Snapshot(func(labels *spy.Labels, stack []byte, v uint64) error {
				appName := ps.appName
				if labels != nil {
					if newAppName, ok := labelsCache[labels.ID()]; ok {
						appName = newAppName
					} else {
						newAppName, err := mergeTagsWithAppName(appName, labels.Tags())
						if err != nil {
							return fmt.Errorf("error setting tags: %w", err)
						}
						appName = newAppName
						labelsCache[labels.ID()] = appName
					}
				}
				if len(stack) > 0 {
					if _, ok := ps.tries[appName]; !ok {
						ps.initializeTries(appName)
					}
					// 一个spy一棵trie树
					//fmt.Printf("==stack:%s\n", string(stack))
					ps.tries[appName][i].Insert(stack, v, true)
				}
				return nil
			})
			if err != nil {
				if pid >= 0 && !process.Exists(pid) {
					ps.logger.Debugf("error taking snapshot: PID %d: process doesn't exist?", pid)
					pidsToRemove = append(pidsToRemove, pid)
				} else {
					ps.throttler.Run(func(skipped int) {
						if skipped > 0 {
							ps.logger.Errorf("error taking snapshot: %v, %d messages skipped due to throttling", err, skipped)
						} else {
							ps.logger.Errorf("error taking snapshot: %v", err)
						}
					})
				}
			}
		}
	}
	for _, pid := range pidsToRemove {
		for _, s := range ps.spies[pid] {
			s.Stop()
		}
		delete(ps.spies, pid)
	}
}

func (ps *ProfileSession) areSpiesResettable() bool {
	for _, sarr := range ps.spies {
		for _, s := range sarr {
			if _, ok := s.(spy.Resettable); ok {
				return true
			}
		}
	}
	return false
}

func (ps *ProfileSession) resetSpies() {
	for _, sarr := range ps.spies {
		for _, s := range sarr {
			if sr, ok := s.(spy.Resettable); ok {
				sr.Reset()
			}
		}
	}
}

func (ps *ProfileSession) initializeSpies(pid int) ([]spy.Spy, error) {
	return ps.spyFactory(pid)
}

func (ps *ProfileSession) ChangeName(newName string) error {
	ps.trieMutex.Lock()
	defer ps.trieMutex.Unlock()

	var err error
	newName, err = mergeTagsWithAppName(newName, map[string]string{})
	if err != nil {
		return err
	}

	ps.appName = newName
	ps.initializeTries(ps.appName)

	return nil
}

func (ps *ProfileSession) initializeTries(appName string) {
	if _, ok := ps.previousTries[appName]; !ok {
		// TODO Only set the trie if it's not already set
		ps.previousTries[appName] = []*transporttrie.Trie{}
		ps.tries[appName] = []*transporttrie.Trie{}
		for i := 0; i < len(ps.profileTypes); i++ {
			ps.previousTries[appName] = append(ps.previousTries[appName], nil)
			ps.tries[appName] = append(ps.tries[appName], transporttrie.New())
		}
	}
}

// SetTags - add new tags to the session.
func (ps *ProfileSession) SetTags(tags map[string]string) error {
	newName, err := mergeTagsWithAppName(ps.appName, tags)
	if err != nil {
		return err
	}
	return ps.ChangeName(newName)
}

// SetTag - add a new tag to the session.
func (ps *ProfileSession) SetTag(key, val string) error {
	return ps.SetTags(map[string]string{key: val})
}

// RemoveTags - remove tags from the session.
func (ps *ProfileSession) RemoveTags(keys ...string) error {
	removals := make(map[string]string)
	for _, key := range keys {
		// 'Adding' a key with an empty string triggers a key removal.
		removals[key] = ""
	}
	newName, err := mergeTagsWithAppName(ps.appName, removals)
	if err != nil {
		return err
	}
	return ps.ChangeName(newName)
}

func (ps *ProfileSession) Start() error {
	ps.reset(time.Now().Truncate(ps.uploadRate))

	pid := ps.pid
	spies, err := ps.initializeSpies(pid)
	if err != nil {
		return err
	}

	ps.spies[pid] = spies

	go ps.takeSnapshots()
	return nil
}

// the difference between stop and reset is that reset stops current session
// and then instantly starts a new one
func (ps *ProfileSession) reset(endTimeTruncated time.Time) {
	ps.trieMutex.Lock()
	defer ps.trieMutex.Unlock()

	// if the process was forked the spy will keep profiling the old process. That's usually not what you want
	//   so in that case we stop the profiling session early
	if ps.clibIntegration && !ps.noForkDetection && ps.isForked() {
		ps.logger.Debugf("fork detected, stopping the session")
		ps.stopOnce.Do(func() {
			close(ps.stopCh)
		})
		return
	}

	// upload the read data to server
	if !ps.startTimeTruncated.IsZero() {
		// ps.logger.Debugf("tao, fork detected, stopping the session")
		ps.uploadTries(endTimeTruncated)
	}

	// reset the start time
	ps.startTimeTruncated = endTimeTruncated
}

func (ps *ProfileSession) Stop() {
	ps.trieMutex.Lock()
	defer ps.trieMutex.Unlock()

	ps.stopOnce.Do(func() {
		// TODO: wait for stopCh consumer to finish!
		close(ps.stopCh)
		// before stopping, upload the tries
		if !ps.startTimeTruncated.IsZero() {
			ps.uploadTries(ps.startTimeTruncated.Add(ps.uploadRate))
		} // was never started
	})
}

func (ps *ProfileSession) uploadTries(endTimeTruncated time.Time) {
	// 遍历trie树
	for name, tarr := range ps.tries {
		for i, trie := range tarr {
			//fmt.Printf("~~trie:%s\n", string(trie.Bytes()))
			profileType := ps.profileTypes[i]
			skipUpload := false

			if trie != nil {
				endTime := endTimeTruncated
				startTime := endTime.Add(-ps.uploadRate)

				uploadTrie := trie
				// ebpf不是cumulative类型
				if profileType.IsCumulative() {
					previousTrie := ps.previousTries[name][i]
					if previousTrie == nil {
						skipUpload = true
					} else {
						// TODO: Diff doesn't remove empty branches. We need to add that at some point
						uploadTrie = trie.Diff(previousTrie)
					}
				}

				if !skipUpload && !uploadTrie.IsEmpty() {
					nameWithSuffix, _ := addSuffix(name, profileType)
					ps.upstream.Upload(&upstream.UploadJob{
						Name:            nameWithSuffix,
						StartTime:       startTime,
						EndTime:         endTime,
						SpyName:         ps.spyName,
						SampleRate:      ps.sampleRate,
						Units:           profileType.Units(),
						AggregationType: profileType.AggregationType(),
						Trie:            uploadTrie,
					})
				}
				if profileType.IsCumulative() {
					ps.previousTries[name][i] = trie
				}
			}
			ps.tries[name][i] = transporttrie.New()
		}
	}
}

func (ps *ProfileSession) isForked() bool {
	return os.Getpid() != ps.pid
}

package ebpf

import (
	"sync"
	"fmt"

	"github.com/chentao-kernel/cloud_ebpf/profile/spy"
)

type EbpfSpy struct {
	mutex  sync.Mutex
	reset  bool
	stop   bool
	stopdata bool
	stopCh chan struct{}

	// session中包含ebpf的信息
	session *Session
}

func NewEBPFSpy(s *Session) *EbpfSpy {
	return &EbpfSpy{
		session: s,
		stopCh:  make(chan struct{}),
	}
}

func (s *EbpfSpy) Snapshot(cb func(*spy.Labels, []byte, uint64) error) error {
	s.mutex.Lock()
	defer s.mutex.Unlock()

	var err error
	if !s.reset {
		return nil
	}

	s.reset = false
	// Use different reset handle by ProfileType, hard code
	if s.session.ProfileTypes[0] == spy.ProfileCPU {
		err = s.session.Reset(func(labels *spy.Labels, name []byte, v uint64, pid uint32) error {
			return cb(labels, name, v)
		})
	} else if s.session.ProfileTypes[0] == spy.ProfileOFFCPU {
		err = s.session.ResetOffCpu(func(labels *spy.Labels, name []byte, v uint64, pid uint32) error {
			return cb(labels, name, v)
		})
	} else if s.session.ProfileTypes[0] == spy.ProfileUserSlow {
		err = s.session.ResetUserSlow(func(labels *spy.Labels, name []byte, v uint64, pid uint32) error {
			return cb(labels, name, v)
		})
	} else if s.session.ProfileTypes[0] == spy.ProfilePingSlow {
		err = s.session.ResetPingSlow(func(labels *spy.Labels, name []byte, v uint64, pid uint32) error {
			return cb(labels, name, v)
		})
	}

	if s.stop {
		// 对ebpf程序的stop
		s.session.Stop()
		// 这里的通道值没有实际使用
		s.stopCh <- struct{}{}
	}

	return err
}

func (s *EbpfSpy) StopData() error {
	s.mutex.Lock()
	s.stopdata = true
	s.mutex.Unlock()
	s.session.StopData()
	return nil
}

func (s *EbpfSpy) Restart() error {
	if s.stopdata == false {
		return fmt.Errorf("Spy is online")
	}
	s.mutex.Lock()
	s.stopdata = false
	s.mutex.Unlock()
	s.session.Restart()
	return nil
}

func (s *EbpfSpy) Stop() error {
	s.mutex.Lock()
	s.stop = true
	s.mutex.Unlock()
	<-s.stopCh
	return nil
}

func (s *EbpfSpy) Reset() {
	s.mutex.Lock()
	defer s.mutex.Unlock()

	s.reset = true
}
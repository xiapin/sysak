package ebpf

import "C"
import (
	"fmt"
	"sync"

	"github.com/chentao-kernel/cloud_ebpf/profile/symtab"
	"github.com/pyroscope-io/pyroscope/pkg/util/genericlru"
)

type symbolCacheEntry struct {
	symbolTable symtab.SymbolTable
	roundNumber int
}
type pidKey uint32

type symbolCache struct {
	pid2Cache *genericlru.GenericLRU[pidKey, symbolCacheEntry]
	mutex     sync.Mutex
}

func newSymbolCache(cacheSize int) (*symbolCache, error) {
	pid2Cache, err := genericlru.NewGenericLRU[pidKey, symbolCacheEntry](cacheSize, func(pid pidKey, e *symbolCacheEntry) {
		e.symbolTable.Close()
	})
	if err != nil {
		return nil, err
	}
	return &symbolCache{
		pid2Cache: pid2Cache,
	}, nil
}

func (sc *symbolCache) bccResolve(pid uint32, addr uint64, roundNumber int) symtab.Symbol {
	e := sc.getOrCreateCacheEntry(pidKey(pid))
	staleCheck := false
	if roundNumber != e.roundNumber {
		e.roundNumber = roundNumber
		staleCheck = true
	}
	// duck模式，kernelSymTab或者userSymTab
	return e.symbolTable.Resolve(addr, staleCheck)
}

func (sc *symbolCache) getOrCreateCacheEntry(pid pidKey) *symbolCacheEntry {
	sc.mutex.Lock()
	defer sc.mutex.Unlock()
	if cache, ok := sc.pid2Cache.Get(pid); ok {
		return cache
	}
	var symbolTable symtab.SymbolTable
	exe := fmt.Sprintf("/proc/%d/exe", pid)
	bcc := func() symtab.SymbolTable {
		return symtab.NewBCCSymbolTable(int(pid))
	}
	symbolTable, err := symtab.NewGoSymbolTable(exe, &bcc)
	if err != nil || symbolTable == nil {
		// bccSymTab pid=0或者非go程序
		symbolTable = bcc()
	}
	e := &symbolCacheEntry{symbolTable: symbolTable}
	sc.pid2Cache.Add(pid, e)
	return e
}

func (sc *symbolCache) clear() {
	sc.mutex.Lock()
	defer sc.mutex.Unlock()
	for _, pid := range sc.pid2Cache.Keys() {
		sc.pid2Cache.Remove(pid)
	}
}

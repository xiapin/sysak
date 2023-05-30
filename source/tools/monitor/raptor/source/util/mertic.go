package util

import (
	"container/list"
	"sync"
)

type MetricQueue struct {
	Queue *list.List
	Mutex sync.RWMutex
}

var MQueue MetricQueue

func MetricQueueInit() {
	MQueue.Queue = list.New()
}
package agent

import (
	"sync"
	"fmt"
	"github.com/prometheus/procfs"
)

const jumpBackSeconds = 3.0

type CpuCollector struct {
	fs				procfs.FS
	CpuStats 		[]procfs.CPUStat
	LastCpuStats 	[]procfs.CPUStat
	CpuMetric 	[]procfs.CPUStat
	CpuTotal    	procfs.CPUStat
	LastCpuTotal    procfs.CPUStat
	cpuStatsMutex 	sync.Mutex
}

func NewCPUCollector() (*CpuCollector, error) {
	fs, err := procfs.NewFS("/proc")
	if err != nil {
		return nil, fmt.Errorf("Failed to open procfs:%w", err)
	}
	c := &CpuCollector{
		fs: 	fs,
	}
	return c, nil
}

func (c *CpuCollector) StatCPUTotal(cpu procfs.CPUStat) float64 {
	return (cpu.User + cpu.Nice + cpu.System + cpu.Idle + cpu.Iowait + cpu.IRQ + cpu.SoftIRQ + cpu.Steal + cpu.Guest + cpu.GuestNice)
}

func (c *CpuCollector) UpdateCpuStats() error {
	stats, err := c.fs.Stat()
	if err != nil {
		return err
	}
	c.updateCPUStats(stats)
	return nil
}

func (c *CpuCollector) updateCPUStats(stat procfs.Stat) {

	c.cpuStatsMutex.Lock()
	defer c.cpuStatsMutex.Unlock()
	c.LastCpuTotal = c.CpuTotal
	c.CpuTotal = stat.CPUTotal
	newStats := stat.CPU
	if len(c.CpuStats) != len(newStats) {
		c.CpuStats = make([]procfs.CPUStat, len(newStats))
		c.LastCpuStats = make([]procfs.CPUStat, len(newStats))
		c.CpuMetric = make([]procfs.CPUStat, len(newStats))
	}

	for i, n := range newStats {
		if (c.CpuStats[i].Idle - n.Idle) >= jumpBackSeconds {
			c.CpuStats[i] = procfs.CPUStat{}
			c.LastCpuStats[i] = procfs.CPUStat{}
		}

		if n.Idle >= c.CpuStats[i].Idle {
			c.LastCpuStats[i].Idle = c.CpuStats[i].Idle
			c.CpuStats[i].Idle = n.Idle
		} else {
			fmt.Printf("CPU Idle counter jumpback cpu:%v, old_value:%v, new_value:%v", i, c.CpuStats[i].Idle, n.Idle)
		}

		if n.User >= c.CpuStats[i].User {
			c.LastCpuStats[i].User = c.CpuStats[i].User
			c.CpuStats[i].User = n.User
		} else {
			fmt.Printf("CPU User counter jumpback cpu:%v, old_value:%v, new_value:%v", i, c.CpuStats[i].User, n.User)
		}

		if n.Nice >= c.CpuStats[i].Nice {
			c.LastCpuStats[i].Nice = c.CpuStats[i].Nice
			c.CpuStats[i].Nice = n.Nice
		} else {
			fmt.Printf("CPU Nice counter jumpback cpu:%v, old_value:%v, new_value:%v", i, c.CpuStats[i].Nice, n.Nice)
		}

		if n.System >= c.CpuStats[i].System {
			c.LastCpuStats[i].System = c.CpuStats[i].System
			c.CpuStats[i].System = n.System
		} else {
			fmt.Printf("CPU System counter jumpback cpu:%v, old_value:%v, new_value:%v", i, c.CpuStats[i].System, n.System)
		}

		if n.Iowait >= c.CpuStats[i].Iowait {
			c.LastCpuStats[i].Iowait = c.CpuStats[i].Iowait
			c.CpuStats[i].Iowait = n.Iowait
		} else {
			fmt.Printf("CPU Iowait counter jumpback cpu:%v, old_value:%v, new_value:%v", i, c.CpuStats[i].Iowait, n.Iowait)
		}

		if n.IRQ >= c.CpuStats[i].IRQ {
			c.LastCpuStats[i].IRQ = c.CpuStats[i].IRQ
			c.CpuStats[i].IRQ = n.IRQ
		} else {
			fmt.Printf("CPU IRQ counter jumpback cpu:%v, old_value:%v, new_value:%v", i, c.CpuStats[i].IRQ, n.IRQ)
		}

		if n.SoftIRQ >= c.CpuStats[i].SoftIRQ {
			c.LastCpuStats[i].SoftIRQ = c.CpuStats[i].SoftIRQ
			c.CpuStats[i].SoftIRQ = n.SoftIRQ
		} else {
			fmt.Printf("CPU SoftIRQ counter jumpback cpu:%v, old_value:%v, new_value:%v", i, c.CpuStats[i].SoftIRQ, n.SoftIRQ)
		}

		if n.Steal >= c.CpuStats[i].Steal {
			c.LastCpuStats[i].Steal = c.CpuStats[i].Steal
			c.CpuStats[i].Steal = n.Steal
		} else {
			fmt.Printf("CPU Steal counter jumpback cpu:%v, old_value:%v, new_value:%v", i, c.CpuStats[i].Steal, n.Steal)
		}

		if n.Guest >= c.CpuStats[i].Guest {
			c.LastCpuStats[i].Guest = c.CpuStats[i].Guest
			c.CpuStats[i].Guest = n.Guest
		} else {
			fmt.Printf("CPU Guest counter jumpback cpu:%v, old_value:%v, new_value:%v", i, c.CpuStats[i].Guest, n.Guest)
		}

		if n.GuestNice >= c.CpuStats[i].GuestNice {
			c.LastCpuStats[i].GuestNice = c.CpuStats[i].GuestNice
			c.CpuStats[i].GuestNice = n.GuestNice
		} else {
			fmt.Printf("CPU GuestNice counter jumpback cpu:%v, old_value:%v, new_value:%v", i, c.CpuStats[i].GuestNice, n.GuestNice)
		}
	}
}

func (c *CpuCollector) MertricCalculate() {
	for i, val := range c.CpuStats {
		total := c.StatCPUTotal(val) - c.StatCPUTotal(c.LastCpuStats[i])
		c.CpuMetric[i].User = ((val.User - c.LastCpuStats[i].User) / total) * 100
		c.CpuMetric[i].Nice = ((val.Nice - c.LastCpuStats[i].Nice) / total) * 100
		c.CpuMetric[i].System = ((val.System - c.LastCpuStats[i].System) / total) * 100
		c.CpuMetric[i].Idle = ((val.Idle - c.LastCpuStats[i].Idle) / total) * 100
		c.CpuMetric[i].Iowait = ((val.Iowait - c.LastCpuStats[i].Iowait) / total) * 100
		c.CpuMetric[i].IRQ = ((val.IRQ - c.LastCpuStats[i].IRQ) / total) * 100
		c.CpuMetric[i].SoftIRQ = ((val.SoftIRQ - c.LastCpuStats[i].SoftIRQ) / total) * 100
		c.CpuMetric[i].Steal = ((val.Steal - c.LastCpuStats[i].Steal) / total) * 100
		c.CpuMetric[i].Guest = ((val.Guest - c.LastCpuStats[i].Guest) / total) * 100
		c.CpuMetric[i].GuestNice = ((val.GuestNice - c.LastCpuStats[i].GuestNice) / total) * 100
	}
}
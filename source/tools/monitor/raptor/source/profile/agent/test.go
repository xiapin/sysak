package agent

import (
	"fmt"
	"time"
	"math"
)

func percentCal(val1 float64, val2 float64) float64 {
	if val2 == 0 {
		return 0
	}

	return (val1 / val2) * 100
}

func Round(f float64, n int) float64 {
	pow10_n := math.Pow10(n)
	return math.Trunc((f+05./pow10_n)*pow10_n)/pow10_n
}

func TestCpu() {
	cpu, err := NewCPUCollector()
	if err != nil {
		fmt.Printf("Failed to new cpu collector:%v\n", err)
	}
	var count int
	for ;; {
		count++
		cpu.UpdateCpuStats()
		cpu.MertricCalculate()
		/*
		for i, cp := range cpu.CpuStats {
			fmt.Printf("============cpu:%d==========\n", i)
			total := cpu.StatCPUTotal(cp) - cpu.StatCPUTotal(cpu.LastCpuStats[i])
			fmt.Printf("us:%.1f, ni:%.1f, sys:%.1f, id:%.1f, hi:%.1f, si:%.1f, st:%.1f\n",
						percentCal(cp.User - cpu.LastCpuStats[i].User, total),
						percentCal(cp.Nice - cpu.LastCpuStats[i].Nice, total),
						percentCal(cp.System - cpu.LastCpuStats[i].System, total),
						percentCal(cp.Idle - cpu.LastCpuStats[i].Idle, total),
						percentCal(cp.IRQ - cpu.LastCpuStats[i].IRQ, total),
						percentCal(cp.SoftIRQ - cpu.LastCpuStats[i].SoftIRQ, total),
						percentCal(cp.Steal - cpu.LastCpuStats[i].Steal, total),
						)
		}
		*/
		fmt.Printf("metric:%v\n", cpu.CpuMetric)
		// 时间更新跟top保持一致
		time.Sleep(3 * time.Second)
	}
}
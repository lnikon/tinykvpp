package storage

import (
	"fmt"
	"log"
	"slices"
	"sync/atomic"
	"time"

	"github.com/lnikon/kvtest/pkg/core"
)

// MemtableStressScenario tests memtable behavior under various load conditions
type MemtableStressScenario struct {
	name              string
	recordSize        int
	recordCount       int
	concurrentWriters int
	flushThreshold    int

	// Internal state for tracking
	writeLatencies    []time.Duration
	flushDetected     bool
	consistencyErrors atomic.Int64
}

// NewMemtableStressScenario creates a new memtable stress test scenario
func NewMemtableStressScenario(params map[string]interface{}) *MemtableStressScenario {
	scenario := &MemtableStressScenario{
		name:              "MemtableStress",
		recordSize:        1024,
		recordCount:       10000,
		concurrentWriters: 4,
		flushThreshold:    8 * 1024 * 1024,
		writeLatencies:    make([]time.Duration, 0, 10000),
	}

	// Apply parameters from config
	if val, ok := params["record_size"].(int); ok {
		scenario.recordSize = val
	}
	if val, ok := params["record_count"].(int); ok {
		scenario.recordCount = val
	}
	if val, ok := params["concurrent_writes"].(int); ok {
		scenario.concurrentWriters = val
	}
	if val, ok := params["flush_threshold"].(int); ok {
		scenario.flushThreshold = val
	}

	return scenario
}

func (s *MemtableStressScenario) Execute(ctx *core.TestContext) error {
	log.Printf("[%s] === Test 1: Sequential Writes (Flush Detection) === ", s.name)
	if err := s.testSequentialWrites(ctx); err != nil {
		return fmt.Errorf("sequantial writes failec: %w", err)
	}
	log.Printf("[%s] Sequential writes test passed", s.name)

	return nil
}

func (s *MemtableStressScenario) Name() string {
	return s.name
}

func (s *MemtableStressScenario) Setup(ctx *core.TestContext) error {

	log.Printf("[%s] Setting up memtable stress test", s.name)
	log.Printf("[%s] Configuration:", s.name)
	log.Printf("  - Record size: %d bytes", s.recordSize)
	log.Printf("  - Record count: %d", s.recordCount)
	log.Printf("  - Concurrent writers: %d", s.concurrentWriters)
	log.Printf("  - Expected flush threshold: %d bytes", s.flushThreshold)

	// Optional: Clean up any existing test data
	// This would require a Delete or Clear operation on your KV store
	// For now, we'll use unique key prefixes to avoid conflicts

	return nil

}

func (s *MemtableStressScenario) Teardown(ctx *core.TestContext) error {
	log.Printf("[%s] Tearing down memtable stress test", s.name)
	return nil
}

func (s *MemtableStressScenario) testSequentialWrites(ctx *core.TestContext) error {
	startTime := time.Now()
	lastReportTime := startTime

	var recordsSinceReport int

	// Track latency percentiles to detect flush spikes
	var latencyWindow []time.Duration
	const windowSize = 100

	for i := 0; i < s.recordCount; i++ {
		key := fmt.Sprintf("seq_key_%08d", i)
		value := ctx.GenerateValue(s.recordSize)

		// Measure individual write latency
		writeStart := time.Now()
		err := ctx.KV.Put(ctx.Context(), []byte(key), value)
		writeLatency := time.Since(writeStart)

		if err != nil {
			return fmt.Errorf("put failed at record %d: %w", i, err)
		}

		// Track latencies
		s.writeLatencies = append(s.writeLatencies, writeLatency)
		latencyWindow = append(latencyWindow, writeLatency)
		if len(latencyWindow) > windowSize {
			latencyWindow = latencyWindow[1:]
		}

		recordsSinceReport++

		// Record potential flush by latency spike
		if writeLatency > 50*time.Millisecond && !s.flushDetected {
			log.Printf("[%s] Potentional flush detected at record %d (latency: %v)",
				s.name, i, writeLatency)
			s.flushDetected = true
		}

		if i > 0 && i%1000 == 0 {
			elapsed := time.Since(lastReportTime)
			throughput := float64(recordsSinceReport) / elapsed.Seconds()
			avgLatency := s.calculateAverage(latencyWindow)
			p99Latency := s.calculateP99(latencyWindow)

			log.Printf("[%s] Progress: %d/%d records | Throughput: %.0f | Avg latency: %v | P99: %v",
				s.name, i, s.recordCount, throughput, avgLatency, p99Latency)

			lastReportTime = time.Now()
			recordsSinceReport = 0
		}
	}

	totalDuration := time.Since(startTime)
	overallThroughput := float64(s.recordCount) / totalDuration.Seconds()

	log.Printf("[%s] Sequential write test completed:", s.name)
	log.Printf("  - Total time: %v", totalDuration)
	log.Printf("  - Overall throughput: %.0f ops/s", overallThroughput)
	log.Printf("  - Flush detected: %v", s.flushDetected)

	return nil
}

func (s *MemtableStressScenario) calculateAverage(latencies []time.Duration) time.Duration {
	if len(latencies) == 0 {
		return 0
	}
	var sum time.Duration
	for _, lat := range latencies {
		sum += lat
	}
	return sum / time.Duration(len(latencies))
}

func (s *MemtableStressScenario) calculateP99(latencies []time.Duration) time.Duration {
	return s.calculatePercentile(latencies, 0.99)
}

func (s *MemtableStressScenario) calculatePercentile(latencies []time.Duration, percentile float64) time.Duration {
	if len(latencies) == 0 {
		return 0
	}

	sorted := make([]time.Duration, len(latencies))
	copy(sorted, latencies)
	slices.Sort(sorted)

	index := int(float64(len(sorted)) * percentile)
	if index >= len(sorted) {
		index = len(sorted) - 1
	}

	return sorted[index]
}

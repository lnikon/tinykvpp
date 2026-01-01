package core

import (
	"context"
	"fmt"
	"math/rand"
	"time"
)

type TestContext struct {
	KV KVStoreInterface
	// Metrics *metrics.MetricsCollector
	Config        *TestConfig
	ctx           context.Context
	cancel        context.CancelFunc
	VirtualUserID int
	TestPhase     string

	// Data tracking
	generatedKeys []string
	writtenData   map[string][]byte
}

func NewTestContext(adapter KVStoreInterface, config *TestConfig) *TestContext {
	ctx, cancel := context.WithCancel(context.Background())

	return &TestContext{
		KV: adapter,
		// Metrics: metrics.NewMetricsCollector(),
		Config:        config,
		ctx:           ctx,
		cancel:        cancel,
		TestPhase:     "init",
		generatedKeys: make([]string, 0),
		writtenData:   make(map[string][]byte),
	}
}

func (tc *TestContext) Context() context.Context {
	return tc.ctx
}

func (tc *TestContext) Cancel() {
	tc.cancel()
}

func (tc *TestContext) GenerateKey() string {
	key := fmt.Sprintf("test_%d_%d_%d",
		tc.VirtualUserID,
		time.Now().UnixNano(),
		rand.Intn(1000))
	tc.generatedKeys = append(tc.generatedKeys, key)
	return key
}

func (tc *TestContext) GenerateValue(size int) []byte {
	if size <= 0 {
		size = 256
	}

	value := make([]byte, size)
	for i := range value {
		value[i] = byte('A' + (i % 26))
	}
	return value
}

func (tc *TestContext) GetRandomExistingKey() string {
	if len(tc.generatedKeys) == 0 {
		return tc.GenerateKey()
	}

	keys := make([]string, 0, len(tc.generatedKeys))
	for key := range tc.writtenData {
		keys = append(keys, key)
	}

	return keys[rand.Intn(len(keys))]
}

func (tc *TestContext) RecordWrite(key string, value []byte) {
	tc.writtenData[key] = value
}

func (tc *TestContext) GetWrittenData() map[string][]byte {
	return tc.writtenData
}

func (tc *TestContext) Cleanup() error {
	return fmt.Errorf("delete not implemented")
	// TODO(lnikon): Uncomment once Delete() is implemented
	// for _, key := range tc.generatedKeys {
	// 	// tc.KV.Delete(tc.ctx, []byte(key))
	// }
}

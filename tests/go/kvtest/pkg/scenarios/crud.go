package scenarios

import (
	"bytes"
	"fmt"
	"log"
	"math/rand"

	// "time"

	"github.com/lnikon/tinykvpp/tests/go/kvtest/pkg/core"
)

// CRUDScenario implements basic CRUD operations
type CRUDScenario struct {
	name       string
	readRatio  float64
	writeRatio float64
	valueSize  int
}

// NewCRUDScenario creates a new CRUD scenario
func NewCRUDScenario(params map[string]interface{}) *CRUDScenario {
	scenario := &CRUDScenario{
		name:       "CRUD Operations",
		readRatio:  0.7,
		writeRatio: 0.3,
		valueSize:  256,
	}

	// Parse  parameters
	if val, ok := params["read_ratio"].(float64); ok {
		scenario.readRatio = val
	}
	if val, ok := params["write_ratio"].(float64); ok {
		scenario.writeRatio = val
	}
	if val, ok := params["value_size"].(int); ok {
		scenario.valueSize = val
	}

	return scenario
}

// Execute method runs the test scenario
func (s *CRUDScenario) Execute(ctx *core.TestContext) error {
	// startTime := time.Now()

	operation := s.selectOperation()
	log.Printf("executing %v", operation)

	var err error
	switch operation {
	case "read":
		err = s.executeRead(ctx)
	case "write":
		err = s.executeWrite(ctx)
	case "delete":
	case "exists":
		err = fmt.Errorf("operation not implemented")
	}

	// duration := time.Since(startTime)

	// TODO(lnikon): Implement metric collection
	// ctx.Metrics.RecordOperation(operation, duration, err)

	return err
}

func (s *CRUDScenario) Name() string {
	return s.name
}

func (s *CRUDScenario) Setup(ctx *core.TestContext) error {
	// Pre-populate some data for read operations
	for range 10 {
		key := ctx.GenerateKey()
		value := ctx.GenerateValue(s.valueSize)

		if err := ctx.KV.Put(ctx.Context(), []byte(key), value); err != nil {
			return fmt.Errorf("setup failed to put key %s: %w", key, err)
		}

		ctx.RecordWrite(key, value)
	}
	return nil
}

func (s *CRUDScenario) Teardown(ctx *core.TestContext) error {
	return ctx.Cleanup()
}

func (s *CRUDScenario) selectOperation() string {
	r := rand.Float64()

	if r < s.readRatio {
		return "read"
	} else if r < s.readRatio+s.writeRatio {
		return "write"
	} else if r < s.readRatio+s.writeRatio+0.1 {
		// TODO(lnikon): Should be "exists"
		// return "delete"
		return "delete"
	} else {
		// TODO(lnikon): Should be "exists"
		// return "exists"
		return "exists"
	}
}

func (s *CRUDScenario) executeRead(ctx *core.TestContext) error {
	key := ctx.GetRandomExistingKey()

	log.Printf("requesting existing key: %s", key)

	value, err := ctx.KV.Get(ctx.Context(), []byte(key))
	if err != nil {
		return err
	}

	writtenData := ctx.GetWrittenData()
	if expectedValue, exists := writtenData[key]; exists {
		if !bytes.Equal(value, expectedValue) {
			return fmt.Errorf("data consistency error: expected %v, got %v", expectedValue, value)
		}
	}

	return nil
}

func (s *CRUDScenario) executeWrite(ctx *core.TestContext) error {
	key := ctx.GenerateKey()
	value := ctx.GenerateValue(s.valueSize)

	err := ctx.KV.Put(ctx.Context(), []byte(key), value)
	if err == nil {
		ctx.RecordWrite(key, value)
	}

	return err
}

package scenarios

import (
	"bytes"
	"fmt"
	"time"

	"github.com/lnikon/kvtest/pkg/core"
)

type ReadYourWritesScenario struct {
	name          string
	valueSize     int
	numOperations int
	delay         time.Duration
}

func NewReadYourWrites(params map[string]interface{}) *ReadYourWritesScenario {
	scenario := &ReadYourWritesScenario{
		name:          "ReadYourWrites",
		valueSize:     256,
		numOperations: 1024,
		delay:         100 * time.Millisecond,
	}

	if val, ok := params["value_size"].(int); ok {
		scenario.valueSize = val
	}

	if val, ok := params["num_operations"].(int); ok {
		scenario.numOperations = val
	}

	if val, ok := params["delay"].(time.Duration); ok {
		scenario.delay = val
	}

	return scenario
}

func (s *ReadYourWritesScenario) Execute(ctx *core.TestContext) error {
	for i := 0; i < s.numOperations; i++ {
		key := ctx.GenerateKey()
		expectedValue := ctx.GenerateValue(s.valueSize)

		err := ctx.KV.Put(ctx.Context(), []byte(key), []byte(expectedValue))
		if err != nil {
			return err
		}

		value, err := ctx.KV.Get(ctx.Context(), []byte(key))
		if err != nil {
			return err
		}

		if !bytes.Equal(expectedValue, value) {
			return fmt.Errorf("data consistency error: expected %v, got %v", expectedValue, value)
		}

		time.Sleep(s.delay)
	}

	return nil
}

func (s *ReadYourWritesScenario) Name() string {
	return s.name
}

func (s *ReadYourWritesScenario) Setup(ctx *core.TestContext) error {
	return nil
}

func (s *ReadYourWritesScenario) Teardown(ctx *core.TestContext) error {
	return nil
}

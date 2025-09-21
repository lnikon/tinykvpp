package scenarios

import (
	"bytes"
	"fmt"
	"time"

	"github.com/lnikon/kvtest/pkg/core"
)

type WriteReadScenario struct {
	name          string
	valueSize     int
	numOperations int
	delay         time.Duration
}

func NewWriteReadScenario(params map[string]interface{}) *WriteReadScenario {
	scenario := &WriteReadScenario{
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

func (wr *WriteReadScenario) Execute(ctx *core.TestContext) error {
	for i := 0; i < wr.numOperations; i++ {
		key := ctx.GenerateKey()
		expectedValue := ctx.GenerateValue(wr.valueSize)

		err := ctx.KV.Put(ctx.Context(), []byte(key), []byte(expectedValue))
		if err != nil {
			return err
		}

		value, err := ctx.KV.Get(ctx.Context(), []byte(key))
		if err != nil {
			return nil
		}

		if !bytes.Equal(expectedValue, value) {
			return fmt.Errorf("data consistency error: expected %v, got %v", expectedValue, value)
		}

		time.Sleep(wr.delay)
	}

	return nil
}

func (wr *WriteReadScenario) Name() string {
	return wr.name
}

func (wr *WriteReadScenario) Setup(ctx *core.TestContext) error {
	return nil
}

func (wr *WriteReadScenario) Teardown(ctx *core.TestContext) error {
	return nil
}

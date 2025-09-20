package scenarios

import (
	"bytes"
	"fmt"

	"github.com/lnikon/tinykvpp/tests/go/kvtest/pkg/core"
)

type WriteReadScenario struct {
	name      string
	valueSize int
}

func NewWriteReadScenario(params map[string]interface{}) *WriteReadScenario {
	scenario := &WriteReadScenario{
		name:      "ReadYourWrites",
		valueSize: 256,
	}

	if val, ok := params["value_size"].(int); ok {
		scenario.valueSize = val
	}

	return scenario
}

func (wr *WriteReadScenario) Execute(ctx *core.TestContext) error {
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

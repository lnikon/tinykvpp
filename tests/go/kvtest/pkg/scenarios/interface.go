package scenarios

import (
	"github.com/lnikon/tinykvpp/tests/go/kvtest/pkg/core"
)

// Scenario defines the interface all test scenarios must implement
type Scenario interface {
	Execute(ctx *core.TestContext) error

	Name() string

	Setup(ctx *core.TestContext) error

	Teardown(ctx *core.TestContext) error
}

// WeightedScenario used for load testing
type WeightedScenario struct {
	Scenario Scenario
	Weight   int
}

// ScenarioFactory creates scenarios based on configuration
type ScenarioFactory struct{}

func NewScenarioFactory() *ScenarioFactory {
	return &ScenarioFactory{}
}

func (f *ScenarioFactory) Create(config core.ScenarioConfig) (Scenario, error) {
	switch config.Type {
	case "crud":
		return NewCRUDScenario(config.Parameters), nil
	default:
		return NewCRUDScenario(config.Parameters), nil
	}
}

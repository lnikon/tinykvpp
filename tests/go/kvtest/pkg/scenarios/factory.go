package scenarios

import "github.com/lnikon/kvtest/pkg/core"

// ScenarioFactory creates scenarios based on configuration
type ScenarioFactory struct{}

func NewScenarioFactory() *ScenarioFactory {
	return &ScenarioFactory{}
}

func (f *ScenarioFactory) Create(config core.ScenarioConfig) (core.Scenario, error) {
	switch config.Type {
	case "crud":
		return NewCRUDScenario(config.Parameters), nil
	case "write_read":
		return NewWriteReadScenario(config.Parameters), nil
	default:
		return NewCRUDScenario(config.Parameters), nil
	}
}

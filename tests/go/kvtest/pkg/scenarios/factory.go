package scenarios

import (
	"fmt"

	"github.com/lnikon/kvtest/pkg/core"
	"github.com/lnikon/kvtest/pkg/scenarios/storage"
)

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
		return NewReadYourWrites(config.Parameters), nil
	case "memtable_stress":
		return storage.NewMemtableStressScenario(config.Parameters), nil
	default:
		return nil, fmt.Errorf("unkown scenario: %s", config.Type)
	}
}

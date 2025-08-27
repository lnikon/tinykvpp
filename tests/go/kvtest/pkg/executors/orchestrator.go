package executors

import (
	"fmt"
	"log"

	"github.com/lnikon/tinykvpp/tests/go/kvtest/pkg/core"
	"github.com/lnikon/tinykvpp/tests/go/kvtest/pkg/scenarios"
)

type TestOrchestrator struct {
	kvstore core.KVStoreInterface
	config  *core.TestConfig
	factory *scenarios.ScenarioFactory
}

func NewTestOrchestrator(kvstore core.KVStoreInterface, config *core.TestConfig) *TestOrchestrator {
	return &TestOrchestrator{
		kvstore: kvstore,
		config:  config,
		factory: scenarios.NewScenarioFactory(),
	}
}

func (o *TestOrchestrator) RunComplete() (*CompleteTestResult, error) {
	result := &CompleteTestResult{}

	if o.config.Integration.Enabled {
		log.Println("=== Phase 1: Integration Testing ===")

		integrationResult, err := o.runIntegrationTests()
		if err != nil {
			return nil, fmt.Errorf("integration tests failed: %w", err)
		}

		result.Integration = integrationResult

		if !integrationResult.AllPassed() {
			log.Printf("integration tests failed (%d/%d passed)",
				result.Integration.Passed, result.Integration.Total)
			return result, nil
		}

		log.Printf("integration tests completed successfully (%d/%d passed)",
			result.Integration.Passed, result.Integration.Total)
	}

	return result, nil
}

func (o *TestOrchestrator) runIntegrationTests() (*core.IntegrationTestResult, error) {
	executor := core.NewIntegrationTestExecutor(o.kvstore, o.config)

	for _, scenariConfig := range o.config.Scenarios {
		if scenariConfig.Enabled {
			scenario, err := o.factory.Create(scenariConfig)
			if err != nil {
				return nil, fmt.Errorf("failed to create scenario %s: %w", scenariConfig.Name, err)
			}
			executor.AddScenario(scenario)
		}
	}

	return executor.Execute()
}

type CompleteTestResult struct {
	Integration *core.IntegrationTestResult
}

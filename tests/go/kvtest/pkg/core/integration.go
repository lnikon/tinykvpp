package core

import (
	"fmt"
	"log"
)

type IntegrationTestExecutor struct {
	kvstore   KVStoreInterface
	scenarios []Scenario
	config    *TestConfig
}

func NewIntegrationTestExecutor(kvstore KVStoreInterface, config *TestConfig) *IntegrationTestExecutor {
	return &IntegrationTestExecutor{
		kvstore: kvstore,
		config:  config,
	}
}

func (e *IntegrationTestExecutor) AddScenario(scenario Scenario) {
	e.scenarios = append(e.scenarios, scenario)
}

func (e *IntegrationTestExecutor) Execute() (*IntegrationTestResult, error) {
	result := &IntegrationTestResult{
		Results: make(map[string]*ScenarioResult),
	}

	log.Printf("Starting integration tests with %d scenarios", len(e.scenarios))

	for _, scenario := range e.scenarios {
		log.Printf("Running scenario: %s", scenario.Name())

		ctx := NewTestContext(e.kvstore, e.config)
		ctx.TestPhase = "integration"

		scenarioResult := &ScenarioResult{
			Name: scenario.Name(),
		}

		// Setup phase
		if err := scenario.Setup(ctx); err != nil {
			scenarioResult.Error = fmt.Errorf("setup Failed: %w", err)
			result.Results[scenario.Name()] = scenarioResult
			result.Failed++
			continue
		}

		// Execute phase
		if err := scenario.Execute(ctx); err != nil {
			scenarioResult.Error = fmt.Errorf("execute failed: %w", err)
			result.Results[scenario.Name()] = scenarioResult
			result.Failed++
			continue
		} else {
			scenarioResult.Success = true
			result.Results[scenario.Name()] = scenarioResult
			result.Passed++
		}

		// Teardown phase
		if err := scenario.Teardown(ctx); err != nil {
			log.Printf("Warning: teardown failed for %s: %v", scenario.Name(), err)
		}

		// Stop metrics collection
		// TODO(lnikon): Implement MetricsSummary
		// ctx.Metrics.Stop()
		// scenarioResult.Metrics = ctx.Metrics.MetricsSummary
	}

	result.Total = len(e.scenarios)
	return result, nil
}

type IntegrationTestResult struct {
	Total   int
	Passed  int
	Failed  int
	Results map[string]*ScenarioResult
}

type ScenarioResult struct {
	Name    string
	Success bool
	Error   error
	// TODO(lnikon): Implement MetricsSummary
	// Metrics *MetricsSummary
}

func (r *IntegrationTestResult) AllPassed() bool {
	return r.Failed == 0
}

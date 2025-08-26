package core

// Scenario defines the interface all test scenarios must implement
type Scenario interface {
	Execute(ctx *TestContext) error

	Name() string

	Setup(ctx *TestContext) error

	Teardown(ctx *TestContext) error
}

// WeightedScenario used for load testing
type WeightedScenario struct {
	Scenario Scenario
	Weight   int
}

package core

import (
	"time"
)

type TestConfig struct {
	// General settings
	Timeout  time.Duration `yaml:"timeout"`
	LogLevel string        `yaml:"log_level"`

	// KV Store configuration
	Adapter AdapterConfig `yaml:"adapter"`

	// Test execution settings
	Integration IntegrationConfig `yaml:"integration"`
	Load        LoadConfig        `yaml:"load"`

	// Scenario configurations
	Scenarios []ScenarioConfig `yaml:"scenarios"`
}

// AdapterConfig specified which KV store adapter to use
type AdapterConfig struct {
	Type              string        `yaml:"type"`
	Address           string        `yaml:"address"`
	ConnectionTimeout time.Duration `yaml:"connection_timeout"`
	RequestTimeout    time.Duration `yaml:"request_timeout"`
	MaxRetries        int           `yaml:"max_retries"`
}

// IntegrationConfig settings for integration tests
type IntegrationConfig struct {
	Enabled     bool     `yaml:"enabled"`
	Validations []string `yaml:"validations"`
	Parallel    bool     `yaml:"parallel"`
}

// LoadConfig settings for load tests
type LoadConfig struct {
	Enabled     bool           `yaml:"enabled"`
	Algorithm   string         `yaml:"algorithm"`
	StartUsers  int            `yaml:"start_users"`
	TargetUsers int            `yaml:"target_users"`
	Duration    time.Duration  `yaml:"durartion"`
	RampUpTime  time.Duration  `yaml:"ramp_up_time"`
	ThinkTime   time.Duration  `yaml:"think_time"`
	Weights     map[string]int `yaml:"weights"`
}

// ScenarioConfig defines test scenario parameters
type ScenarioConfig struct {
	Name       string                 `yaml:"name"`
	Type       string                 `yaml:"type"`
	Enabled    bool                   `yaml:"enabled"`
	Parameters map[string]interface{} `yaml:"parameters"`
}

// DefaultConfig returns a default configuration
func DefaultConfig() *TestConfig {
	return &TestConfig{
		Timeout:  30 * time.Second,
		LogLevel: "info",

		Adapter: AdapterConfig{
			Type:              "tinykvpp",
			Address:           "localhost:9891",
			ConnectionTimeout: 5 * time.Second,
			RequestTimeout:    1 * time.Second,
			MaxRetries:        3,
		},

		Integration: IntegrationConfig{
			Enabled:     true,
			Validations: []string{"consistency", "isolation"},
			Parallel:    false,
		},

		Load: LoadConfig{
			Enabled:     true,
			Algorithm:   "constant",
			StartUsers:  1,
			TargetUsers: 10,
			Duration:    60 * time.Second,
			ThinkTime:   100 * time.Microsecond,
		},

		Scenarios: []ScenarioConfig{
			{
				Name:    "basic_crud",
				Type:    "crud",
				Enabled: true,
				Parameters: map[string]interface{}{
					"read_ratio":  0.7,
					"write_ratio": 0.3,
				},
			},
		},
	}
}

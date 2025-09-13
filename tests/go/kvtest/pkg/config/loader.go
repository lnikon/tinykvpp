package config

import (
	"fmt"
	"os"

	"gopkg.in/yaml.v3"

	"github.com/lnikon/tinykvpp/tests/go/kvtest/pkg/core"
)

// LoadFromFile loads configuration from a YAML file
func LoadFromFile(filename string) (*core.TestConfig, error) {
	data, err := os.ReadFile(filename)
	if err != nil {
		return nil, fmt.Errorf("failed to read config file: %w", err)
	}

	config := core.DefaultConfig()
	if err := yaml.Unmarshal(data, config); err != nil {
		return nil, fmt.Errorf("failed to parse config file: %w", err)
	}

	return config, nil
}

// SaveToFile saves configuration to a YAML file
func SaveToFile(config *core.TestConfig, filename string) error {
	data, err := yaml.Marshal(config)
	if err != nil {
		return fmt.Errorf("failed to marshal config: %w", err)
	}

	if err := os.WriteFile(filename, data, 0644); err != nil {
		return fmt.Errorf("failed to write config file: %w", err)
	}

	return nil
}

func Validate(config *core.TestConfig) error {
	// Validate adapter configuration
	if config.Adapter.Type == "" {
		return fmt.Errorf("adapter.type is required")
	}

	if config.Adapter.Address == "" {
		return fmt.Errorf("adapter.address is required")
	}

	if config.Adapter.ConnectionTimeout <= 0 {
		return fmt.Errorf("adapter.connection_timeout must be positive")
	}

	if config.Adapter.RequestTimeout <= 0 {
		return fmt.Errorf("adapter.request_timeout must be positive")
	}

	// Validate scenarios
	if len(config.Scenarios) == 0 {
		return fmt.Errorf("at least one scenario must be present")
	}

	enabledScenarios := 0
	for i, scenario := range config.Scenarios {
		if scenario.Name == "" {
			return fmt.Errorf("scenario[%d].name is required", i)
		}

		if scenario.Type == "" {
			return fmt.Errorf("scenario[%d].type is required", i)
		}

		if scenario.Enabled {
			enabledScenarios++
		}
	}

	if enabledScenarios == 0 {
		return fmt.Errorf("at least one scenario must be enabled")
	}

	return nil
}

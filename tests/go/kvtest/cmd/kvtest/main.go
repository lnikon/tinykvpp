package main

import (
	"fmt"
	"log"
	"os"

	"github.com/lnikon/kvtest/pkg/adapters"
	"github.com/lnikon/kvtest/pkg/config"
	"github.com/lnikon/kvtest/pkg/core"
	"github.com/lnikon/kvtest/pkg/scenarios"
	"github.com/spf13/cobra"
)

var (
	configFile  string
	adapterType string
	address     string
	verbose     bool
)

func main() {
	if err := rootCmd.Execute(); err != nil {
		log.Fatal(err)
	}
}

var rootCmd = &cobra.Command{
	Use:   "kvtest",
	Short: "A simple testing framework for key-value stores",
	Long: `KVTest is a simple testing framework for key-value stores that supports
integration testing with basic CRUD operations.`,
	Version: "0.0.1",
}

var runCmd = &cobra.Command{
	Use:   "run",
	Short: "Run integration tests",
	RunE:  runTests,
}

var validateCmd = &cobra.Command{
	Use:   "validate",
	Short: "Validate configuration",
	RunE:  validateConfig,
}

var generateConfigCmd = &cobra.Command{
	Use:   "generate [output-file]",
	Short: "Generate sample configration",
	Args:  cobra.MaximumNArgs(1),
	RunE:  generateConfig,
}

func init() {
	// Global flags
	rootCmd.PersistentFlags().StringVarP(&configFile, "config", "c", "", "Configuration file path")
	rootCmd.PersistentFlags().StringVarP(&adapterType, "adapter", "a", "tinykvpp", "KV store adapter type")
	rootCmd.PersistentFlags().StringVar(&address, "address", "localhost:9891", "KV store address")
	rootCmd.PersistentFlags().BoolVarP(&verbose, "verbose", "v", false, "Enable verbose logging")

	// Add subcommands
	rootCmd.AddCommand(runCmd)
	rootCmd.AddCommand(validateCmd)
	rootCmd.AddCommand(generateConfigCmd)
}

func runTests(cmd *cobra.Command, args []string) error {
	// Load configuration
	cfg, err := loadConfiguration()
	if err != nil {
		return fmt.Errorf("failed to load configuration: %w", err)
	}

	// Override with command line flags if provided
	if adapterType != "tinykvpp" {
		cfg.Adapter.Type = adapterType
	}
	if address != "localhost:9891" {
		cfg.Adapter.Address = address
	}

	// Validate configuration
	if err := config.Validate(cfg); err != nil {
		return fmt.Errorf("configuration validation failed: %w", err)
	}

	// Create KV store adapter
	kvstore, err := createAdapter(cfg)
	if err != nil {
		return fmt.Errorf("failed to create adapter: %w", err)
	}
	defer kvstore.Close()

	// Connect to the KV store
	if err := kvstore.Connect(createAdapterConfig(cfg)); err != nil {
		return fmt.Errorf("failed to connect to KV store: %w", err)
	}

	// Create scenario factory
	factory := scenarios.NewScenarioFactory()

	// Create integration test executor
	executor := core.NewIntegrationTestExecutor(kvstore, cfg)

	// Add scenarios from configuration
	for _, scenarioCfg := range cfg.Scenarios {
		if !scenarioCfg.Enabled {
			continue
		}

		scenario, err := factory.Create(scenarioCfg)
		if err != nil {
			return fmt.Errorf("failed to create scenario %s: %w", scenarioCfg.Name, err)
		}

		executor.AddScenario(scenario)
	}

	// Execute tests
	log.Printf("Starting integration testing...")
	result, err := executor.Execute()
	if err != nil {
		return fmt.Errorf("test execution failed: %w", err)
	}

	// Print results
	printResults(result)

	if !result.AllPassed() {
		os.Exit(1)
	}

	return nil
}

func validateConfig(cmd *cobra.Command, args []string) error { return nil }

func generateConfig(cmd *cobra.Command, args []string) error { return nil }

func loadConfiguration() (*core.TestConfig, error) {
	if configFile != "" {
		return config.LoadFromFile(configFile)
	}

	// Try to load from default locations
	defaultFiles := []string{"kvtest.yaml", "kvtest.yml", ".kvtest.yaml"}

	for _, file := range defaultFiles {
		if _, err := os.Stat(file); err == nil {
			return config.LoadFromFile(file)
		}
	}

	// Return default configuration if no file found
	return core.DefaultConfig(), nil
}

func createAdapter(cfg *core.TestConfig) (core.KVStoreInterface, error) {
	switch cfg.Adapter.Type {
	case "tinykvpp":
		return adapters.NewTinyKVPPAdapter(), nil
	default:
		return nil, fmt.Errorf("unsupported adapter type: %s", cfg.Adapter.Type)
	}
}

func createAdapterConfig(cfg *core.TestConfig) core.Config {
	switch cfg.Adapter.Type {
	case "tinykvpp":
		return adapters.TinyKVPPConfig{
			Address:           cfg.Adapter.Address,
			ConnectionTimeout: cfg.Adapter.ConnectionTimeout,
			RequestTimeout:    cfg.Adapter.RequestTimeout,
			MaxRetries:        cfg.Adapter.MaxRetries,
		}
	default:
		return nil
	}
}

func printResults(result *core.IntegrationTestResult) {
	fmt.Printf("\n=== Integration Test Results ===\n")
	fmt.Printf("Total scenarios: %d\n", result.Total)
	fmt.Printf("Passed: %d\n", result.Passed)
	fmt.Printf("Failed: %d\n", result.Failed)
	fmt.Printf("Success rate: %.1f%%\n", float64(result.Passed)/float64(result.Total)*100)

	if len(result.Results) > 0 {
		fmt.Printf("\n=== Scenario Details ===\n")
		for name, scenarioResult := range result.Results {
			status := "PASS"
			if !scenarioResult.Success {
				status = "FAIL"
			}

			fmt.Printf("[%s] %s", status, name)
			if scenarioResult.Error != nil {
				fmt.Printf(" - %v", scenarioResult.Error)
			}
			fmt.Println()
		}
	}

	fmt.Printf("\n=== Summary ===\n")
	if result.AllPassed() {
		fmt.Println("✅ All tests passed!")
	} else {
		fmt.Printf("❌ %d test(s) failed\n", result.Failed)
	}
}

package core

import (
	"context"
)

// Config for KVStore adapters
type Config interface{}

type KVStoreInterface interface {
	// Connection management
	Connect(config Config) error
	Close() error

	// CRUD operations
	Get(ctx context.Context, key []byte) ([]byte, error)
	Put(ctx context.Context, key, value []byte) error

	// Testing utilities
	HealthCheck(ctx context.Context) error
	Name() string
}

// KVError represents kvtest specific error
type KVError struct {
	Op  string
	Err error
}

func (e *KVError) Error() string {
	return e.Op + ": " + e.Err.Error()
}

// Predefined errors
var (
	ErrKeyNotFound  = &KVError{Op: "get", Err: context.DeadlineExceeded}
	ErrPutFailed    = &KVError{Op: "put", Err: context.Canceled}
	ErrDeleteFailed = &KVError{Op: "delete", Err: context.Canceled}
)

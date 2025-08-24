package adapters

import (
	"context"
	"fmt"
	"time"

	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"

	"github.com/lnikon/tinykvpp/tests/go/kvtest/pkg/core"
	pb "github.com/lnikon/tinykvpp/tests/go/kvtest/proto/tinykvpp/v1"
)

type TinyKVPPConfig struct {
	Address           string        `yaml:"address"`
	ConnectionTimeout time.Duration `yaml:"connection_timeout"`
	RequestTimeout    time.Duration `yaml:"request_timeout"`
	MaxRetries        int           `yaml:"max_retries"`
}

type TinyKVPPAdapter struct {
	config TinyKVPPConfig
	conn   *grpc.ClientConn
	client pb.TinyKVPPServiceClient
}

var _ core.KVStoreInterface = (*TinyKVPPAdapter)(nil)

func NewTinyKVPPAdapter(config core.Config) *TinyKVPPAdapter {
	return &TinyKVPPAdapter{}
}

func (a *TinyKVPPAdapter) Connect(config core.Config) error {
	tinyConfig, ok := config.(TinyKVPPConfig)
	if !ok {
		return fmt.Errorf("invalid config type for TinyKVPP adapter")
	}

	a.config = tinyConfig

	conn, err := grpc.NewClient(
		a.config.Address, grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		return fmt.Errorf("failed to connect to %s: %w", a.config.Address, err)
	}

	a.conn = conn
	a.client = pb.NewTinyKVPPServiceClient(conn)

	return a.HealthCheck(context.Background())
}

func (a *TinyKVPPAdapter) Close() error {
	if a.conn != nil {
		return a.conn.Close()
	}
	return nil
}

func (a *TinyKVPPAdapter) Get(ctx context.Context, key []byte) ([]byte, error) {
	if a.conn == nil {
		return nil, fmt.Errorf("not connected to TinyKVPP")
	}

	// Apply requet timeout
	ctx, cancel := context.WithTimeout(ctx, a.config.RequestTimeout)
	defer cancel()

	req := &pb.GetRequest{Key: string(key)}
	resp, err := a.client.Get(ctx, req)
	if err != nil {
		return nil, fmt.Errorf("grpc get failed for key %s: %v", key, err)
	}

	if !resp.Found {
		return nil, &core.KVError{Op: "get", Err: fmt.Errorf("key not found: %s", string(key))}
	}

	return []byte(resp.Value), nil
}

func (a *TinyKVPPAdapter) Put(ctx context.Context, key, value []byte) error {
	if a.conn == nil {
		return fmt.Errorf("not connected to TinyKVPP")
	}

	// Apply requet timeout
	ctx, cancel := context.WithTimeout(ctx, a.config.RequestTimeout)
	defer cancel()

	req := &pb.PutRequest{Key: string(key), Value: string(value)}
	resp, err := a.client.Put(ctx, req)
	if err != nil {
		return fmt.Errorf("grpc put failed for key %s: %v\n", key, err)
	}

	if len(resp.Error) > 0 {
		return &core.KVError{Op: "put", Err: fmt.Errorf("put failed: %s", resp.Error)}
	}

	if !resp.Success {
		return &core.KVError{Op: "put", Err: fmt.Errorf("put operation unsuccessful")}
	}

	return nil
}

func (a *TinyKVPPAdapter) HealthCheck(ctx context.Context) error {
	if a.conn == nil {
		return fmt.Errorf("not connected")
	}

	testKey := "__health_check__"
	a.client.Get(ctx, &pb.GetRequest{Key: testKey})

	return nil
}

func (a *TinyKVPPAdapter) Name() string {
	return "TinyKVPP"
}

package core

import (
	"context"
)

type TestContext struct {
	KVStore KVStoreInterface

	ctx context.Context
}

func NewTestContext(adapter KVStoreInterface) TestContext {
	return TestContext{
		KVStore: adapter,
		ctx:     context.Background(),
	}
}

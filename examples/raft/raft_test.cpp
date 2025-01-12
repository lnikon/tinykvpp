#include "gmock/gmock.h"
#include <catch2/catch_test_macros.hpp>

#include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/support/status.h>

#include <gtest/gtest.h>
#include <memory>

#include "raft.h"
#include "Raft_mock.grpc.pb.h"

TEST_CASE("NodeClient AppendEntries", "[NodeClient]")
{
    raft::node_config_t nodeConfig{.m_id = 1, .m_ip = "0.0.0.0:9090"};

    auto mockStub = std::make_unique<MockRaftServiceStub>();

    AppendEntriesResponse response;
    EXPECT_CALL(*mockStub, AppendEntries)
        .Times(testing::AtLeast(2))
        .WillOnce(testing::DoAll(testing::SetArgPointee<2>(response), testing::Return(grpc::Status::OK)))
        .WillOnce(testing::DoAll(testing::SetArgPointee<2>(response), testing::Return(grpc::Status::CANCELLED)));

    raft::node_client_t nodeClient{nodeConfig, std::move(mockStub)};

    AppendEntriesRequest request;
    EXPECT_EQ(nodeClient.appendEntries(request, &response), true);
    EXPECT_EQ(nodeClient.appendEntries(request, &response), false);
}

TEST_CASE("ConsensusModule Initialization", "[ConsensusModule]")
{
}

TEST_CASE("ConsensusModule Leader Election", "[ConsensusModule]")
{
}

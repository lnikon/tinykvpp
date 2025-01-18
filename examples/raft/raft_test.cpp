#include "gmock/gmock.h"
#include <catch2/catch_test_macros.hpp>

#include <chrono>
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
#include <spdlog/spdlog.h>
#include <thread>
#include <utility>

#include "raft.h"
#include "Raft_mock.grpc.pb.h"

// AppendEntries RPC returns OK - NodeClient::appendEntries returns true
// AppendEntries RPC returns CANCELLED - NodeClient::appendEntries returns false
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
    spdlog::set_level(spdlog::level::debug);

    // Leader node
    raft::node_config_t nodeConfig1{.m_id = 1, .m_ip = "0.0.0.0:9090"};

    // Follower #1
    raft::node_config_t nodeConfig2{.m_id = 2, .m_ip = "0.0.0.0:9091"};
    auto                mockStub2 = std::make_unique<MockRaftServiceStub>();

    // Follower #2
    raft::node_config_t nodeConfig3{.m_id = 3, .m_ip = "0.0.0.0:9092"};
    auto                mockStub3 = std::make_unique<MockRaftServiceStub>();

    RequestVoteResponse response;
    response.set_votegranted(1);
    response.set_responderid(2);
    response.set_term(1);
    EXPECT_CALL(*mockStub2, RequestVote)
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::DoAll(testing::SetArgPointee<2>(response), testing::Return(grpc::Status::OK)));

    response.set_responderid(3);
    EXPECT_CALL(*mockStub3, RequestVote)
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::DoAll(testing::SetArgPointee<2>(response), testing::Return(grpc::Status::OK)));

    AppendEntriesResponse aeResponse;
    aeResponse.set_responderid(2);
    aeResponse.set_success(true);
    EXPECT_CALL(*mockStub2, AppendEntries)
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::DoAll(testing::SetArgPointee<2>(aeResponse), testing::Return(grpc::Status::OK)));

    aeResponse.set_responderid(3);
    EXPECT_CALL(*mockStub3, AppendEntries)
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::DoAll(testing::SetArgPointee<2>(aeResponse), testing::Return(grpc::Status::OK)));

    std::vector<raft::node_client_t> replicas;
    replicas.emplace_back(nodeConfig2, std::move(mockStub2));
    replicas.emplace_back(nodeConfig3, std::move(mockStub3));

    raft::consensus_module_t consensusModule{nodeConfig1, std::move(replicas)};
    consensusModule.start();

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    consensusModule.stop();
}

TEST_CASE("ConsensusModule Leader Election", "[ConsensusModule]")
{
}

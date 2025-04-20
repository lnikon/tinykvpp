#include <algorithm>
#include <fmt/core.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/support/status.h>

#include <memory>
#include <ranges>

#include <spdlog/spdlog.h>
#include <thread>
#include <utility>

#include "raft.h"
#include "Raft_mock.grpc.pb.h"
#include "TinyKVPP_mock.grpc.pb.h"

using namespace std::chrono_literals;

TEST(NodeClient, AppendEntries)
{
    raft::node_config_t nodeConfig{.m_id = 1, .m_ip = "0.0.0.0:9090"};

    auto mockStub = std::make_unique<MockRaftServiceStub>();

    AppendEntriesResponse response;
    EXPECT_CALL(*mockStub, AppendEntries)
        .Times(testing::AtLeast(2))
        .WillOnce(
            testing::DoAll(testing::SetArgPointee<2>(response), testing::Return(grpc::Status::OK)))
        .WillOnce(testing::DoAll(testing::SetArgPointee<2>(response),
                                 testing::Return(grpc::Status::CANCELLED)));

    raft::raft_node_grpc_client_t nodeClient{nodeConfig, std::move(mockStub)};

    AppendEntriesRequest request;
    EXPECT_EQ(nodeClient.appendEntries(request, &response), true);
    EXPECT_EQ(nodeClient.appendEntries(request, &response), false);
}

class ConsensusModuleTest : public testing::Test
{
  protected:
    ConsensusModuleTest()
    {
        for (id_t id{1}; id <= clusterSize; id++)
        {
            m_configs[id] =
                raft::node_config_t{.m_id = id, .m_ip = fmt::format("0.0.0.0:909{}", id)};
        }
    }

    void SetUp() override
    {
        for (id_t id{2}; id <= clusterSize; id++)
        {
            m_raftStubs[id] = new MockRaftServiceStub;

            m_raftClients.emplace(
                id,
                raft::raft_node_grpc_client_t(
                    m_configs[id], std::unique_ptr<MockRaftServiceStub>(m_raftStubs[id])));
        }
    }

    void TearDown() override
    {
        m_raftStubs.clear();
        m_raftClients.clear();
    }

    const std::uint32_t clusterSize{5};

    std::unordered_map<id_t, raft::node_config_t>           m_configs;
    std::unordered_map<id_t, MockRaftServiceStub *>         m_raftStubs;
    std::unordered_map<id_t, raft::raft_node_grpc_client_t> m_raftClients;

    auto raftClients()
    {
        return m_raftClients |
               std::views::transform([](auto &&pair) { return std::move(pair.second); }) |
               std::ranges::to<std::vector<raft::raft_node_grpc_client_t>>();
    }
};

// Node1 will become a leader during first term if majority of peers grant a
// vote to it Node1:
// * Wait for heartbeat
// * Timeout
// * Increment current term
// * Start leader election
// * Start leader election
// * Achieve quorum
// * Become a leader
// * Start to send heartbeats
TEST_F(ConsensusModuleTest, Initialization1)
{
    std::vector<RequestVoteResponse> rvResponses;
    rvResponses.reserve(clusterSize);
    std::vector<AppendEntriesResponse> aeResponses;
    for (id_t id{2}; id <= clusterSize; id++)
    {
        RequestVoteResponse &response = rvResponses.emplace_back();
        response.set_votegranted(1);
        response.set_responderid(id);
        response.set_term(1);
        EXPECT_CALL(*m_raftStubs[id], RequestVote)
            .Times(testing::Exactly(1))
            .WillOnce(testing::DoAll(testing::SetArgPointee<2>(response),
                                     testing::Return(grpc::Status::OK)));

        AppendEntriesResponse &aeResponse = aeResponses.emplace_back();
        aeResponse.set_responderid(id);
        aeResponse.set_success(true);
        EXPECT_CALL(*m_raftStubs[id], AppendEntries)
            .Times(testing::AtLeast(1))
            .WillRepeatedly(testing::DoAll(testing::SetArgPointee<2>(aeResponse),
                                           testing::Return(grpc::Status::OK)));
    }

    raft::consensus_module_t consensusModule{m_configs[1], raftClients()};
    consensusModule.start();
    std::this_thread::sleep_for(1000ms);
    consensusModule.stop();
}

// Difference between this test and Initialization1 is that in this scenario
// Node1 fails to achieve quorum on first try Node1:
// * Wait for heartbeat
// * Timeout
// * Increment current term
// * Start leader election
// * Fail to achieve quorum
// * Wait for heartbeat
// * Timeout
// * Increment current term
// * Start leader election
// * Achieve quorum
// * Become a leader
// * Start to send heartbeats
TEST_F(ConsensusModuleTest, Initialization2)
{
    std::vector<RequestVoteResponse> rvResponses;
    rvResponses.reserve(clusterSize * 2);
    std::vector<AppendEntriesResponse> aeResponses;
    for (id_t id{2}; id <= clusterSize; id++)
    {
        RequestVoteResponse &failedResponse = rvResponses.emplace_back();
        failedResponse.set_votegranted(0);
        failedResponse.set_responderid(id);
        failedResponse.set_term(1);

        RequestVoteResponse &response = rvResponses.emplace_back();
        response.set_votegranted(1);
        response.set_responderid(id);
        response.set_term(2);
        EXPECT_CALL(*m_raftStubs[id], RequestVote)
            .Times(testing::Exactly(2))
            .WillOnce(testing::DoAll(testing::SetArgPointee<2>(failedResponse),
                                     testing::Return(grpc::Status::OK)))
            .WillOnce(testing::DoAll(testing::SetArgPointee<2>(response),
                                     testing::Return(grpc::Status::OK)));

        AppendEntriesResponse &aeResponse = aeResponses.emplace_back();
        aeResponse.set_responderid(id);
        aeResponse.set_success(true);
        EXPECT_CALL(*m_raftStubs[id], AppendEntries)
            .Times(testing::AtLeast(1))
            .WillRepeatedly(testing::DoAll(testing::SetArgPointee<2>(aeResponse),
                                           testing::Return(grpc::Status::OK)));
    }

    raft::consensus_module_t consensusModule{m_configs[1], raftClients()};
    consensusModule.start();
    std::this_thread::sleep_for(1000ms);
    consensusModule.stop();
}

int main(int argc, char **argv)
{
    spdlog::set_level(spdlog::level::debug);

    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

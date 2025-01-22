#include <algorithm>
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

// AppendEntries RPC returns OK - NodeClient::appendEntries returns true
// AppendEntries RPC returns CANCELLED - NodeClient::appendEntries returns false
TEST(NodeClient, AppendEntries)
{
    raft::node_config_t nodeConfig{.m_id = 1, .m_ip = "0.0.0.0:9090"};

    auto mockStub = std::make_unique<MockRaftServiceStub>();

    AppendEntriesResponse response;
    EXPECT_CALL(*mockStub, AppendEntries)
        .Times(testing::AtLeast(2))
        .WillOnce(testing::DoAll(testing::SetArgPointee<2>(response), testing::Return(grpc::Status::OK)))
        .WillOnce(testing::DoAll(testing::SetArgPointee<2>(response), testing::Return(grpc::Status::CANCELLED)));

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
        m_configs[1] = raft::node_config_t{.m_id = 1, .m_ip = "0.0.0.0:9090"};
        m_configs[2] = raft::node_config_t{.m_id = 2, .m_ip = "0.0.0.0:9091"};
        m_configs[3] = raft::node_config_t{.m_id = 3, .m_ip = "0.0.0.0:9092"};
    }

    void SetUp() override
    {
        for (id_t id{2}; id <= clusterSize; id++)
        {
            m_raftStubs[id] = new MockRaftServiceStub;

            m_raftClients.emplace(
                id,
                raft::raft_node_grpc_client_t(m_configs[id], std::unique_ptr<MockRaftServiceStub>(m_raftStubs[id])));

            m_tkvClients.emplace(
                id, raft::tkvpp_node_grpc_client_t(m_configs[id], std::make_unique<MockTinyKVPPServiceStub>()));
        }
    }

    void TearDown() override
    {
        m_raftStubs.clear();
        m_raftClients.clear();
        m_tkvClients.clear();
    }

    const std::uint32_t clusterSize{3};

    std::unordered_map<id_t, raft::node_config_t> m_configs;

    std::unordered_map<id_t, MockRaftServiceStub *>         m_raftStubs;
    std::unordered_map<id_t, raft::raft_node_grpc_client_t> m_raftClients;

    std::unordered_map<id_t, std::unique_ptr<MockTinyKVPPServiceStub>> m_tkvStubs;
    std::unordered_map<id_t, raft::tkvpp_node_grpc_client_t>           m_tkvClients;

    auto raftClients()
    {
        return m_raftClients | std::views::transform([](auto &&pair) { return std::move(pair.second); }) |
               std::ranges::to<std::vector<raft::raft_node_grpc_client_t>>();
    }

    auto tkvClients()
    {
        return m_tkvClients | std::views::transform([](auto &&pair) { return std::move(pair.second); }) |
               std::ranges::to<std::vector<raft::tkvpp_node_grpc_client_t>>();
    }
};

TEST_F(ConsensusModuleTest, Initialization)
{
    std::vector<RequestVoteResponse>   rvResponses;
    std::vector<AppendEntriesResponse> aeResponses;
    for (id_t id{2}; id <= clusterSize; id++)
    {
        RequestVoteResponse &response = rvResponses.emplace_back();
        response.set_votegranted(1);
        response.set_responderid(id);
        response.set_term(1);
        EXPECT_CALL(*m_raftStubs[id], RequestVote)
            .Times(testing::Exactly(1))
            .WillRepeatedly(testing::DoAll(testing::SetArgPointee<2>(response), testing::Return(grpc::Status::OK)));

        AppendEntriesResponse &aeResponse = aeResponses.emplace_back();
        aeResponse.set_responderid(id);
        aeResponse.set_success(true);
        EXPECT_CALL(*m_raftStubs[id], AppendEntries)
            .Times(testing::AtLeast(1))
            .WillRepeatedly(testing::DoAll(testing::SetArgPointee<2>(aeResponse), testing::Return(grpc::Status::OK)));
    }

    raft::consensus_module_t consensusModule{m_configs[1], raftClients(), tkvClients()};
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

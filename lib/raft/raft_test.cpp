#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <vector>
#include <thread>
#include <chrono>
#include <future>
#include <filesystem>

#include "raft.h"
#include "wal/wal.h"
#include "Raft_mock.grpc.pb.h" // Generated mock stubs

using namespace testing;
using namespace raft;

// Test fixtures
class RaftNodeGrpcClientTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        config.m_id = 1;
        config.m_ip = "127.0.0.1:5000";
        mockStub = std::make_unique<MockRaftServiceStub>();
        mockStubPtr = mockStub.get();
    }

    node_config_t                        config;
    std::unique_ptr<MockRaftServiceStub> mockStub;
    MockRaftServiceStub                 *mockStubPtr;
};

class ConsensusModuleTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        nodeConfig.m_id = 1;
        nodeConfig.m_ip = "127.0.0.1:5001";

        // Create directories for test files
        std::filesystem::create_directories("./var/tkvpp/");

        // Create real WAL instance
        walPath = std::filesystem::temp_directory_path() /
                  ("test_wal_" + std::to_string(nodeConfig.m_id) + ".log");

        auto walOpt = wal::wal_builder_t{}.set_file_path(walPath).build<LogEntry>(
            wal::log_storage_type_k::in_memory_k
        );

        ASSERT_TRUE(walOpt.has_value());
        walPtr = std::make_shared<wal::wal_t<LogEntry>>(std::move(walOpt.value()));
    }

    void TearDown() override
    {
        // Clean up any persistent state files
        std::filesystem::remove_all("./var/tkvpp/");
        if (std::filesystem::exists(walPath))
        {
            std::filesystem::remove(walPath);
        }
    }

    auto createConsensusModule(std::vector<raft_node_grpc_client_t> replicas = {})
        -> std::unique_ptr<consensus_module_t>
    {
        return std::make_unique<consensus_module_t>(nodeConfig, std::move(replicas), walPtr);
    }

    auto createConsensusModuleWithPersistentWAL(std::vector<raft_node_grpc_client_t> replicas = {})
        -> std::unique_ptr<consensus_module_t>
    {

        auto persistentWalOpt = wal::wal_builder_t{}.set_file_path(walPath).build<LogEntry>(
            wal::log_storage_type_k::file_based_persistent_k
        );

        if (!persistentWalOpt.has_value())
        {
            return nullptr;
        }

        auto persistentWalPtr =
            std::make_shared<wal::wal_t<LogEntry>>(std::move(persistentWalOpt.value()));
        return std::make_unique<consensus_module_t>(
            nodeConfig, std::move(replicas), persistentWalPtr
        );
    }

    auto createMockClient(id_t id, const std::string &ip) -> raft_node_grpc_client_t
    {
        node_config_t clientConfig{.m_id = id, .m_ip = ip};
        auto          mockStub = std::make_unique<MockRaftServiceStub>();
        return {clientConfig, std::move(mockStub)};
    }

    node_config_t                         nodeConfig;
    std::shared_ptr<wal::wal_t<LogEntry>> walPtr;
    std::filesystem::path                 walPath;
};

// ============================================================================
// raft_node_grpc_client_t Tests
// ============================================================================

TEST_F(RaftNodeGrpcClientTest, Constructor)
{
    raft_node_grpc_client_t client(config, std::move(mockStub));

    EXPECT_EQ(client.id(), 1);
    EXPECT_EQ(client.ip(), "127.0.0.1:5000");
}

TEST_F(RaftNodeGrpcClientTest, AppendEntriesSuccess)
{
    EXPECT_CALL(*mockStubPtr, AppendEntries(_, _, _))
        .WillOnce(DoAll(
            WithArg<2>(
                [](AppendEntriesResponse *response)
                {
                    response->set_success(true);
                    response->set_term(1);
                }
            ),
            Return(grpc::Status::OK)
        ));

    raft_node_grpc_client_t client(config, std::move(mockStub));
    AppendEntriesRequest    request;
    AppendEntriesResponse   response;

    bool result = client.appendEntries(request, &response);

    EXPECT_TRUE(result);
    EXPECT_TRUE(response.success());
    EXPECT_EQ(response.term(), 1);
}

TEST_F(RaftNodeGrpcClientTest, AppendEntriesFailure)
{
    EXPECT_CALL(*mockStubPtr, AppendEntries(_, _, _))
        .WillOnce(Return(grpc::Status(grpc::StatusCode::UNAVAILABLE, "Service unavailable")));

    raft_node_grpc_client_t client(config, std::move(mockStub));
    AppendEntriesRequest    request;
    AppendEntriesResponse   response;

    bool result = client.appendEntries(request, &response);

    EXPECT_FALSE(result);
}

TEST_F(RaftNodeGrpcClientTest, RequestVoteSuccess)
{
    EXPECT_CALL(*mockStubPtr, RequestVote(_, _, _))
        .WillOnce(DoAll(
            WithArg<2>(
                [](RequestVoteResponse *response)
                {
                    response->set_votegranted(1);
                    response->set_term(2);
                }
            ),
            Return(grpc::Status::OK)
        ));

    raft_node_grpc_client_t client(config, std::move(mockStub));
    RequestVoteRequest      request;
    RequestVoteResponse     response;

    bool result = client.requestVote(request, &response);

    EXPECT_TRUE(result);
    EXPECT_EQ(response.votegranted(), 1);
    EXPECT_EQ(response.term(), 2);
}

TEST_F(RaftNodeGrpcClientTest, ReplicateSuccess)
{
    EXPECT_CALL(*mockStubPtr, Replicate(_, _, _))
        .WillOnce(DoAll(
            WithArg<2>([](ReplicateEntriesResponse *response) { response->set_status("OK"); }),
            Return(grpc::Status::OK)
        ));

    raft_node_grpc_client_t  client(config, std::move(mockStub));
    ReplicateEntriesRequest  request;
    ReplicateEntriesResponse response;

    bool result = client.replicate(request, &response);

    EXPECT_TRUE(result);
    EXPECT_EQ(response.status(), "OK");
}

// ============================================================================
// consensus_module_t Basic Tests
// ============================================================================

TEST_F(ConsensusModuleTest, Constructor)
{
    std::vector<raft_node_grpc_client_t> replicas;
    replicas.push_back(createMockClient(2, "127.0.0.1:5002"));
    replicas.push_back(createMockClient(3, "127.0.0.1:5003"));

    auto consensus = createConsensusModule(std::move(replicas));

    EXPECT_EQ(consensus->currentTerm(), 0);
    EXPECT_EQ(consensus->votedFor(), 0);
    EXPECT_EQ(consensus->getStateSafe(), NodeState::FOLLOWER);
}

TEST_F(ConsensusModuleTest, InitializationSuccess)
{
    auto consensus = createConsensusModule();
    bool result = consensus->init();

    EXPECT_TRUE(result);
}

TEST_F(ConsensusModuleTest, StartAndStop)
{
    auto consensus = createConsensusModule();
    ASSERT_TRUE(consensus->init());

    consensus->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    consensus->stop();

    // Should not crash
    SUCCEED();
}

TEST_F(ConsensusModuleTest, WALBasicOperations)
{
    // Test that we can work with the real WAL
    EXPECT_EQ(walPtr->size(), 0);
    EXPECT_TRUE(walPtr->empty());

    LogEntry entry;
    entry.set_term(1);
    entry.set_index(1);
    entry.set_payload("test payload");

    EXPECT_TRUE(walPtr->add(entry));
    EXPECT_EQ(walPtr->size(), 1);
    EXPECT_FALSE(walPtr->empty());

    auto readEntry = walPtr->read(0);
    ASSERT_TRUE(readEntry.has_value());
    EXPECT_EQ(readEntry->term(), 1);
    EXPECT_EQ(readEntry->index(), 1);
    EXPECT_EQ(readEntry->payload(), "test payload");
}

// ============================================================================
// Leader Election Tests
// ============================================================================

TEST_F(ConsensusModuleTest, SingleNodeBecomesLeader)
{
    auto consensus = createConsensusModule();
    ASSERT_TRUE(consensus->init());

    consensus->start();

    // Wait for election timeout and becoming leader
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    EXPECT_EQ(consensus->getStateSafe(), NodeState::LEADER);
    EXPECT_GT(consensus->currentTerm(), 0);

    consensus->stop();
}

TEST_F(ConsensusModuleTest, RequestVoteWithHigherTerm)
{
    auto consensus = createConsensusModule();
    ASSERT_TRUE(consensus->init());

    RequestVoteRequest request;
    request.set_term(5);
    request.set_candidateid(2);
    request.set_lastlogindex(0);
    request.set_lastlogterm(0);

    RequestVoteResponse response;
    grpc::ServerContext context;

    auto status = consensus->RequestVote(&context, &request, &response);

    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.votegranted(), 1);
    EXPECT_EQ(consensus->currentTerm(), 5);
    EXPECT_EQ(consensus->votedFor(), 2);
}

TEST_F(ConsensusModuleTest, RequestVoteWithLowerTerm)
{
    auto consensus = createConsensusModule();
    ASSERT_TRUE(consensus->init());

    // Manually set higher term
    RequestVoteRequest initialRequest;
    initialRequest.set_term(5);
    initialRequest.set_candidateid(2);
    initialRequest.set_lastlogindex(0);
    initialRequest.set_lastlogterm(0);

    RequestVoteResponse initialResponse;
    grpc::ServerContext initialContext;
    consensus->RequestVote(&initialContext, &initialRequest, &initialResponse);

    // Now request with lower term
    RequestVoteRequest request;
    request.set_term(3);
    request.set_candidateid(3);
    request.set_lastlogindex(0);
    request.set_lastlogterm(0);

    RequestVoteResponse response;
    grpc::ServerContext context;

    auto status = consensus->RequestVote(&context, &request, &response);

    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.votegranted(), 0);
    EXPECT_EQ(response.term(), 5);
}

TEST_F(ConsensusModuleTest, RequestVoteAlreadyVoted)
{
    auto consensus = createConsensusModule();
    ASSERT_TRUE(consensus->init());

    // First vote
    RequestVoteRequest request1;
    request1.set_term(5);
    request1.set_candidateid(2);
    request1.set_lastlogindex(0);
    request1.set_lastlogterm(0);

    RequestVoteResponse response1;
    grpc::ServerContext context1;

    consensus->RequestVote(&context1, &request1, &response1);
    EXPECT_EQ(response1.votegranted(), 1);

    // Second vote from different candidate
    RequestVoteRequest request2;
    request2.set_term(5);
    request2.set_candidateid(3);
    request2.set_lastlogindex(0);
    request2.set_lastlogterm(0);

    RequestVoteResponse response2;
    grpc::ServerContext context2;

    auto status = consensus->RequestVote(&context2, &request2, &response2);

    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response2.votegranted(), 0);
}

// ============================================================================
// Log Replication Tests
// ============================================================================

TEST_F(ConsensusModuleTest, AppendEntriesBasic)
{
    auto consensus = createConsensusModule();
    ASSERT_TRUE(consensus->init());

    AppendEntriesRequest request;
    request.set_term(1);
    request.set_senderid(2);
    request.set_prevlogindex(0);
    request.set_prevlogterm(0);
    request.set_leadercommit(0);

    // Add a log entry
    LogEntry *entry = request.add_entries();
    entry->set_term(1);
    entry->set_index(1);
    entry->set_payload("test payload");

    AppendEntriesResponse response;
    grpc::ServerContext   context;

    auto status = consensus->AppendEntries(&context, &request, &response);

    EXPECT_TRUE(status.ok());
    EXPECT_TRUE(response.success());
    EXPECT_EQ(response.term(), 1);

    // Verify the entry was added to WAL
    EXPECT_EQ(walPtr->size(), 1);
    auto readEntry = walPtr->read(0);
    ASSERT_TRUE(readEntry.has_value());
    EXPECT_EQ(readEntry->payload(), "test payload");
}

TEST_F(ConsensusModuleTest, AppendEntriesLogInconsistency)
{
    // Pre-populate WAL with an entry
    LogEntry existingEntry;
    existingEntry.set_term(1);
    existingEntry.set_index(1);
    existingEntry.set_payload("existing");

    ASSERT_TRUE(walPtr->add(existingEntry));

    auto consensus = createConsensusModule();
    ASSERT_TRUE(consensus->init());

    AppendEntriesRequest request;
    request.set_term(2);
    request.set_senderid(2);
    request.set_prevlogindex(1);
    request.set_prevlogterm(2); // Mismatch - existing entry has term 1
    request.set_leadercommit(0);

    AppendEntriesResponse response;
    grpc::ServerContext   context;

    auto status = consensus->AppendEntries(&context, &request, &response);

    EXPECT_TRUE(status.ok());
    EXPECT_FALSE(response.success());
}

TEST_F(ConsensusModuleTest, AppendEntriesWithHigherTerm)
{
    auto consensus = createConsensusModule();
    ASSERT_TRUE(consensus->init());

    AppendEntriesRequest request;
    request.set_term(5);
    request.set_senderid(2);
    request.set_prevlogindex(0);
    request.set_prevlogterm(0);
    request.set_leadercommit(0);

    AppendEntriesResponse response;
    grpc::ServerContext   context;

    auto status = consensus->AppendEntries(&context, &request, &response);

    EXPECT_TRUE(status.ok());
    EXPECT_TRUE(response.success());
    EXPECT_EQ(consensus->currentTerm(), 5);
    EXPECT_EQ(consensus->getStateSafe(), NodeState::FOLLOWER);
}

// ============================================================================
// Client Replication Tests
// ============================================================================

TEST_F(ConsensusModuleTest, ReplicateAsFollower)
{
    auto consensus = createConsensusModule();
    ASSERT_TRUE(consensus->init());

    ReplicateEntriesRequest request;
    request.add_payloads("test payload");

    ReplicateEntriesResponse response;
    grpc::ServerContext      context;

    auto status = consensus->Replicate(&context, &request, &response);

    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.status(), "FAILED");
}

TEST_F(ConsensusModuleTest, ReplicateAsLeader)
{
    auto consensus = createConsensusModule();
    ASSERT_TRUE(consensus->init());

    // Manually become leader for testing
    consensus->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Wait to become leader

    if (consensus->getStateSafe() == NodeState::LEADER)
    {
        auto future = consensus->replicateAsync("test payload");

        // Should complete quickly since there are no replicas
        auto status = future.wait_for(std::chrono::milliseconds(1000));
        EXPECT_EQ(status, std::future_status::ready);

        bool result = future.get();
        EXPECT_TRUE(result); // Single node cluster should succeed immediately

        // Verify entry was added to WAL
        EXPECT_GT(walPtr->size(), 0);
        if (walPtr->size() > 0)
        {
            auto entry = walPtr->read(walPtr->size() - 1);
            ASSERT_TRUE(entry.has_value());
            EXPECT_EQ(entry->payload(), "test payload");
        }
    }

    consensus->stop();
}

// ============================================================================
// Persistence Tests
// ============================================================================

TEST_F(ConsensusModuleTest, PersistentStateRestore)
{
    // Create and initialize first consensus module
    {
        auto consensus1 = createConsensusModule();
        ASSERT_TRUE(consensus1->init());

        // Simulate voting for candidate 2 in term 3
        RequestVoteRequest request;
        request.set_term(3);
        request.set_candidateid(2);
        request.set_lastlogindex(0);
        request.set_lastlogterm(0);

        RequestVoteResponse response;
        grpc::ServerContext context;

        consensus1->RequestVote(&context, &request, &response);
        EXPECT_EQ(response.votegranted(), 1);
    }

    // Create new consensus module and verify state restoration
    {
        auto consensus2 = createConsensusModule();
        ASSERT_TRUE(consensus2->init());

        EXPECT_EQ(consensus2->currentTerm(), 3);
        EXPECT_EQ(consensus2->votedFor(), 2);
    }
}

TEST_F(ConsensusModuleTest, WALPersistenceRecovery)
{
    // Test with persistent WAL
    std::vector<LogEntry> testEntries;
    for (int i = 0; i < 5; ++i)
    {
        LogEntry entry;
        entry.set_term(1);
        entry.set_index(i + 1);
        entry.set_payload("payload_" + std::to_string(i));
        testEntries.push_back(entry);
    }

    // Create persistent WAL and add entries
    {
        auto persistentConsensus = createConsensusModuleWithPersistentWAL();
        if (persistentConsensus)
        {
            ASSERT_TRUE(persistentConsensus->init());

            // Add entries via AppendEntries
            for (const auto &entry : testEntries)
            {
                AppendEntriesRequest request;
                request.set_term(1);
                request.set_senderid(2);
                request.set_prevlogindex(entry.index() - 1);
                request.set_prevlogterm(entry.index() > 1 ? 1 : 0);
                request.set_leadercommit(0);

                LogEntry *reqEntry = request.add_entries();
                reqEntry->CopyFrom(entry);

                AppendEntriesResponse response;
                grpc::ServerContext   context;

                auto status = persistentConsensus->AppendEntries(&context, &request, &response);
                EXPECT_TRUE(status.ok());
                EXPECT_TRUE(response.success());
            }
        }
    }

    // Create new persistent WAL and verify recovery
    {
        auto recoveredConsensus = createConsensusModuleWithPersistentWAL();
        if (recoveredConsensus)
        {
            ASSERT_TRUE(recoveredConsensus->init());

            auto log = recoveredConsensus->log();
            EXPECT_EQ(log.size(), testEntries.size());

            for (size_t i = 0; i < testEntries.size() && i < log.size(); ++i)
            {
                EXPECT_EQ(log[i].payload(), testEntries[i].payload());
                EXPECT_EQ(log[i].term(), testEntries[i].term());
            }
        }
    }
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

TEST_F(ConsensusModuleTest, ConcurrentStateAccess)
{
    auto consensus = createConsensusModule();
    ASSERT_TRUE(consensus->init());
    consensus->start();

    std::atomic<bool>        stop{false};
    std::vector<std::thread> threads;

    // Multiple threads accessing state
    for (int i = 0; i < 5; ++i)
    {
        threads.emplace_back(
            [&consensus, &stop]()
            {
                while (!stop.load())
                {
                    auto term = consensus->currentTerm();
                    auto votedFor = consensus->votedFor();
                    auto state = consensus->getStateSafe();
                    (void)term;
                    (void)votedFor;
                    (void)state; // Suppress unused warnings
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            }
        );
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    stop = true;

    for (auto &thread : threads)
    {
        thread.join();
    }

    consensus->stop();
    SUCCEED(); // If we get here without crashes, the test passes
}

// ============================================================================
// WAL Operations Tests
// ============================================================================

TEST_F(ConsensusModuleTest, WALResetOperations)
{
    // Add some entries to WAL
    for (int i = 0; i < 5; ++i)
    {
        LogEntry entry;
        entry.set_term(1);
        entry.set_index(i + 1);
        entry.set_payload("entry_" + std::to_string(i));
        ASSERT_TRUE(walPtr->add(entry));
    }

    EXPECT_EQ(walPtr->size(), 5);

    // Test reset_last_n
    EXPECT_TRUE(walPtr->reset_last_n(2));
    EXPECT_EQ(walPtr->size(), 3);

    // Verify remaining entries
    for (size_t i = 0; i < 3; ++i)
    {
        auto entry = walPtr->read(i);
        ASSERT_TRUE(entry.has_value());
        EXPECT_EQ(entry->payload(), "entry_" + std::to_string(i));
    }

    // Test full reset
    EXPECT_TRUE(walPtr->reset());
    EXPECT_EQ(walPtr->size(), 0);
    EXPECT_TRUE(walPtr->empty());
}

// ============================================================================
// Callback Tests
// ============================================================================

TEST_F(ConsensusModuleTest, OnCommitCallback)
{
    auto consensus = createConsensusModule();

    bool        callbackCalled = false;
    std::string receivedPayload;

    consensus->setOnCommitCallback(
        [&](const LogEntry &entry) -> bool
        {
            callbackCalled = true;
            receivedPayload = entry.payload();
            return true;
        }
    );

    ASSERT_TRUE(consensus->init());

    // Add an entry and set commit index
    AppendEntriesRequest request;
    request.set_term(1);
    request.set_senderid(2);
    request.set_prevlogindex(0);
    request.set_prevlogterm(0);
    request.set_leadercommit(1);

    LogEntry *entry = request.add_entries();
    entry->set_term(1);
    entry->set_index(1);
    entry->set_payload("callback_test");

    AppendEntriesResponse response;
    grpc::ServerContext   context;

    auto status = consensus->AppendEntries(&context, &request, &response);
    EXPECT_TRUE(status.ok());

    // Give some time for async callback execution
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    EXPECT_TRUE(callbackCalled);
    EXPECT_EQ(receivedPayload, "callback_test");
}

TEST_F(ConsensusModuleTest, OnLeaderChangeCallback)
{
    auto consensus = createConsensusModule();

    std::atomic<bool> becameLeader{false};
    std::atomic<bool> lostLeadership{false};

    consensus->setOnLeaderChangeCallback(
        [&](bool isLeader)
        {
            if (isLeader)
            {
                becameLeader = true;
            }
            else
            {
                lostLeadership = true;
            }
        }
    );

    ASSERT_TRUE(consensus->init());
    consensus->start();

    // Wait to become leader
    std::this_thread::sleep_for(std::chrono::milliseconds(600));

    EXPECT_TRUE(becameLeader.load());

    // Simulate losing leadership through higher term AppendEntries
    AppendEntriesRequest request;
    request.set_term(consensus->currentTerm() + 1);
    request.set_senderid(2);
    request.set_prevlogindex(0);
    request.set_prevlogterm(0);
    request.set_leadercommit(0);

    AppendEntriesResponse response;
    grpc::ServerContext   context;

    consensus->AppendEntries(&context, &request, &response);

    // Give time for callback
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    EXPECT_TRUE(lostLeadership.load());

    consensus->stop();
}

// ============================================================================
// Advanced Integration Tests with Mock Network Communication
// ============================================================================

class RaftClusterTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        std::filesystem::create_directories("./var/tkvpp/");

        // Create mock stubs for network communication
        mockStub1 = std::make_unique<MockRaftServiceStub>();
        mockStub2 = std::make_unique<MockRaftServiceStub>();
        mockStub3 = std::make_unique<MockRaftServiceStub>();

        // Keep raw pointers for setting expectations
        mockStubPtr1 = mockStub1.get();
        mockStubPtr2 = mockStub2.get();
        mockStubPtr3 = mockStub3.get();
    }

    void TearDown() override
    {
        std::filesystem::remove_all("./var/tkvpp/");
    }

    std::unique_ptr<consensus_module_t> createNodeWithMockPeers(
        id_t                                              nodeId,
        const std::string                                &nodeIp,
        std::vector<std::pair<id_t, std::string>>         peerConfigs,
        std::vector<std::unique_ptr<MockRaftServiceStub>> peerStubs
    )
    {

        node_config_t nodeConfig{nodeId, nodeIp};

        // Create WAL for this node
        auto walPath = std::filesystem::temp_directory_path() /
                       ("cluster_test_wal_" + std::to_string(nodeId) + ".log");

        auto walOpt = wal::wal_builder_t{}.set_file_path(walPath).build<LogEntry>(
            wal::log_storage_type_k::in_memory_k
        );

        if (!walOpt.has_value())
        {
            return nullptr;
        }

        auto walPtr = std::make_shared<wal::wal_t<LogEntry>>(std::move(walOpt.value()));

        // Create replica clients with mock stubs
        std::vector<raft_node_grpc_client_t> replicas;
        for (size_t i = 0; i < peerConfigs.size() && i < peerStubs.size(); ++i)
        {
            node_config_t peerConfig{peerConfigs[i].first, peerConfigs[i].second};
            replicas.emplace_back(peerConfig, std::move(peerStubs[i]));
        }

        return std::make_unique<consensus_module_t>(nodeConfig, std::move(replicas), walPtr);
    }

    std::unique_ptr<MockRaftServiceStub> mockStub1;
    std::unique_ptr<MockRaftServiceStub> mockStub2;
    std::unique_ptr<MockRaftServiceStub> mockStub3;

    MockRaftServiceStub *mockStubPtr1;
    MockRaftServiceStub *mockStubPtr2;
    MockRaftServiceStub *mockStubPtr3;
};

TEST_F(RaftClusterTest, LeaderElectionWithMockNetwork)
{
    // Setup mock expectations for RequestVote RPCs
    EXPECT_CALL(*mockStubPtr2, RequestVote(_, _, _))
        .WillRepeatedly(DoAll(
            WithArg<2>(
                [](RequestVoteResponse *response)
                {
                    response->set_votegranted(1);
                    response->set_term(1);
                    response->set_responderid(2);
                }
            ),
            Return(grpc::Status::OK)
        ));

    EXPECT_CALL(*mockStubPtr3, RequestVote(_, _, _))
        .WillRepeatedly(DoAll(
            WithArg<2>(
                [](RequestVoteResponse *response)
                {
                    response->set_votegranted(1);
                    response->set_term(1);
                    response->set_responderid(3);
                }
            ),
            Return(grpc::Status::OK)
        ));

    // Create node 1 with mocked peers
    std::vector<std::pair<id_t, std::string>> peerConfigs = {
        {2, "127.0.0.1:5002"}, {3, "127.0.0.1:5003"}
    };

    std::vector<std::unique_ptr<MockRaftServiceStub>> stubs;
    stubs.push_back(std::move(mockStub2));
    stubs.push_back(std::move(mockStub3));

    auto node1 = createNodeWithMockPeers(1, "127.0.0.1:5001", peerConfigs, std::move(stubs));
    ASSERT_NE(node1, nullptr);
    ASSERT_TRUE(node1->init());

    node1->start();

    // Wait for election to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(600));

    EXPECT_EQ(node1->getStateSafe(), NodeState::LEADER);
    EXPECT_GT(node1->currentTerm(), 0);

    node1->stop();
}

TEST_F(RaftClusterTest, HeartbeatCommunication)
{
    // Setup expectations for heartbeat AppendEntries
    EXPECT_CALL(*mockStubPtr2, AppendEntries(_, _, _))
        .WillRepeatedly(DoAll(
            WithArg<2>(
                [](AppendEntriesResponse *response)
                {
                    response->set_success(true);
                    response->set_term(1);
                    response->set_responderid(2);
                    response->set_match_index(0);
                }
            ),
            Return(grpc::Status::OK)
        ));

    EXPECT_CALL(*mockStubPtr3, AppendEntries(_, _, _))
        .WillRepeatedly(DoAll(
            WithArg<2>(
                [](AppendEntriesResponse *response)
                {
                    response->set_success(true);
                    response->set_term(1);
                    response->set_responderid(3);
                    response->set_match_index(0);
                }
            ),
            Return(grpc::Status::OK)
        ));

    // First setup RequestVote expectations to become leader
    EXPECT_CALL(*mockStubPtr2, RequestVote(_, _, _))
        .WillOnce(DoAll(
            WithArg<2>(
                [](RequestVoteResponse *response)
                {
                    response->set_votegranted(1);
                    response->set_term(1);
                    response->set_responderid(2);
                }
            ),
            Return(grpc::Status::OK)
        ));

    EXPECT_CALL(*mockStubPtr3, RequestVote(_, _, _))
        .WillOnce(DoAll(
            WithArg<2>(
                [](RequestVoteResponse *response)
                {
                    response->set_votegranted(1);
                    response->set_term(1);
                    response->set_responderid(3);
                }
            ),
            Return(grpc::Status::OK)
        ));

    std::vector<std::pair<id_t, std::string>> peerConfigs = {
        {2, "127.0.0.1:5002"}, {3, "127.0.0.1:5003"}
    };

    std::vector<std::unique_ptr<MockRaftServiceStub>> stubs;
    stubs.push_back(std::move(mockStub2));
    stubs.push_back(std::move(mockStub3));

    auto leader = createNodeWithMockPeers(1, "127.0.0.1:5001", peerConfigs, std::move(stubs));
    ASSERT_NE(leader, nullptr);
    ASSERT_TRUE(leader->init());

    leader->start();

    // Wait longer to ensure heartbeats are sent
    std::this_thread::sleep_for(std::chrono::milliseconds(800));

    EXPECT_EQ(leader->getStateSafe(), NodeState::LEADER);

    leader->stop();
}

TEST_F(RaftClusterTest, LogReplicationWithMockNetwork)
{
    // Setup expectations for log replication
    EXPECT_CALL(*mockStubPtr2, AppendEntries(_, _, _))
        .WillRepeatedly(DoAll(
            WithArg<2>(
                [](AppendEntriesResponse *response)
                {
                    response->set_success(true);
                    response->set_term(1);
                    response->set_responderid(2);
                    response->set_match_index(1);
                }
            ),
            Return(grpc::Status::OK)
        ));

    EXPECT_CALL(*mockStubPtr3, AppendEntries(_, _, _))
        .WillRepeatedly(DoAll(
            WithArg<2>(
                [](AppendEntriesResponse *response)
                {
                    response->set_success(true);
                    response->set_term(1);
                    response->set_responderid(3);
                    response->set_match_index(1);
                }
            ),
            Return(grpc::Status::OK)
        ));

    // RequestVote expectations to become leader
    EXPECT_CALL(*mockStubPtr2, RequestVote(_, _, _))
        .WillOnce(DoAll(
            WithArg<2>(
                [](RequestVoteResponse *response)
                {
                    response->set_votegranted(1);
                    response->set_term(1);
                    response->set_responderid(2);
                }
            ),
            Return(grpc::Status::OK)
        ));

    EXPECT_CALL(*mockStubPtr3, RequestVote(_, _, _))
        .WillOnce(DoAll(
            WithArg<2>(
                [](RequestVoteResponse *response)
                {
                    response->set_votegranted(1);
                    response->set_term(1);
                    response->set_responderid(3);
                }
            ),
            Return(grpc::Status::OK)
        ));

    std::vector<std::pair<id_t, std::string>> peerConfigs = {
        {2, "127.0.0.1:5002"}, {3, "127.0.0.1:5003"}
    };

    std::vector<std::unique_ptr<MockRaftServiceStub>> stubs;
    stubs.push_back(std::move(mockStub2));
    stubs.push_back(std::move(mockStub3));

    auto leader = createNodeWithMockPeers(1, "127.0.0.1:5001", peerConfigs, std::move(stubs));
    ASSERT_NE(leader, nullptr);
    ASSERT_TRUE(leader->init());

    leader->start();

    // Wait to become leader
    std::this_thread::sleep_for(std::chrono::milliseconds(600));

    if (leader->getStateSafe() == NodeState::LEADER)
    {
        // Test async replication
        auto future = leader->replicateAsync("test_replication_payload");

        auto status = future.wait_for(std::chrono::seconds(2));
        EXPECT_EQ(status, std::future_status::ready);

        if (status == std::future_status::ready)
        {
            bool result = future.get();
            EXPECT_TRUE(result);
        }
    }

    leader->stop();
}

TEST_F(RaftClusterTest, NetworkPartitionSimulation)
{
    // Simulate network partition by having some peers fail
    EXPECT_CALL(*mockStubPtr2, RequestVote(_, _, _))
        .WillRepeatedly(Return(grpc::Status(grpc::StatusCode::UNAVAILABLE, "Network partition")));

    EXPECT_CALL(*mockStubPtr3, RequestVote(_, _, _))
        .WillOnce(DoAll(
            WithArg<2>(
                [](RequestVoteResponse *response)
                {
                    response->set_votegranted(1);
                    response->set_term(1);
                    response->set_responderid(3);
                }
            ),
            Return(grpc::Status::OK)
        ));

    std::vector<std::pair<id_t, std::string>> peerConfigs = {
        {2, "127.0.0.1:5002"}, {3, "127.0.0.1:5003"}
    };

    std::vector<std::unique_ptr<MockRaftServiceStub>> stubs;
    stubs.push_back(std::move(mockStub2));
    stubs.push_back(std::move(mockStub3));

    auto node1 = createNodeWithMockPeers(1, "127.0.0.1:5001", peerConfigs, std::move(stubs));
    ASSERT_NE(node1, nullptr);
    ASSERT_TRUE(node1->init());

    node1->start();

    // In a 3-node cluster with one partition, node should not become leader
    // (needs majority = 2 votes, but only gets 1 from node 3 + itself = 2 total)
    std::this_thread::sleep_for(std::chrono::milliseconds(800));

    // Should be able to become leader with majority (2 out of 3)
    EXPECT_EQ(node1->getStateSafe(), NodeState::LEADER);

    node1->stop();
}

TEST_F(RaftClusterTest, ConflictingTerms)
{
    // Simulate receiving higher term from peer
    EXPECT_CALL(*mockStubPtr2, RequestVote(_, _, _))
        .WillOnce(DoAll(
            WithArg<2>(
                [](RequestVoteResponse *response)
                {
                    response->set_votegranted(0); // Don't grant vote
                    response->set_term(5);        // But indicate higher term
                    response->set_responderid(2);
                }
            ),
            Return(grpc::Status::OK)
        ));

    EXPECT_CALL(*mockStubPtr3, RequestVote(_, _, _))
        .WillOnce(DoAll(
            WithArg<2>(
                [](RequestVoteResponse *response)
                {
                    response->set_votegranted(1);
                    response->set_term(1); // Lower term
                    response->set_responderid(3);
                }
            ),
            Return(grpc::Status::OK)
        ));

    std::vector<std::pair<id_t, std::string>> peerConfigs = {
        {2, "127.0.0.1:5002"}, {3, "127.0.0.1:5003"}
    };

    std::vector<std::unique_ptr<MockRaftServiceStub>> stubs;
    stubs.push_back(std::move(mockStub2));
    stubs.push_back(std::move(mockStub3));

    auto node1 = createNodeWithMockPeers(1, "127.0.0.1:5001", peerConfigs, std::move(stubs));
    ASSERT_NE(node1, nullptr);
    ASSERT_TRUE(node1->init());

    node1->start();

    std::this_thread::sleep_for(std::chrono::milliseconds(600));

    // Node should update its term and revert to follower
    EXPECT_EQ(node1->currentTerm(), 5);
    EXPECT_EQ(node1->getStateSafe(), NodeState::FOLLOWER);

    node1->stop();
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(ConsensusModuleTest, ThreeNodeClusterSetup)
{
    std::vector<raft_node_grpc_client_t> replicas;
    replicas.push_back(createMockClient(2, "127.0.0.1:5002"));
    replicas.push_back(createMockClient(3, "127.0.0.1:5003"));

    auto consensus = createConsensusModule(std::move(replicas));
    ASSERT_TRUE(consensus->init());

    // Verify cluster configuration
    EXPECT_EQ(consensus->currentTerm(), 0);
    EXPECT_EQ(consensus->getStateSafe(), NodeState::FOLLOWER);

    // The actual election would require mock expectations for network calls
    // This is a basic structural test
    SUCCEED();
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(ConsensusModuleTest, WALErrorHandling)
{
    auto consensus = createConsensusModule();
    ASSERT_TRUE(consensus->init());

    // Test invalid read operations
    EXPECT_FALSE(walPtr->read(1000).has_value());

    // Test reset_last_n with invalid parameters
    EXPECT_FALSE(walPtr->reset_last_n(1000)); // More than available
    EXPECT_FALSE(walPtr->reset_last_n(0));    // Invalid size
}

TEST_F(ConsensusModuleTest, AppendEntriesErrorConditions)
{
    auto consensus = createConsensusModule();
    ASSERT_TRUE(consensus->init());

    // Test with missing previous log entry
    AppendEntriesRequest request;
    request.set_term(1);
    request.set_senderid(2);
    request.set_prevlogindex(10); // Non-existent index
    request.set_prevlogterm(1);
    request.set_leadercommit(0);

    AppendEntriesResponse response;
    grpc::ServerContext   context;

    auto status = consensus->AppendEntries(&context, &request, &response);

    EXPECT_TRUE(status.ok());
    EXPECT_FALSE(response.success()); // Should fail due to missing previous entry
}

// ============================================================================
// WAL-specific Integration Tests
// ============================================================================

TEST_F(ConsensusModuleTest, LogConsistencyWithWAL)
{
    auto consensus = createConsensusModule();
    ASSERT_TRUE(consensus->init());

    std::vector<LogEntry> entries;
    for (int i = 0; i < 3; ++i)
    {
        LogEntry entry;
        entry.set_term(1);
        entry.set_index(i + 1);
        entry.set_payload("consistency_test_" + std::to_string(i));
        entries.push_back(entry);
    }

    // Add entries via AppendEntries
    for (const auto &entry : entries)
    {
        AppendEntriesRequest request;
        request.set_term(1);
        request.set_senderid(2);
        request.set_prevlogindex(entry.index() - 1);
        request.set_prevlogterm(entry.index() > 1 ? 1 : 0);
        request.set_leadercommit(0);

        LogEntry *reqEntry = request.add_entries();
        reqEntry->CopyFrom(entry);

        AppendEntriesResponse response;
        grpc::ServerContext   context;

        auto status = consensus->AppendEntries(&context, &request, &response);
        EXPECT_TRUE(status.ok());
        EXPECT_TRUE(response.success());
    }

    // Verify WAL consistency
    EXPECT_EQ(walPtr->size(), entries.size());
    auto walRecords = walPtr->records();
    EXPECT_EQ(walRecords.size(), entries.size());

    for (size_t i = 0; i < entries.size(); ++i)
    {
        auto walEntry = walPtr->read(i);
        ASSERT_TRUE(walEntry.has_value());
        EXPECT_EQ(walEntry->payload(), entries[i].payload());
        EXPECT_EQ(walEntry->term(), entries[i].term());
        EXPECT_EQ(walEntry->index(), entries[i].index());

        // Also check records() method
        EXPECT_EQ(walRecords[i].payload(), entries[i].payload());
    }
}

// Test log truncation scenarios
TEST_F(ConsensusModuleTest, LogTruncationWithWAL)
{
    auto consensus = createConsensusModule();
    ASSERT_TRUE(consensus->init());

    // Add initial entries
    std::vector<LogEntry> initialEntries;
    for (int i = 0; i < 5; ++i)
    {
        LogEntry entry;
        entry.set_term(1);
        entry.set_index(i + 1);
        entry.set_payload("initial_" + std::to_string(i));
        initialEntries.push_back(entry);

        AppendEntriesRequest request;
        request.set_term(1);
        request.set_senderid(2);
        request.set_prevlogindex(i);
        request.set_prevlogterm(i > 0 ? 1 : 0);
        request.set_leadercommit(0);

        LogEntry *reqEntry = request.add_entries();
        reqEntry->CopyFrom(entry);

        AppendEntriesResponse response;
        grpc::ServerContext   context;
        consensus->AppendEntries(&context, &request, &response);
    }

    EXPECT_EQ(walPtr->size(), 5);

    // Now send conflicting entry that should cause truncation
    AppendEntriesRequest conflictRequest;
    conflictRequest.set_term(2); // Higher term
    conflictRequest.set_senderid(3);
    conflictRequest.set_prevlogindex(3); // After 3rd entry
    conflictRequest.set_prevlogterm(1);
    conflictRequest.set_leadercommit(0);

    LogEntry *conflictEntry = conflictRequest.add_entries();
    conflictEntry->set_term(2);
    conflictEntry->set_index(4);
    conflictEntry->set_payload("conflict_entry");

    AppendEntriesResponse conflictResponse;
    grpc::ServerContext   conflictContext;

    auto status = consensus->AppendEntries(&conflictContext, &conflictRequest, &conflictResponse);
    EXPECT_TRUE(status.ok());
    EXPECT_TRUE(conflictResponse.success());

    // The implementation should handle log conflicts appropriately
    // Verify that WAL state is consistent after the operation
    EXPECT_GT(walPtr->size(), 0);
}

// ============================================================================
// Advanced WAL Integration Tests
// ============================================================================

TEST_F(ConsensusModuleTest, WALRecordsMethodTest)
{
    // Test the records() method specifically
    std::vector<LogEntry> testEntries;
    for (int i = 0; i < 3; ++i)
    {
        LogEntry entry;
        entry.set_term(1);
        entry.set_index(i + 1);
        entry.set_payload("records_test_" + std::to_string(i));
        testEntries.push_back(entry);
        ASSERT_TRUE(walPtr->add(entry));
    }

    auto records = walPtr->records();
    EXPECT_EQ(records.size(), 3);

    for (size_t i = 0; i < records.size(); ++i)
    {
        EXPECT_EQ(records[i].payload(), testEntries[i].payload());
        EXPECT_EQ(records[i].term(), testEntries[i].term());
        EXPECT_EQ(records[i].index(), testEntries[i].index());
    }

    // Test records on empty WAL
    ASSERT_TRUE(walPtr->reset());
    auto emptyRecords = walPtr->records();
    EXPECT_EQ(emptyRecords.size(), 0);
}

TEST_F(ConsensusModuleTest, ConsensusModuleLogMethodTest)
{
    auto consensus = createConsensusModule();
    ASSERT_TRUE(consensus->init());

    // Add entries through consensus module
    for (int i = 0; i < 3; ++i)
    {
        AppendEntriesRequest request;
        request.set_term(1);
        request.set_senderid(2);
        request.set_prevlogindex(i);
        request.set_prevlogterm(i > 0 ? 1 : 0);
        request.set_leadercommit(0);

        LogEntry *entry = request.add_entries();
        entry->set_term(1);
        entry->set_index(i + 1);
        entry->set_payload("consensus_log_test_" + std::to_string(i));

        AppendEntriesResponse response;
        grpc::ServerContext   context;
        consensus->AppendEntries(&context, &request, &response);
        EXPECT_TRUE(response.success());
    }

    // Test the log() method from consensus module
    auto consensusLog = consensus->log();
    EXPECT_EQ(consensusLog.size(), 3);

    for (size_t i = 0; i < consensusLog.size(); ++i)
    {
        EXPECT_EQ(consensusLog[i].payload(), "consensus_log_test_" + std::to_string(i));
        EXPECT_EQ(consensusLog[i].term(), 1);
        EXPECT_EQ(consensusLog[i].index(), i + 1);
    }
}

// ============================================================================
// Performance and Stress Tests
// ============================================================================

TEST_F(ConsensusModuleTest, WALPerformanceTest)
{
    const size_t numEntries = 100;

    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < numEntries; ++i)
    {
        LogEntry entry;
        entry.set_term(1);
        entry.set_index(i + 1);
        entry.set_payload("perf_test_" + std::to_string(i));
        ASSERT_TRUE(walPtr->add(entry));
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_EQ(walPtr->size(), numEntries);

    // Performance shouldn't be terrible (arbitrary threshold)
    EXPECT_LT(duration.count(), 1000); // Less than 1 second for 100 entries

    // Test read performance
    start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < numEntries; ++i)
    {
        auto entry = walPtr->read(i);
        ASSERT_TRUE(entry.has_value());
        EXPECT_EQ(entry->index(), i + 1);
    }

    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_LT(duration.count(), 500); // Less than 0.5 seconds to read 100 entries
}

TEST_F(ConsensusModuleTest, ConcurrentWALAccess)
{
    auto consensus = createConsensusModule();
    ASSERT_TRUE(consensus->init());

    const int                numThreads = 4;
    const int                entriesPerThread = 10;
    std::vector<std::thread> threads;
    std::atomic<int>         successCount{0};

    // Multiple threads adding entries concurrently
    for (int t = 0; t < numThreads; ++t)
    {
        threads.emplace_back(
            [&, t]()
            {
                for (int i = 0; i < entriesPerThread; ++i)
                {
                    AppendEntriesRequest request;
                    request.set_term(1);
                    request.set_senderid(2);
                    request.set_prevlogindex(0); // Simplified for concurrent test
                    request.set_prevlogterm(0);
                    request.set_leadercommit(0);

                    LogEntry *entry = request.add_entries();
                    entry->set_term(1);
                    entry->set_index(1); // Simplified
                    entry->set_payload(
                        "thread_" + std::to_string(t) + "_entry_" + std::to_string(i)
                    );

                    AppendEntriesResponse response;
                    grpc::ServerContext   context;

                    auto status = consensus->AppendEntries(&context, &request, &response);
                    if (status.ok() && response.success())
                    {
                        successCount++;
                    }
                }
            }
        );
    }

    for (auto &thread : threads)
    {
        thread.join();
    }

    // We expect some successful operations
    EXPECT_GT(successCount.load(), 0);
    EXPECT_GT(walPtr->size(), 0);
}

// ============================================================================
// Edge Cases and Error Recovery Tests
// ============================================================================

TEST_F(ConsensusModuleTest, WALBoundaryConditions)
{
    // Test reading from empty WAL
    EXPECT_FALSE(walPtr->read(0).has_value());
    EXPECT_FALSE(walPtr->read(SIZE_MAX).has_value());

    // Add one entry
    LogEntry entry;
    entry.set_term(1);
    entry.set_index(1);
    entry.set_payload("boundary_test");
    ASSERT_TRUE(walPtr->add(entry));

    // Test valid and invalid reads
    EXPECT_TRUE(walPtr->read(0).has_value());
    EXPECT_FALSE(walPtr->read(1).has_value());
    EXPECT_FALSE(walPtr->read(SIZE_MAX).has_value());

    // Test reset_last_n boundary conditions
    EXPECT_FALSE(walPtr->reset_last_n(0)); // Should fail
    EXPECT_FALSE(walPtr->reset_last_n(2)); // More than available
    EXPECT_TRUE(walPtr->reset_last_n(1));  // Should succeed
    EXPECT_EQ(walPtr->size(), 0);
}

TEST_F(ConsensusModuleTest, ConsensusModuleWithEmptyWAL)
{
    auto consensus = createConsensusModule();
    ASSERT_TRUE(consensus->init());

    // Test operations on consensus module with empty WAL
    EXPECT_EQ(consensus->currentTerm(), 0);
    EXPECT_EQ(consensus->votedFor(), 0);

    auto log = consensus->log();
    EXPECT_EQ(log.size(), 0);

    // Test vote request with empty log
    RequestVoteRequest request;
    request.set_term(1);
    request.set_candidateid(2);
    request.set_lastlogindex(0);
    request.set_lastlogterm(0);

    RequestVoteResponse response;
    grpc::ServerContext context;

    auto status = consensus->RequestVote(&context, &request, &response);
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.votegranted(), 1);
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::InitGoogleMock(&argc, argv);

    // Create test directories
    std::filesystem::create_directories("./var/tkvpp/");

    int result = RUN_ALL_TESTS();

    // Cleanup
    std::filesystem::remove_all("./var/tkvpp/");

    return result;
}

#pragma once

#include <chrono>        // for 'std::chrono::high_resolution_clock'
#include <cstdint>       // for 'uint32_t'
#include <string>        // for 'std::string'
#include <thread>        // for 'std::jthread'
#include <memory>        // for 'std::shared_ptr', 'std::unique_ptr'
#include <optional>      // for 'std::optional'
#include <unordered_map> // for 'std::unordered_map'
#include <vector>        // for 'std::vector'

#include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/support/status.h>
#include <absl/base/thread_annotations.h>

#include "Raft.grpc.pb.h"
#include "Raft.pb.h"

namespace raft
{

// NOLINTBEGIN(modernize-use-trailing-return-type)

using ip_t = std::string;
using id_t = uint32_t;
using clock_t = std::chrono::high_resolution_clock;
using timepoint_t = std::chrono::high_resolution_clock::time_point;

// Valid IDs start from 1
constexpr const id_t gInvalidId = 0;

struct node_config_t
{
    id_t m_id{gInvalidId};
    ip_t m_ip;
};

/**
 * Client for communicating with other nodes in the Raft cluster.
 * Handles RPC operations for consensus and key-value operations.
 */
class raft_node_grpc_client_t
{
  public:
    /**
     * Constructs a client for communicating with a specific node.
     * @param nodeId Unique identifier for the target node
     * @param nodeIp IP address of the target node
     * @throws std::runtime_error if connection cannot be established
     */
    raft_node_grpc_client_t(node_config_t                               config,
                            std::unique_ptr<RaftService::StubInterface> pRaftStub);
    virtual ~raft_node_grpc_client_t() noexcept = default;

    raft_node_grpc_client_t(const raft_node_grpc_client_t &) = delete;
    auto operator=(const raft_node_grpc_client_t &) -> raft_node_grpc_client_t & = delete;

    raft_node_grpc_client_t(raft_node_grpc_client_t &&) = default;
    auto operator=(raft_node_grpc_client_t &&) -> raft_node_grpc_client_t & = default;

    auto appendEntries(const AppendEntriesRequest &request, AppendEntriesResponse *response)
        -> bool;
    auto requestVote(const RequestVoteRequest &request, RequestVoteResponse *response) -> bool;

    [[nodiscard]] auto id() const -> id_t;
    [[nodiscard]] auto ip() const -> ip_t;

  private:
    node_config_t                               m_config{};
    std::unique_ptr<RaftService::StubInterface> m_stub{nullptr};
};

class consensus_module_t : public RaftService::Service
{
  public:
    consensus_module_t() = delete;
    consensus_module_t(node_config_t                        nodeConfig,
                       std::vector<raft_node_grpc_client_t> replicas) noexcept;

    consensus_module_t(const consensus_module_t &) = delete;
    consensus_module_t &operator=(const consensus_module_t &) = delete;

    consensus_module_t(consensus_module_t &&) = delete;
    consensus_module_t &operator=(consensus_module_t &&) = delete;

    ~consensus_module_t() override = default;

    [[nodiscard]] auto init() -> bool;
    void               start();
    void               stop();

    [[nodiscard]] grpc::Status AppendEntries(grpc::ServerContext        *pContext,
                                             const AppendEntriesRequest *pRequest,
                                             AppendEntriesResponse      *pResponse) override
        ABSL_LOCKS_EXCLUDED(m_stateMutex);

    [[nodiscard]] grpc::Status RequestVote(grpc::ServerContext      *pContext,
                                           const RequestVoteRequest *pRequest,
                                           RequestVoteResponse      *pResponse) override
        ABSL_LOCKS_EXCLUDED(m_stateMutex);

    [[nodiscard]] auto replicate(LogEntry logEntry) -> bool;

    [[nodiscard]] std::uint32_t         currentTerm() const;
    [[nodiscard]] id_t                  votedFor() const;
    [[nodiscard]] std::vector<LogEntry> log() const;
    [[nodiscard]] NodeState             getState() const ABSL_SHARED_LOCKS_REQUIRED(m_stateMutex);

  private:
    // ---- State transitions ----
    void becomeFollower(uint32_t newTerm) ABSL_EXCLUSIVE_LOCKS_REQUIRED(m_stateMutex);
    void becomeLeader() ABSL_EXCLUSIVE_LOCKS_REQUIRED(m_stateMutex);
    // --------

    // ---- Heartbeat ----
    void runHeartbeatThread(std::stop_token token);
    auto waitForHeartbeat(std::stop_token token) -> bool;
    void sendAppendEntriesRPC(raft_node_grpc_client_t &client, std::vector<LogEntry> logEntries);
    auto onSendAppendEntriesRPC(raft_node_grpc_client_t     &client,
                                const AppendEntriesResponse &response) noexcept -> bool;
    // --------

    // ---- Leader election ----
    void runElectionThread(std::stop_token token) noexcept;
    void startElection();
    void sendRequestVoteRPCs(const RequestVoteRequest &request, std::uint64_t newTerm);
    // --------

    // ---- Constant utility methods ----
    [[nodiscard]] uint32_t getLastLogIndex() const ABSL_SHARED_LOCKS_REQUIRED(m_stateMutex);
    [[nodiscard]] uint32_t getLastLogTerm() const ABSL_SHARED_LOCKS_REQUIRED(m_stateMutex);
    [[nodiscard]] auto     hasMajority(uint32_t votes) const -> bool;
    [[nodiscard]] uint32_t getLogTerm(uint32_t index) const
        ABSL_EXCLUSIVE_LOCKS_REQUIRED(m_stateMutex);
    [[nodiscard]] uint32_t findMajorityIndexMatch() const ABSL_SHARED_LOCKS_REQUIRED(m_stateMutex);
    [[nodiscard]] auto     waitForMajorityReplication(uint32_t logIndex) const -> bool;
    // --------

    // ---- Persistent state management ----
    // TODO(lnikon): Move into separate class
    [[nodiscard]] bool initializePersistentState() ABSL_EXCLUSIVE_LOCKS_REQUIRED(m_stateMutex);
    [[nodiscard]] bool updatePersistentState(std::optional<std::uint32_t> commitIndex,
                                             std::optional<std::uint32_t> votedFor)
        ABSL_EXCLUSIVE_LOCKS_REQUIRED(m_stateMutex);

    [[nodiscard]] bool flushPersistentState() ABSL_EXCLUSIVE_LOCKS_REQUIRED(m_stateMutex);
    [[nodiscard]] bool restorePersistentState() ABSL_EXCLUSIVE_LOCKS_REQUIRED(m_stateMutex);
    // --------

    // ---- Cleanup ----
    void cleanupHeartbeatThread();
    void cleanupElectionThread();
    // --------

    // ---- Member fields ----
    // Map from client ID to a gRPC client.
    std::unordered_map<id_t, std::optional<raft_node_grpc_client_t>> m_replicas;

    // Stores ID and IP of the current node. Received from outside.
    node_config_t m_config;

    // Persistent state on all servers
    mutable absl::Mutex         m_stateMutex;
    uint32_t m_currentTerm      ABSL_GUARDED_BY(m_stateMutex);
    uint32_t m_votedFor         ABSL_GUARDED_BY(m_stateMutex);
    std::vector<LogEntry> m_log ABSL_GUARDED_BY(m_stateMutex);

    // Volatile state on all servers.
    uint32_t m_commitIndex ABSL_GUARDED_BY(m_stateMutex);
    uint32_t m_lastApplied ABSL_GUARDED_BY(m_stateMutex);
    NodeState m_state      ABSL_GUARDED_BY(m_stateMutex);

    // Log replication related fields. Volatile state on leaders.
    std::unordered_map<id_t, uint32_t> m_matchIndex ABSL_GUARDED_BY(m_stateMutex);
    std::unordered_map<id_t, uint32_t> m_nextIndex  ABSL_GUARDED_BY(m_stateMutex);

    // Election related fields
    std::atomic<bool>     m_leaderHeartbeatReceived;
    std::atomic<uint32_t> m_voteCount{0};

    // Threads for election and heartbeat
    std::jthread m_electionThread;
    std::jthread m_heartbeatThread;

    // Used to shutdown the entire consensus module
    bool m_shutdown{false};
};

// NOLINTEND(modernize-use-trailing-return-type)

} // namespace raft

#pragma once

#include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/support/status.h>

#include "Raft.grpc.pb.h"
#include "Raft.pb.h"

#include <absl/base/thread_annotations.h>

#include <chrono>        // for 'std::chrono::high_resolution_clock'
#include <cstdint>       // for 'uint32_t'
#include <string>        // for 'std::string'
#include <thread>        // for 'std::jthread'
#include <memory>        // for 'std::shared_ptr', 'std::unique_ptr'
#include <optional>      // for 'std::optional'
#include <unordered_map> // for 'std::unordered_map'
#include <vector>        // for 'std::vector'

namespace raft
{

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
// class tkvpp_node_grpc_client_t
// {
//   public:
//     /**
//      * Constructs a client for communicating with a specific node.
//      * @param nodeId Unique identifier for the target node
//      * @param nodeIp IP address of the target node
//      * @throws std::runtime_error if connection cannot be established
//      */
//     tkvpp_node_grpc_client_t(node_config_t config, std::unique_ptr<TinyKVPPService::StubInterface> pRaftStub);
//     virtual ~tkvpp_node_grpc_client_t() noexcept = default;
//
//     tkvpp_node_grpc_client_t(const tkvpp_node_grpc_client_t &) = delete;
//     auto operator=(const tkvpp_node_grpc_client_t &) -> tkvpp_node_grpc_client_t & = delete;
//
//     tkvpp_node_grpc_client_t(tkvpp_node_grpc_client_t &&) = default;
//     auto operator=(tkvpp_node_grpc_client_t &&) -> tkvpp_node_grpc_client_t & = default;
//
//     auto put(const PutRequest &request, PutResponse *pResponse) -> bool;
//
//     [[nodiscard]] auto id() const -> id_t;
//     [[nodiscard]] auto ip() const -> ip_t;
//
//   private:
//     node_config_t                                   m_config{};
//     std::unique_ptr<TinyKVPPService::StubInterface> m_stub{nullptr};
// };

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
    raft_node_grpc_client_t(node_config_t config, std::unique_ptr<RaftService::StubInterface> pRaftStub);
    virtual ~raft_node_grpc_client_t() noexcept = default;

    raft_node_grpc_client_t(const raft_node_grpc_client_t &) = delete;
    auto operator=(const raft_node_grpc_client_t &) -> raft_node_grpc_client_t & = delete;

    raft_node_grpc_client_t(raft_node_grpc_client_t &&) = default;
    auto operator=(raft_node_grpc_client_t &&) -> raft_node_grpc_client_t & = default;

    auto appendEntries(const AppendEntriesRequest &request, AppendEntriesResponse *response) -> bool;
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
    // @id is the ID of the current node. Order of RaftServices in @replicas is important!
    consensus_module_t(node_config_t nodeConfig, std::vector<raft_node_grpc_client_t> replicas);

    // NOLINTBEGIN(modernize-use-trailing-return-type)
    grpc::Status AppendEntries(grpc::ServerContext        *pContext,
                               const AppendEntriesRequest *pRequest,
                               AppendEntriesResponse      *pResponse) override ABSL_LOCKS_EXCLUDED(m_stateMutex);

    auto RequestVote(grpc::ServerContext *pContext, const RequestVoteRequest *pRequest, RequestVoteResponse *pResponse)
        -> grpc::Status override ABSL_LOCKS_EXCLUDED(m_stateMutex);
    // NOLINTEND(modernize-use-trailing-return-type)

    [[nodiscard]] auto init() -> bool;
    void               start();
    void               stop();

    auto replicate(LogEntry logEntry) -> bool;

    // NOLINTBEGIN(modernize-use-trailing-return-type)
    [[nodiscard]] std::uint32_t         currentTerm() const;
    [[nodiscard]] id_t                  votedFor() const;
    [[nodiscard]] std::vector<LogEntry> log() const;
    [[nodiscard]] NodeState             getState() ABSL_SHARED_LOCKS_REQUIRED(m_stateMutex);
    // NOLINTEND(modernize-use-trailing-return-type)

  private:
    // Logic behind Leader election and log replication
    void becomeFollower(uint32_t newTerm) ABSL_EXCLUSIVE_LOCKS_REQUIRED(m_stateMutex);
    void becomeLeader() ABSL_EXCLUSIVE_LOCKS_REQUIRED(m_stateMutex);
    void sendHeartbeat(raft_node_grpc_client_t &client) ABSL_EXCLUSIVE_LOCKS_REQUIRED(m_stateMutex);
    auto waitForHeartbeat(std::stop_token token) -> bool;
    void startElection();
    void sendRequestVoteRPCs(const RequestVoteRequest& request, std::uint64_t newTerm);

    void               sendAppendEntriesRPC(raft_node_grpc_client_t &client, std::vector<LogEntry> logEntries);
    [[nodiscard]] auto waitForMajorityReplication(uint32_t logIndex) -> bool;

    // Utility methods
    // NOLINTBEGIN(modernize-use-trailing-return-type)
    [[nodiscard]] bool     initializePersistentState() ABSL_EXCLUSIVE_LOCKS_REQUIRED(m_stateMutex);
    [[nodiscard]] uint32_t getLastLogIndex() const ABSL_SHARED_LOCKS_REQUIRED(m_stateMutex);
    [[nodiscard]] uint32_t getLastLogTerm() const ABSL_SHARED_LOCKS_REQUIRED(m_stateMutex);
    [[nodiscard]] auto     hasMajority(uint32_t votes) const -> bool;
    [[nodiscard]] uint32_t getLogTerm(uint32_t index) const ABSL_EXCLUSIVE_LOCKS_REQUIRED(m_stateMutex);
    [[nodiscard]] uint32_t findMajorityIndexMatch() ABSL_SHARED_LOCKS_REQUIRED(m_stateMutex);

    [[nodiscard]] bool updatePersistentState(std::optional<std::uint32_t> commitIndex,
                                             std::optional<std::uint32_t> votedFor)
        ABSL_EXCLUSIVE_LOCKS_REQUIRED(m_stateMutex);

    [[nodiscard]] bool flushPersistentState() ABSL_EXCLUSIVE_LOCKS_REQUIRED(m_stateMutex);
    [[nodiscard]] bool restorePersistentState() ABSL_EXCLUSIVE_LOCKS_REQUIRED(m_stateMutex);

    void cleanupHeartbeatThreads() ABSL_EXCLUSIVE_LOCKS_REQUIRED(m_stateMutex);
    // NOLINTEND(modernize-use-trailing-return-type)

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

    // No need to guard @m_electionThread as it is managed only within main thread
    std::jthread m_electionThread;

    // Stores clusterSize - 1 thread to send heartbeat to replicas
    std::vector<std::jthread> m_heartbeatThreads ABSL_GUARDED_BY(m_stateMutex);

    // Used to shutdown the entire consensus module
    bool m_shutdown{false};

    // Used to shutdown heartbeat threads
    bool m_shutdownHeartbeatThreads{false};
};

} // namespace raft

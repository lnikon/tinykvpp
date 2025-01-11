#pragma once

#include <wal/wal.h>

#include "Raft.grpc.pb.h"
#include "Raft.pb.h"

#include "TinyKVPP.pb.h"
#include "TinyKVPP.grpc.pb.h"

#include <absl/base/thread_annotations.h>

#include <chrono>  // for 'std::chrono::high_resolution_clock'
#include <cstdint> // for 'uint32_t'
#include <string>  // for 'std::string'
#include <thread>  // for 'std::jthread'

namespace raft
{

using ip_t = std::string;
using id_t = uint32_t;
using clock_t = std::chrono::high_resolution_clock;
using timepoint_t = std::chrono::high_resolution_clock::time_point;

// Valid IDs start from 1
constexpr const id_t gInvalidId = 0;

/**
 * Client for communicating with other nodes in the Raft cluster.
 * Handles RPC operations for consensus and key-value operations.
 */
class node_client_t
{
  public:
    /**
     * Constructs a client for communicating with a specific node.
     * @param nodeId Unique identifier for the target node
     * @param nodeIp IP address of the target node
     * @throws std::runtime_error if connection cannot be established
     */
    node_client_t(id_t nodeId, ip_t nodeIp);
    virtual ~node_client_t() noexcept = default;

    node_client_t(const node_client_t &) = delete;
    auto operator=(const node_client_t &) -> node_client_t & = delete;

    node_client_t(node_client_t &&) = default;
    auto operator=(node_client_t &&) -> node_client_t & = default;

    auto appendEntries(const AppendEntriesRequest &request, AppendEntriesResponse *response) -> bool;
    auto requestVote(const RequestVoteRequest &request, RequestVoteResponse *response) -> bool;

    auto put(const PutRequest &request, PutResponse *pResponse) -> bool;

    [[nodiscard]] auto id() const -> id_t;

  private:
    id_t m_id{gInvalidId};
    ip_t m_ip;

    std::shared_ptr<grpc::ChannelInterface> m_channel{nullptr};
    std::unique_ptr<RaftService::Stub>      m_stub{nullptr};
    std::unique_ptr<TinyKVPPService::Stub>  m_kvStub{nullptr};
};

class consensus_module_t : public RaftService::Service,
                           public TinyKVPPService::Service,
                           public std::enable_shared_from_this<consensus_module_t>
{
  public:
    // @id is the ID of the current node. Order of RaftServices in @replicas is important!
    consensus_module_t(id_t nodeId, std::vector<ip_t> replicas);

    auto AppendEntries(grpc::ServerContext        *pContext,
                       const AppendEntriesRequest *pRequest,
                       AppendEntriesResponse      *pResponse) -> grpc::Status override;

    auto RequestVote(grpc::ServerContext *pContext, const RequestVoteRequest *pRequest, RequestVoteResponse *pResponse)
        -> grpc::Status override;

    auto Put(grpc::ServerContext *pContext, const PutRequest *pRequest, PutResponse *pResponse)
        -> grpc::Status override;

    auto Get(grpc::ServerContext *pContext, const GetRequest *pRequest, GetResponse *pResponse)
        -> grpc::Status override;

    [[nodiscard]] auto init() -> bool;

    void start();

    void stop();

  private:
    // Logic behind Leader election and log replication
    void               startElection();
    void               becomeFollower(uint32_t newTerm) ABSL_EXCLUSIVE_LOCKS_REQUIRED(m_stateMutex);
    void               becomeLeader() ABSL_EXCLUSIVE_LOCKS_REQUIRED(m_stateMutex);
    void               sendHeartbeat(node_client_t &client) ABSL_EXCLUSIVE_LOCKS_REQUIRED(m_stateMutex);
    void               sendAppendEntriesRPC(node_client_t &client, std::vector<LogEntry> logEntries);
    [[nodiscard]] auto waitForMajorityReplication(uint32_t logIndex) -> bool;

    // Utility methods
    // NOLINTBEGIN(modernize-use-trailing-return-type)
    [[nodiscard]] bool      initializePersistentState() ABSL_EXCLUSIVE_LOCKS_REQUIRED(m_stateMutex);
    [[nodiscard]] uint32_t  getLastLogIndex() const ABSL_SHARED_LOCKS_REQUIRED(m_stateMutex);
    [[nodiscard]] uint32_t  getLastLogTerm() const ABSL_SHARED_LOCKS_REQUIRED(m_stateMutex);
    [[nodiscard]] auto      hasMajority(uint32_t votes) const -> bool;
    [[nodiscard]] NodeState getState() ABSL_SHARED_LOCKS_REQUIRED(m_stateMutex);
    [[nodiscard]] uint32_t  getLogTerm(uint32_t index) const ABSL_EXCLUSIVE_LOCKS_REQUIRED(m_stateMutex);
    [[nodiscard]] uint32_t  findMajorityIndexMatch() ABSL_SHARED_LOCKS_REQUIRED(m_stateMutex);

    [[nodiscard]] bool updatePersistentState(std::optional<std::uint32_t> commitIndex,
                                             std::optional<std::uint32_t> votedFor)
        ABSL_EXCLUSIVE_LOCKS_REQUIRED(m_stateMutex);

    [[nodiscard]] bool flushPersistentState() ABSL_EXCLUSIVE_LOCKS_REQUIRED(m_stateMutex);
    [[nodiscard]] bool restorePersistentState() ABSL_EXCLUSIVE_LOCKS_REQUIRED(m_stateMutex);
    // NOLINTEND(modernize-use-trailing-return-type)

    // Map from client ID to a gRPC client.
    std::unordered_map<id_t, std::optional<node_client_t>> m_replicas;

    // Id of the current node. Received from outside.
    id_t m_id{gInvalidId};

    // IP of the current node. Received from outside.
    ip_t m_ip;

    // gRPC server to receive Raft RPCs
    std::unique_ptr<grpc::Server> m_raftServer{nullptr};

    // gRPC server to receive KV RPCs
    std::unique_ptr<grpc::Server> m_kvServer{nullptr};

    // Persistent state on all servers
    absl::Mutex                 m_stateMutex;
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
    absl::Mutex           m_timerMutex;
    std::atomic<bool>     m_leaderHeartbeatReceived{false};
    std::atomic<bool>     m_stopElection{false};
    std::jthread          m_electionThread;
    std::atomic<uint32_t> m_voteCount{0};

    // Stores clusterSize - 1 thread to send heartbeat to replicas
    std::vector<std::jthread> m_heartbeatThreads ABSL_GUARDED_BY(m_stateMutex);

    // Serves incoming RPC's
    std::jthread m_serverThread;

    // Temporary in-memory hashtable to store KVs
    std::unordered_map<std::string, std::string> m_kv;
};

} // namespace raft

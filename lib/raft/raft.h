#pragma once

#include <atomic>
#include <chrono>        // `std::chrono::high_resolution_clock`
#include <cstdint>       // `uint32_t`
#include <future>        // `std::promise`
#include <string>        // `std::string`
#include <thread>        // `std::jthread`
#include <memory>        // `std::shared_ptr`, `std::unique_ptr`
#include <optional>      // `std::optional`
#include <unordered_map> // `std::unordered_map`
#include <vector>        // `std::vector`

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
#include "concurrency/thread_pool.h"

template <typename TStream> auto operator<<(TStream &stream, const LogEntry &record) -> TStream &
{
    stream << record.SerializeAsString();

    // TODO(lnikon): Uncomment when protobuf serialization is implemented. Protobuf serializes into
    // binary format by default.
    // record.SerializeToOstream(&stream);

    return stream;
}

template <typename TStream> auto operator>>(TStream &stream, LogEntry &record) -> TStream &
{
    record.ParseFromString(stream.str());

    // TODO(lnikon): Uncomment when protobuf serialization is implemented. Protobuf serializes into
    // binary format by default.
    // record.ParseFromIstream(&stream);

    return stream;
}

namespace wal
{
template <typename TEntry> class wal_t;
};

namespace raft
{

// NOLINTBEGIN(modernize-use-trailing-return-type)

using ip_t = std::string;
using id_t = uint32_t;
using clock_t = std::chrono::high_resolution_clock;
using timepoint_t = std::chrono::high_resolution_clock::time_point;

// Valid IDs start from 1
constexpr const id_t gInvalidId = 0;

// Configuration for a Raft node
struct node_config_t
{
    id_t m_id{gInvalidId};
    ip_t m_ip;
};

enum class raft_operation_status_k : int8_t
{
    unknown_k = -1,
    success_k,
    failure_k,
    timeout_k,
    invalid_request_k,
    not_leader_k,
    leader_not_found_k,
    wal_add_failed_k,
    replicate_failed_k,
    forward_failed_k,
};

class raft_node_grpc_client_t
{
  public:
    raft_node_grpc_client_t(
        node_config_t config, std::unique_ptr<RaftService::StubInterface> pRaftStub
    );
    virtual ~raft_node_grpc_client_t() noexcept = default;

    raft_node_grpc_client_t(const raft_node_grpc_client_t &) = delete;
    auto operator=(const raft_node_grpc_client_t &) -> raft_node_grpc_client_t & = delete;

    raft_node_grpc_client_t(raft_node_grpc_client_t &&) = default;
    auto operator=(raft_node_grpc_client_t &&) -> raft_node_grpc_client_t & = default;

    [[nodiscard]] auto
    appendEntries(const AppendEntriesRequest &request, AppendEntriesResponse *response) -> bool;

    [[nodiscard]] auto requestVote(const RequestVoteRequest &request, RequestVoteResponse *response)
        -> bool;

    [[nodiscard]] auto
    replicate(const ReplicateEntriesRequest &request, ReplicateEntriesResponse *response) -> bool;

    [[nodiscard]] auto id() const -> id_t;
    [[nodiscard]] auto ip() const -> ip_t;

  private:
    node_config_t                               m_config{};
    std::unique_ptr<RaftService::StubInterface> m_stub{nullptr};
};

class consensus_module_t final : public RaftService::Service
{
  public:
    using wal_entry_t = LogEntry;
    using wal_ptr_t = std::shared_ptr<wal::wal_t<wal_entry_t>>;
    using on_commit_cbk_t = std::function<bool(const LogEntry &)>;
    using on_leader_change_cbk_t = std::function<void(bool)>;

    consensus_module_t() = delete;
    consensus_module_t(
        node_config_t nodeConfig, std::vector<raft_node_grpc_client_t> replicas, wal_ptr_t pWal
    ) noexcept;

    consensus_module_t(const consensus_module_t &) = delete;
    consensus_module_t &operator=(const consensus_module_t &) = delete;

    consensus_module_t(consensus_module_t &&) = delete;
    consensus_module_t &operator=(consensus_module_t &&) = delete;

    ~consensus_module_t() override;

    [[nodiscard]] auto init() -> bool;
    void               start();
    void               stop();

    // Executed on the follower
    [[nodiscard]] grpc::Status AppendEntries(
        grpc::ServerContext        *pContext,
        const AppendEntriesRequest *pRequest,
        AppendEntriesResponse      *pResponse
    ) override ABSL_LOCKS_EXCLUDED(m_stateMutex);

    // Executed on the follower
    [[nodiscard]] grpc::Status RequestVote(
        grpc::ServerContext      *pContext,
        const RequestVoteRequest *pRequest,
        RequestVoteResponse      *pResponse
    ) override ABSL_LOCKS_EXCLUDED(m_stateMutex);

    // Executed on the leader
    [[nodiscard]] grpc::Status Replicate(
        grpc::ServerContext           *pContext,
        const ReplicateEntriesRequest *pRequest,
        ReplicateEntriesResponse      *pResponse
    ) override ABSL_LOCKS_EXCLUDED(m_stateMutex);

    [[nodiscard]] [[deprecated]] auto replicate(std::string payload) -> raft_operation_status_k;
    [[nodiscard]] auto                replicateAsync(std::string payload) -> std::future<bool>;

    [[nodiscard]] std::uint32_t         currentTerm() const;
    [[nodiscard]] id_t                  votedFor() const;
    [[nodiscard]] std::vector<LogEntry> log() const;
    [[nodiscard]] NodeState             getState() const ABSL_SHARED_LOCKS_REQUIRED(m_stateMutex);
    [[nodiscard]] NodeState             getStateSafe() const ABSL_LOCKS_EXCLUDED(m_stateMutex);
    [[nodiscard]] bool                  isLeader() const ABSL_LOCKS_EXCLUDED(m_stateMutex);
    [[nodiscard]] std::string           getLeaderHint() const ABSL_LOCKS_EXCLUDED(m_stateMutex);

    void setOnCommitCallback(on_commit_cbk_t onCommitCbk);
    void setOnLeaderChangeCallback(on_leader_change_cbk_t onLeaderChangeCbk);

  private:
    // ---- State transitions ----
    void becomeFollower(uint32_t newTerm) ABSL_EXCLUSIVE_LOCKS_REQUIRED(m_stateMutex);
    void becomeLeader() ABSL_EXCLUSIVE_LOCKS_REQUIRED(m_stateMutex);
    // --------

    // ---- Heartbeat ----
    void runHeartbeatThread(std::stop_token token);
    auto waitForHeartbeat(std::stop_token token) -> bool;
    void sendAppendEntriesAsync(raft_node_grpc_client_t &client, std::vector<LogEntry> logEntries);
    auto onSendAppendEntriesRPC(
        raft_node_grpc_client_t &client, const AppendEntriesResponse &response
    ) noexcept -> bool;
    // --------

    // ---- Leader election ----
    void runElectionThread(std::stop_token token) noexcept;
    void startElection();
    void sendRequestVoteRPCs(const RequestVoteRequest &request, std::uint64_t newTerm);
    void sendRequestVoteAsync(const RequestVoteRequest &request, raft_node_grpc_client_t &client);
    // --------

    // ---- Constant utility methods ----
    [[nodiscard]] uint32_t getLastLogIndex() const ABSL_SHARED_LOCKS_REQUIRED(m_stateMutex);
    [[nodiscard]] uint32_t getLastLogTerm() const ABSL_SHARED_LOCKS_REQUIRED(m_stateMutex);
    [[nodiscard]] auto     hasMajority(uint32_t votes) const -> bool;
    [[nodiscard]] uint32_t
    getLogTerm(uint32_t index) const ABSL_EXCLUSIVE_LOCKS_REQUIRED(m_stateMutex);
    [[nodiscard]] uint32_t findMajorityIndexMatch() const ABSL_SHARED_LOCKS_REQUIRED(m_stateMutex);
    [[nodiscard]] auto     waitForMajorityReplication(uint32_t logIndex) const -> bool;
    // --------

    // ---- Persistent state management ----
    // TODO(lnikon): Move into separate class
    [[nodiscard]] bool initializePersistentState() ABSL_EXCLUSIVE_LOCKS_REQUIRED(m_stateMutex);
    [[nodiscard]] bool updatePersistentState(
        std::optional<std::uint32_t> commitIndex, std::optional<std::uint32_t> votedFor
    ) ABSL_EXCLUSIVE_LOCKS_REQUIRED(m_stateMutex);

    [[nodiscard]] bool flushPersistentState() ABSL_EXCLUSIVE_LOCKS_REQUIRED(m_stateMutex);
    [[nodiscard]] bool restorePersistentState() ABSL_EXCLUSIVE_LOCKS_REQUIRED(m_stateMutex);
    // --------

    // ---- Async helpers ----
    void monitorPendingReplications();
    void applyCommittedEntries() ABSL_EXCLUSIVE_LOCKS_REQUIRED(m_stateMutex);
    // --------

    // ---- Cleanup ----
    void cleanupHeartbeatThread();
    void cleanupElectionThread();
    // --------

    // ---- Async replication tracking ----
    struct pending_replication_t
    {
        std::promise<bool>                    promise;
        uint32_t                              logIndex;
        std::chrono::steady_clock::time_point deadline;
    };

    mutable absl::Mutex m_pendingMutex;
    absl::flat_hash_map<uint64_t, pending_replication_t>
        m_pendingReplications ABSL_GUARDED_BY(m_pendingMutex);
    std::atomic<uint64_t>     m_replicationIdCounter{0};
    std::thread               m_replicationMonitor;
    // --------

    // ---- Thread pools ----
    std::unique_ptr<concurrency::thread_pool_t> m_rpcThreadPool;
    std::unique_ptr<concurrency::thread_pool_t> m_internalThreadPool;
    // --------

    // Map from client ID to a gRPC client.
    std::unordered_map<id_t, std::optional<raft_node_grpc_client_t>> m_replicas;

    // Current leader tracking
    mutable absl::Mutex             m_leaderMutex;
    std::string m_currentLeaderHint ABSL_GUARDED_BY(m_leaderMutex);

    // Stores ID and IP of the current node. Received from outside.
    node_config_t m_config;

    // Executed on follower node after successful log replication to update the state machine.
    on_commit_cbk_t        m_onCommitCbk;
    on_leader_change_cbk_t m_onLeaderChangeCbk;

    // Persistent state on all servers
    mutable absl::Mutex    m_stateMutex;
    uint32_t m_currentTerm ABSL_GUARDED_BY(m_stateMutex);
    uint32_t m_votedFor    ABSL_GUARDED_BY(m_stateMutex);
    wal_ptr_t m_log        ABSL_GUARDED_BY(m_stateMutex);

    // Volatile state on all servers.
    uint32_t m_commitIndex              ABSL_GUARDED_BY(m_stateMutex);
    std::atomic<uint32_t> m_lastApplied ABSL_GUARDED_BY(m_stateMutex);
    NodeState m_state                   ABSL_GUARDED_BY(m_stateMutex);

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
    std::atomic<bool> m_shutdown{false};
};

// NOLINTEND(modernize-use-trailing-return-type)

} // namespace raft

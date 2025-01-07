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

using IP = std::string;
using ID = uint32_t;
using Clock = std::chrono::high_resolution_clock;
using TimePoint = std::chrono::high_resolution_clock::time_point;

// Valid IDs start from 1
constexpr const ID invalidId = 0;

auto generateRandomTimeout() -> int;

class NodeClient
{
  public:
    NodeClient(ID nodeId, IP nodeIp);

    auto appendEntries(const AppendEntriesRequest &request, AppendEntriesResponse *response) -> bool;
    auto requestVote(const RequestVoteRequest &request, RequestVoteResponse *response) -> bool;

    auto put(const PutRequest &request, PutResponse *pResponse) -> bool;

    [[nodiscard]] auto getId() const -> ID;

  private:
    ID m_id{invalidId};
    IP m_ip;

    std::shared_ptr<grpc::ChannelInterface> m_channel{nullptr};
    std::unique_ptr<RaftService::Stub>      m_stub{nullptr};
    std::unique_ptr<TinyKVPPService::Stub>  m_kvStub{nullptr};
};

class ConsensusModule : public RaftService::Service,
                        public TinyKVPPService::Service,
                        std::enable_shared_from_this<ConsensusModule>
{
  public:
    // @id is the ID of the current node. Order of RaftServices in @replicas is important!
    ConsensusModule(ID nodeId, std::vector<IP> replicas);

    auto AppendEntries(grpc::ServerContext        *pContext,
                       const AppendEntriesRequest *pRequest,
                       AppendEntriesResponse      *pResponse) -> grpc::Status override;

    auto RequestVote(grpc::ServerContext *pContext, const RequestVoteRequest *pRequest, RequestVoteResponse *pResponse)
        -> grpc::Status override;

    auto Put(grpc::ServerContext *pContext, const PutRequest *pRequest, PutResponse *pResponse)
        -> grpc::Status override;

    auto Get(grpc::ServerContext *pContext, const GetRequest *pRequest, GetResponse *pResponse)
        -> grpc::Status override;

    auto init() -> bool;

    void start();

    void startServer();

    void stop();

  private:
    // State initialization
    bool initializePersistentState() ABSL_EXCLUSIVE_LOCKS_REQUIRED(m_stateMutex);

    // The logic behind election
    void startElection();
    void becomeFollower(uint32_t newTerm) ABSL_EXCLUSIVE_LOCKS_REQUIRED(m_stateMutex);
    auto hasMajority(uint32_t votes) const -> bool;
    void becomeLeader() ABSL_EXCLUSIVE_LOCKS_REQUIRED(m_stateMutex);
    void sendHeartbeat(NodeClient &client) ABSL_EXCLUSIVE_LOCKS_REQUIRED(m_stateMutex);
    void sendAppendEntriesRPC(NodeClient &client, std::vector<LogEntry> logEntries);

    // NOLINTBEGIN(modernize-use-trailing-return-type)
    [[nodiscard]] uint32_t  getLastLogIndex() const ABSL_SHARED_LOCKS_REQUIRED(m_stateMutex);
    [[nodiscard]] uint32_t  getLastLogTerm() const ABSL_SHARED_LOCKS_REQUIRED(m_stateMutex);
    [[nodiscard]] NodeState getState() ABSL_SHARED_LOCKS_REQUIRED(m_stateMutex);
    uint32_t                getLogTerm(uint32_t index) const ABSL_EXCLUSIVE_LOCKS_REQUIRED(m_stateMutex);
    uint32_t                findMajorityIndexMatch() ABSL_SHARED_LOCKS_REQUIRED(m_stateMutex);
    bool                    waitForMajorityReplication(uint32_t logIndex); // ABSL_SHARED_LOCKS_REQUIRED(m_stateMutex);

    bool updatePersistentState(std::optional<std::uint32_t> commitIndex, std::optional<std::uint32_t> votedFor)
        ABSL_EXCLUSIVE_LOCKS_REQUIRED(m_stateMutex);
    [[nodiscard]] bool flushPersistentState() ABSL_EXCLUSIVE_LOCKS_REQUIRED(m_stateMutex);
    [[nodiscard]] bool restorePersistentState() ABSL_EXCLUSIVE_LOCKS_REQUIRED(m_stateMutex);
    // NOLINTEND(modernize-use-trailing-return-type)

    // Id of the current node. Received from outside.
    ID m_id{invalidId};

    // IP of the current node. Received from outside.
    IP m_ip;

    // gRPC server to receive Raft RPCs
    std::unique_ptr<grpc::Server> m_raftServer{nullptr};

    // gRPC server to receive KV RPCs
    std::unique_ptr<grpc::Server> m_kvServer{nullptr};

    // Persistent state on all servers
    absl::Mutex                 m_stateMutex;
    uint32_t m_currentTerm      ABSL_GUARDED_BY(m_stateMutex);
    uint32_t m_votedFor         ABSL_GUARDED_BY(m_stateMutex);
    NodeState m_state           ABSL_GUARDED_BY(m_stateMutex);
    std::vector<LogEntry> m_log ABSL_GUARDED_BY(m_stateMutex);
    /*db::wal::wal_t m_wal        ABSL_GUARDED_BY(m_stateMutex);*/

    // Volatile state on all servers. Reseted on each server start.
    uint32_t m_commitIndex ABSL_GUARDED_BY(m_stateMutex);
    uint32_t m_lastApplied ABSL_GUARDED_BY(m_stateMutex);

    // Log replication related fields
    std::unordered_map<ID, std::optional<NodeClient>> m_replicas;
    std::unordered_map<ID, uint32_t> m_matchIndex     ABSL_GUARDED_BY(m_stateMutex);
    std::unordered_map<ID, uint32_t> m_nextIndex      ABSL_GUARDED_BY(m_stateMutex);

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

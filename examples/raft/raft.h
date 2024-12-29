#pragma once

#include "Raft.grpc.pb.h"
#include "Raft.pb.h"

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
    NodeClient(const ID id, const IP ip);

    auto appendEntries(const AppendEntriesRequest &request, AppendEntriesResponse *response) -> bool;
    auto requestVote(const RequestVoteRequest &request, RequestVoteResponse *response) -> bool;

    [[nodiscard]] auto getId() const -> ID;

  private:
    ID m_id{invalidId};
    IP m_ip;

    std::shared_ptr<grpc::ChannelInterface> m_channel{nullptr};
    std::unique_ptr<RaftService::Stub>      m_stub{nullptr};
};

class ConsensusModule : public RaftService::Service, std::enable_shared_from_this<ConsensusModule>
{
  public:
    // @id is the ID of the current node. Order of RaftServices in @replicas is important!
    ConsensusModule(const ID id, std::vector<IP> replicas);

    auto AppendEntries(grpc::ServerContext        *pContext,
                       const AppendEntriesRequest *pRequest,
                       AppendEntriesResponse      *pResponse) -> grpc::Status override;

    auto RequestVote(grpc::ServerContext *pContext, const RequestVoteRequest *pRequest, RequestVoteResponse *pResponse)
        -> grpc::Status override;

    auto init() -> bool;

    void start();

    void startServer();

    void stop();

  private:
    // State initialization
    auto initializePersistentState() -> bool;
    auto initializeVolatileState() -> bool;

    // Timer handling
    // Called every time 'AppendEntries' received.
    void resetElectionTimer();

    // The logic behind election
    void startElection();
    void becomeFollower(const uint32_t newTerm) ABSL_EXCLUSIVE_LOCKS_REQUIRED(m_stateMutex);
    auto hasMajority(const uint32_t votes) const -> bool;
    void becomeLeader() ABSL_EXCLUSIVE_LOCKS_REQUIRED(m_stateMutex);
    void sendHeartbeat(NodeClient &client) ABSL_EXCLUSIVE_LOCKS_REQUIRED(m_stateMutex);
    void decrementNextIndex(ID id);

    // NOLINTBEGIN(modernize-use-trailing-return-type)
    [[nodiscard]] uint32_t  getLastLogIndex() const ABSL_SHARED_LOCKS_REQUIRED(m_stateMutex);
    [[nodiscard]] uint32_t  getLastLogTerm() const ABSL_SHARED_LOCKS_REQUIRED(m_stateMutex);
    [[nodiscard]] NodeState getState() ABSL_SHARED_LOCKS_REQUIRED(m_stateMutex);
    // NOLINTEND(modernize-use-trailing-return-type)

    // Id of the current node. Received from outside.
    ID m_id{invalidId};

    // IP of the current node. Received from outside.
    IP m_ip;

    // gRPC server for the current node
    std::unique_ptr<grpc::Server> m_server;

    // Persistent state on all servers
    absl::Mutex m_stateMutex    ABSL_ACQUIRED_AFTER(m_timerMutex);
    uint32_t m_currentTerm      ABSL_GUARDED_BY(m_stateMutex);
    uint32_t m_votedFor         ABSL_GUARDED_BY(m_stateMutex);
    NodeState m_state           ABSL_GUARDED_BY(m_stateMutex);
    std::vector<LogEntry> m_log ABSL_GUARDED_BY(m_stateMutex);

    // Leader specific members
    std::vector<std::jthread> m_heartbeatThreads ABSL_GUARDED_BY(m_stateMutex);

    // Volatile state on all servers. Reseted on each server start.
    uint32_t m_commitIndex{0};
    uint32_t m_lastApplied{0};

    // Volatile state on leaders
    std::unordered_map<ID, NodeClient> m_replicas;
    std::vector<uint32_t>              m_nextIndex;
    std::vector<uint32_t>              m_matchIndex;

    // Election and election timer related fields.
    absl::Mutex m_timerMutex ABSL_ACQUIRED_BEFORE(m_stateMutex);
    std::atomic<bool>        m_leaderHeartbeatReceived{false};
    absl::CondVar            m_timerCV;
    std::atomic<bool>        m_stopElectionTimer{false};
    std::atomic<int>         m_electionTimeout{generateRandomTimeout()};
    TimePoint                m_electionTimeoutResetTime{Clock::now()};
    std::jthread             m_electionTimerThread;
    std::atomic<uint32_t>    m_voteCount{0};
    std::atomic<bool>        m_electionInProgress{false};

    // TODO[lnikon]: Use this to serve RPC's in a different thread
    std::jthread m_serverThread;
};

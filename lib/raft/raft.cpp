#include <algorithm>
#include <chrono>
#include <exception>
#include <filesystem>
#include <fmt/core.h>
#include <fstream>
#include <magic_enum/magic_enum.hpp>
#include <random>
#include <ranges>
#include <thread>
#include <utility>

#include <absl/synchronization/mutex.h>
#include <absl/time/time.h>
#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <libassert/assert.hpp>

#include "raft/raft.h"
#include "concurrency/thread_pool.h"
#include "wal/wal.h"

namespace
{

const std::string_view gRaftFilename = "RAFT_PERSISTENCE";

auto constructFilename(std::string_view filename, std::uint32_t peerId) -> std::string
{
    return fmt::format("./var/tkvpp/{}_NODE_{}", filename, peerId);
}

auto generate_random_timeout(const int minTimeout, const int maxTimeout) -> int
{
    static thread_local std::random_device randomDevice;
    static thread_local std::mt19937       gen(randomDevice());

    std::uniform_int_distribution<> dist(minTimeout, maxTimeout);
    return dist(gen);
}

auto generate_raft_timeout()
{
    constexpr const int minTimeout{150};
    constexpr const int maxTimeout{300};

    return generate_random_timeout(minTimeout, maxTimeout);
}

} // namespace

namespace consensus
{

// -------------------------------
// raft_node_grpc_client_t
// -------------------------------
raft_node_grpc_client_t::raft_node_grpc_client_t(
    node_config_t config, std::unique_ptr<raft::v1::RaftService::StubInterface> pRaftStub
)
    : m_config{std::move(config)},
      m_stub{std::move(pRaftStub)}
{
    ASSERT(m_config.m_id != 0);
    ASSERT(!m_config.m_ip.empty());
}

auto raft_node_grpc_client_t::appendEntries(
    const raft::v1::AppendEntriesRequest &request, raft::v1::AppendEntriesResponse *response
) -> bool
{
    const auto          rpc_timeout_duration = std::chrono::milliseconds(generate_raft_timeout());
    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + rpc_timeout_duration);
    grpc::Status status = m_stub->AppendEntries(&context, request, response);
    if (!status.ok())
    {
        spdlog::error(
            "AppendEntries RPC to peer id={} ip={} failed. Error "
            "code={} message={}",
            m_config.m_id,
            m_config.m_ip,
            static_cast<int>(status.error_code()),
            status.error_message()
        );
        return false;
    }

    return true;
}

auto raft_node_grpc_client_t::requestVote(
    const raft::v1::RequestVoteRequest &request, raft::v1::RequestVoteResponse *response
) -> bool
{
    const auto rpc_timeout_duration = std::chrono::milliseconds(generate_raft_timeout());

    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + rpc_timeout_duration);

    grpc::Status status = m_stub->RequestVote(&context, request, response);
    if (!status.ok())
    {
        spdlog::error(
            "RequestVote RPC to peer id={} ip={} failed. Error "
            "code={} message={}",
            m_config.m_id,
            m_config.m_ip,
            static_cast<int>(status.error_code()),
            status.error_message()
        );
        return false;
    }

    return true;
}

[[nodiscard]] auto raft_node_grpc_client_t::replicate(
    const raft::v1::ReplicateRequest &request, raft::v1::ReplicateResponse *response
) -> bool
{
    const auto rpc_timeout_duration = std::chrono::milliseconds(generate_raft_timeout());

    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + rpc_timeout_duration);

    grpc::Status status = m_stub->Replicate(&context, request, response);
    if (!status.ok())
    {
        spdlog::error(
            "Replicate RPC to peer id={} ip={} failed. Error "
            "code={} message={}",
            m_config.m_id,
            m_config.m_ip,
            static_cast<int>(status.error_code()),
            status.error_message()
        );
        return false;
    }

    return true;
}

auto raft_node_grpc_client_t::id() const -> id_t
{
    return m_config.m_id;
}

auto raft_node_grpc_client_t::ip() const -> ip_t
{
    return m_config.m_ip;
}

// -------------------------------
// consensus_module_t
// -------------------------------
consensus_module_t::consensus_module_t(
    node_config_t nodeConfig, std::vector<raft_node_grpc_client_t> replicas, wal_ptr_t pWal
) noexcept
    : m_rpcThreadPool{std::make_unique<concurrency::thread_pool_t>(2, "raft_rpc")},
      m_internalThreadPool{std::make_unique<concurrency::thread_pool_t>(2, "raft_internal")},
      m_config{std::move(nodeConfig)},
      m_currentTerm{0},
      m_votedFor{0},
      m_log{std::move(pWal)},
      m_commitIndex{0},
      m_lastApplied{0},
      m_state{raft::v1::NodeState::NODE_STATE_FOLLOWER},
      m_leaderHeartbeatReceived{false},
      m_voteCount{0}
{
    ASSERT(m_config.m_id != 0);
    ASSERT(m_config.m_id <= replicas.size() + 1);

    for (auto &&nodeClient : replicas)
    {
        m_matchIndex[nodeClient.id()] = 0;
        m_nextIndex[nodeClient.id()] = 1;
        m_replicas[nodeClient.id()] = std::move(nodeClient);
    }
}

consensus_module_t::~consensus_module_t()
{
    {
        absl::WriterMutexLock locker{&m_stateMutex};
        if (m_shutdown)
        {
            return;
        }
    }

    stop();
}

auto consensus_module_t::init() -> bool
{
    absl::WriterMutexLock locker{&m_stateMutex};
    if (!initializePersistentState())
    {
        spdlog::warn("Unable to initialize persistent state!");
        return false;
    }

    m_replicationMonitor = std::thread([this] { monitorPendingReplications(); });

    return true;
}

void consensus_module_t::start()
{
    absl::WriterMutexLock locker{&m_stateMutex};
    m_electionThread = std::jthread([this](std::stop_token token) { runElectionThread(token); });
}

void consensus_module_t::stop()
{
    spdlog::info("Shutting down consensus module");

    {
        absl::WriterMutexLock locker{&m_stateMutex};
        m_shutdown = true;
    }

    cleanupHeartbeatThread();
    cleanupElectionThread();

    // Stop replication monitor
    if (m_replicationMonitor.joinable())
    {
        m_replicationMonitor.join();
    }

    // Shutdown thread pools
    if (m_rpcThreadPool)
    {
        spdlog::info("Shutting down RPC thread pool (queue size: {})", m_rpcThreadPool->size());
        m_rpcThreadPool->shutdown();
    }

    if (m_internalThreadPool)
    {
        spdlog::info(
            "Shutting down internal thread pool (queue size: {})", m_internalThreadPool->size()
        );
        m_internalThreadPool->shutdown();
    }

    // Complete any pending replications
    {
        absl::WriterMutexLock locker{&m_pendingMutex};
        for (auto &[id, pending] : m_pendingReplications)
        {
            (void)id;
            pending.promise.set_value(false);
        }
        m_pendingReplications.clear();
    }

    spdlog::info("Consensus module stopped gracefully!");
}

auto consensus_module_t::AppendEntries(
    grpc::ServerContext                  *pContext,
    const raft::v1::AppendEntriesRequest *pRequest,
    raft::v1::AppendEntriesResponse      *pResponse
) -> grpc::Status
{
    (void)pContext;

    spdlog::debug(
        "Node={} recevied AppendEntries RPC from leader={} during term={}",
        m_config.m_id,
        pRequest->sender_id(),
        pRequest->term()
    );

    if (pRequest->term() >= currentTerm())
    {
        // Find the leader's IP from our replica list
        std::string leaderIp;
        for (const auto &[id, client] : m_replicas)
        {
            if (id == pRequest->sender_id() && client.has_value())
            {
                leaderIp = client->ip();
            }
        }

        if (leaderIp.empty() && pRequest->sender_id() == m_config.m_id)
        {
            leaderIp = m_config.m_ip;
        }

        if (!leaderIp.empty())
        {
            absl::WriterMutexLock locker{&m_leaderMutex};
            m_currentLeaderHint = leaderIp;
        }
    }

    absl::WriterMutexLock locker(&m_stateMutex);

    // 1. Term check
    if (pRequest->term() < m_currentTerm)
    {
        spdlog::debug(
            "Node={} receviedTerm={} is smaller than currentTerm={}",
            m_config.m_id,
            pRequest->term(),
            m_currentTerm
        );
        pResponse->set_term(m_currentTerm);
        pResponse->set_success(false);
        pResponse->set_responder_id(m_config.m_id);
        return grpc::Status::OK;
    }

    if (pRequest->term() > m_currentTerm)
    {
        spdlog::debug(
            "Node={} receviedTerm={} is higher than currentTerm={}. "
            "Reverting to follower",
            m_config.m_id,
            pRequest->term(),
            m_currentTerm
        );
        becomeFollower(pRequest->term());
    }

    // 2. Log consistency check
    if (pRequest->prev_log_index() > 0)
    {
        const auto &logEntry{m_log->read(pRequest->prev_log_index() - 1)};
        if (!logEntry.has_value())
        {
            spdlog::error(
                "Node={} received prevlogindex={} which does not exist. logSize={}",
                m_config.m_id,
                pRequest->prev_log_index(),
                m_log->size()
            );

            pResponse->set_term(m_currentTerm);
            pResponse->set_success(false);
            pResponse->set_responder_id(m_config.m_id);
            return grpc::Status::OK;
        }

        if (logEntry->term() != pRequest->prev_log_term())
        {
            spdlog::error(
                "Node={} received prevlogindex={} with mismatched prevlogterm={}. Current log "
                "term={}",
                m_config.m_id,
                pRequest->prev_log_index(),
                pRequest->prev_log_term(),
                logEntry->term()
            );

            pResponse->set_term(m_currentTerm);
            pResponse->set_success(false);
            pResponse->set_responder_id(m_config.m_id);
            return grpc::Status::OK;
        }
    }

    // 3. Append new entries and remove conflicting ones
    auto newEntryStart = pRequest->prev_log_index() + 1;
    for (auto idx{0}; idx < pRequest->entries().size(); ++idx)
    {
        const auto newIdx{newEntryStart + idx - 1};
        if (m_log->size() >= newEntryStart + idx &&
            m_log->read(newIdx)->term() != pRequest->entries(idx).term())
        {
            // TODO(lnikon): How to handle resize() with wal::wal_t?
            m_log->reset_last_n(newIdx);
            break;
        }
    }

    const auto &entries{pRequest->entries()};
    for (const auto &entry : entries)
    {
        m_log->add(entry);
    }

    if (!pRequest->entries().empty())
    {
        spdlog::info("leaderCommit={}, m_commitIndex={}", pRequest->leader_commit(), m_commitIndex);
    }

    if (pRequest->leader_commit() > m_commitIndex)
    {
        // Update m_commitIndex
        if (!updatePersistentState(
                std::min(pRequest->leader_commit(), (uint32_t)m_log->size()), std::nullopt
            ))
        {
            spdlog::error("Node={} is unable to persist commitIndex", m_config.m_id, m_commitIndex);
            pResponse->set_term(m_currentTerm);
            pResponse->set_success(false);
            pResponse->set_responder_id(m_config.m_id);
            return grpc::Status::OK;
        }
    }

    if (!entries.empty())
    {
        spdlog::info(
            "Follower Node={} PREPARING applying m_lastApplied+1={}, m_commitIndex={}",
            m_config.m_id,
            m_lastApplied + 1,
            m_commitIndex
        );
    }

    // Apply committed entries to the state machine
    applyCommittedEntries();

    pResponse->set_term(m_currentTerm);
    pResponse->set_success(true);
    pResponse->set_responder_id(m_config.m_id);
    pResponse->set_match_index(m_log->size());

    // Update @m_votedFor
    if (!updatePersistentState(std::nullopt, pRequest->sender_id()))
    {
        spdlog::error("Node={} is unable to persist votedFor", m_config.m_id, m_votedFor);
    }

    {
        m_leaderHeartbeatReceived.store(true);
    }

    spdlog::debug("Node={} is resetting election timeout at term={}", m_config.m_id, m_currentTerm);

    return grpc::Status::OK;
}

auto consensus_module_t::RequestVote(
    grpc::ServerContext                *pContext,
    const raft::v1::RequestVoteRequest *pRequest,
    raft::v1::RequestVoteResponse      *pResponse
) -> grpc::Status
{
    (void)pContext;

    m_leaderHeartbeatReceived.store(true);

    absl::WriterMutexLock locker(&m_stateMutex);

    spdlog::debug(
        "Node={} received RequestVote RPC from candidate={} during "
        "term={} peerTerm={}",
        m_config.m_id,
        pRequest->candidate_id(),
        m_currentTerm,
        pRequest->term()
    );

    pResponse->set_term(m_currentTerm);
    pResponse->set_vote_granted(0);
    pResponse->set_responder_id(m_config.m_id);

    // Become follower if candidates has higher term
    if (pRequest->term() > m_currentTerm)
    {
        becomeFollower(pRequest->term());
    }

    // Don't grant vote to the candidate if the nodes term is higher
    if (pRequest->term() < m_currentTerm)
    {
        spdlog::debug(
            "receivedTerm={} is lower than currentTerm={}", pRequest->term(), m_currentTerm
        );
        return grpc::Status::OK;
    }

    // Grant vote to the candidate if the node hasn't voted yet and
    // candidates log is at least as up-to-date as receiver's log
    if (m_votedFor == 0 || m_votedFor == pRequest->candidate_id())
    {
        spdlog::debug("votedFor={} candidateid={}", m_votedFor, pRequest->candidate_id());
        if (pRequest->last_log_term() > getLastLogTerm() ||
            (pRequest->last_log_term() == getLastLogTerm() &&
             pRequest->last_log_index() >= getLastLogIndex()))
        {
            if (!updatePersistentState(std::nullopt, pRequest->candidate_id()))
            {
                spdlog::error("Node={} is unable to persist votedFor", m_config.m_id, m_votedFor);
            }

            spdlog::info("Node={} votedFor={}", m_config.m_id, pRequest->candidate_id());

            pResponse->set_term(m_currentTerm);
            pResponse->set_vote_granted(1);
        }
    }

    return grpc::Status::OK;
}

[[nodiscard]] auto consensus_module_t::Replicate(
    grpc::ServerContext              *pContext,
    const raft::v1::ReplicateRequest *pRequest,
    raft::v1::ReplicateResponse      *pResponse
) -> grpc::Status
{
    (void)pContext;

    const auto &payloads{pRequest->payloads()};
    for (const auto &payload : payloads)
    {
        const auto status{replicate(payload)};
        if (status != raft_operation_status_k::success_k)
        {
            spdlog::error("Failed to replicate payload={}", payload);
            pResponse->set_status("FAILED");
            return grpc::Status::OK;
        }
    }

    pResponse->set_status("OK");
    return grpc::Status::OK;
}

auto consensus_module_t::replicate(std::string payload) -> raft_operation_status_k
{
    auto future = replicateAsync(std::move(payload));

    // Wait for up to 5 seconds
    if (future.wait_for(std::chrono::seconds(5)) == std::future_status::ready)
    {
        return future.get() ? raft_operation_status_k::success_k
                            : raft_operation_status_k::replicate_failed_k;
    }

    return raft_operation_status_k::timeout_k;
}

[[nodiscard]] auto consensus_module_t::replicateAsync(std::string payload) -> std::future<bool>
{
    auto replicationId = m_replicationIdCounter.fetch_add(1);

    pending_replication_t pending;
    pending.deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    auto future = pending.promise.get_future();

    raft::v1::LogEntry logEntry;

    {
        absl::MutexLock locker{&m_stateMutex};

        if (m_state != raft::v1::NodeState::NODE_STATE_LEADER)
        {
            pending.promise.set_value(false);
            return future;
        }

        logEntry.set_term(m_currentTerm);
        logEntry.set_index(getLastLogIndex() + 1);
        logEntry.set_payload(std::move(payload));

        pending.logIndex = logEntry.index();

        if (!m_log->add(logEntry))
        {
            spdlog::error("replicateAsync: Unable to update the WAL");
            pending.promise.set_value(false);
            return future;
        }

        if (m_replicas.empty())
        {
            // Single node - immediately commit and apply
            if (!updatePersistentState(logEntry.index(), std::nullopt))
            {
                spdlog::error("replicateAsync: Unable to update commit index for single node");
                pending.promise.set_value(false);
                return future;
            }

            applyCommittedEntries();
            pending.promise.set_value(true);
            return future;
        }
    }

    // Store pending replication
    {
        absl::WriterMutexLock lock{&m_pendingMutex};
        m_pendingReplications[replicationId] = std::move(pending);
    }

    // Send to replicas using thread pool
    for (auto &[id, client] : m_replicas)
    {
        if (!client.has_value())
        {
            continue;
        }

        m_rpcThreadPool->enqueue([this, &client, logEntry]
                                 { sendAppendEntriesAsync(client.value(), {logEntry}); });
    }

    return future;
}

auto consensus_module_t::currentTerm() const -> uint32_t
{
    absl::ReaderMutexLock locker{&m_stateMutex};
    return m_currentTerm;
}

auto consensus_module_t::votedFor() const -> uint32_t
{
    absl::ReaderMutexLock locker{&m_stateMutex};
    return m_votedFor;
}

auto consensus_module_t::log() const -> std::vector<raft::v1::LogEntry>
{
    absl::ReaderMutexLock locker{&m_stateMutex};
    return m_log->records();
}

auto consensus_module_t::getState() const -> raft::v1::NodeState
{
    return m_state;
}

[[nodiscard]] auto consensus_module_t::getStateSafe() const -> raft::v1::NodeState
{
    absl::ReaderMutexLock locker{&m_stateMutex};
    return m_state;
}

[[nodiscard]] auto consensus_module_t::isLeader() const -> bool
{
    return getStateSafe() == raft::v1::NodeState::NODE_STATE_LEADER;
}

[[nodiscard]] auto consensus_module_t::getLeaderHint() const -> std::string
{
    absl::WriterMutexLock locker{&m_leaderMutex};
    return m_currentLeaderHint;
}

void consensus_module_t::becomeFollower(uint32_t newTerm)
{
    m_currentTerm = newTerm;
    m_state = raft::v1::NodeState::NODE_STATE_LEADER;
    spdlog::info("Node={} reverted to follower state in term={}", m_config.m_id, m_currentTerm);

    cleanupHeartbeatThread();

    if (!updatePersistentState(std::nullopt, 0))
    {
        spdlog::error("Node={} is unable to persist votedFor={}", m_config.m_id, m_votedFor);
    }

    if (m_onLeaderChangeCbk)
    {
        auto callback = m_onLeaderChangeCbk;
        m_internalThreadPool->enqueue([callback = std::move(callback)] { callback(false); });
    }
}

void consensus_module_t::becomeLeader()
{
    if (m_state == raft::v1::NodeState::NODE_STATE_LEADER)
    {
        spdlog::warn("Node={} is already a leader", m_config.m_id);
        return;
    }

    m_state = raft::v1::NodeState::NODE_STATE_LEADER;
    m_voteCount = 0;

    {
        absl::WriterMutexLock locker{&m_leaderMutex};
        m_currentLeaderHint = m_config.m_ip;
    }

    // TODO(lnikon): Should I reinit m_nextIndex and m_matchIndex here again?

    cleanupHeartbeatThread();
    m_heartbeatThread = std::jthread([this](std::stop_token token) { runHeartbeatThread(token); });

    spdlog::info("Node={} become a leader at term={}", m_config.m_id, m_currentTerm);

    if (m_onLeaderChangeCbk)
    {
        auto callback = m_onLeaderChangeCbk;
        m_internalThreadPool->enqueue([callback = std::move(callback)] { callback(true); });
    }
}

void consensus_module_t::runHeartbeatThread(std::stop_token token)
{
    constexpr const auto heartbeatInterval{std::chrono::milliseconds(100)};

    while (!token.stop_requested() && !m_shutdown)
    {
        // Check if still leader
        {
            absl::ReaderMutexLock locker{&m_stateMutex};
            if (m_state != raft::v1::NodeState::NODE_STATE_LEADER)
            {
                break;
            }
        }

        // Send heartbeats to all followers using thread pool
        std::vector<std::future<void>> heartbeatFutures;
        heartbeatFutures.reserve(m_replicas.size());

        // TODO(lnikon): Use async gRPC instead
        for (auto &[id, client] : m_replicas)
        {
            auto future = m_rpcThreadPool->enqueue(
                [this, &client]
                {
                    spdlog::debug("Sending heartbeat to peer={}", client->id());
                    sendAppendEntriesAsync(client.value(), {});
                }
            );
            heartbeatFutures.emplace_back(std::move(future));
        }

        for (auto &future : heartbeatFutures)
        {
            future.wait_for(std::chrono::milliseconds(50));
        }

        std::this_thread::sleep_for(heartbeatInterval);
    }
}

auto consensus_module_t::waitForHeartbeat(std::stop_token token) -> bool
{
    // Determine the timeout duration
    const int64_t timeoutMs = generate_raft_timeout();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);

    // Wake up when
    // 1) Thread should be stopped
    // 2) Leader sent a heartbeat
    // 3) Wait for the heartbeat was too long
    auto heartbeatReceivedCondition = [this, deadline, token]()
    {
        return token.stop_requested() || m_leaderHeartbeatReceived.load() ||
               std::chrono::steady_clock::now() >= deadline;
    };

    spdlog::debug(
        "Timer thread at node={} will block for {}ms for the leader "
        "to send a heartbeat",
        m_config.m_id,
        timeoutMs
    );

    // Wait for the condition to be met or timeout
    absl::ReaderMutexLock locker{&m_stateMutex};
    bool                  signaled = m_stateMutex.AwaitWithTimeout(
        absl::Condition(&heartbeatReceivedCondition), absl::Milliseconds(timeoutMs)
    );

    if (token.stop_requested())
    {
        spdlog::info("Node={} election thread terminating due to stop request", m_config.m_id);
        return true;
    }

    // If timer CV gets signaled, then node has received the heartbeat from the
    // leader. Otherwise, heartbeat timed out and node needs to start the new
    // leader election
    if (signaled && m_leaderHeartbeatReceived.load())
    {
        spdlog::debug("Node={} received heartbeat from", m_config.m_id);
        m_leaderHeartbeatReceived.store(false);
        return true;
    }

    return false;
}

void consensus_module_t::sendAppendEntriesAsync(
    raft_node_grpc_client_t &client, std::vector<raft::v1::LogEntry> logEntries
)
{
    constexpr std::size_t MAX_RETRIES = 3;
    std::size_t           retryCount = 0;

    while (retryCount < MAX_RETRIES && !m_shutdown.load())
    {
        raft::v1::AppendEntriesRequest request;
        uint32_t                       nextIndex = 1; // Default for a fresh follower

        {
            absl::MutexLock locker{&m_stateMutex};

            if (m_state != raft::v1::NodeState::NODE_STATE_LEADER)
            {
                return;
            }

            request.set_term(m_currentTerm);

            // Determine nextIndex for this follower
            auto it = m_nextIndex.find(client.id());
            if (it != m_nextIndex.end())
            {
                nextIndex = it->second;
            }
            else
            {
                // Initialize nextIndex for new follower
                nextIndex = getLastLogIndex() + 1;
                m_nextIndex[client.id()] = nextIndex;
            }

            // Set prevLogIndex and prevLogTerm based on nextIndex
            if (nextIndex > 1)
            {
                auto prevEntry = m_log->read(nextIndex - 2); // log is 0-based
                request.set_prev_log_index(nextIndex - 1);
                request.set_prev_log_term(prevEntry.has_value() ? prevEntry->term() : 0);
            }
            else
            {
                request.set_prev_log_index(0);
                request.set_prev_log_term(0);
            }

            request.set_leader_commit(m_commitIndex);
            request.set_sender_id(m_config.m_id);

            // Add log entries starting from nextIndex
            for (const auto &logEntrie : logEntries)
            {
                if (logEntrie.index() >= nextIndex)
                {
                    request.add_entries()->CopyFrom(logEntrie);
                }
            }
        }

        // Send request
        raft::v1::AppendEntriesResponse response;
        bool                            rpcSuccess = false;

        try
        {
            rpcSuccess = client.appendEntries(request, &response);
        }
        catch (const std::exception &e)
        {
            spdlog::error("AppendEntries RPC exception to peer={}: {}", client.id(), e.what());
        }

        // Backoff
        if (!rpcSuccess)
        {
            retryCount++;
            if (retryCount < MAX_RETRIES)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100 * retryCount));
            }
            continue;
        }

        // Process response
        {
            absl::MutexLock locker{&m_stateMutex};

            if (response.term() > m_currentTerm)
            {
                becomeFollower(response.term());
                return;
            }
        }

        if (onSendAppendEntriesRPC(client, response))
        {
            break;
        }

        retryCount++;
    }
}

auto consensus_module_t::onSendAppendEntriesRPC(
    raft_node_grpc_client_t &client, const raft::v1::AppendEntriesResponse &response

) noexcept -> bool
{
    if (response.success())
    {
        absl::WriterMutexLock locker{&m_stateMutex};
        m_matchIndex[client.id()] = response.match_index();
        m_nextIndex[client.id()] = response.match_index() + 1;

        uint32_t majorityIndex = findMajorityIndexMatch();

        if (m_log->size() >= majorityIndex && majorityIndex > m_commitIndex)
        {
            spdlog::info("Node={} found majorityIndex={}", m_config.m_id, majorityIndex);
            if (m_log->read(majorityIndex - 1)->term() == m_currentTerm)
            {
                spdlog::info(
                    "Node={} is updating commitIndex to majorityIndex={}",
                    m_config.m_id,
                    majorityIndex
                );

                if (!updatePersistentState(majorityIndex, std::nullopt))
                {
                    spdlog::error(
                        "Node={} unable to persist commitIndex={}", m_config.m_id, m_commitIndex
                    );
                    // TODO(lnikon): Returning false here is dangerous, because
                    // is it non-distinguishable from nextIndex related issues
                    return false;
                }

                applyCommittedEntries();
            }
        }
    }
    else
    {
        absl::MutexLock locker{&m_stateMutex};

        spdlog::warn(
            "Node={} received AppendEntriesResponse from peer={} "
            "at term={} success={} match_index={}",
            m_config.m_id,
            client.id(),
            response.term(),
            response.success(),
            response.match_index()
        );

        m_nextIndex[client.id()] = std::max(1U, m_nextIndex[client.id()] - 1);

        return false;
    }

    return true;
}

void consensus_module_t::runElectionThread(std::stop_token token) noexcept
{
    while (!token.stop_requested() && !m_shutdown)
    {
        {
            absl::ReaderMutexLock locker{&m_stateMutex};
            if (getState() == raft::v1::NodeState::NODE_STATE_LEADER)
            {
                continue;
            }
        }

        if (waitForHeartbeat(token))
        {
            continue;
        }

        startElection();
    }

    spdlog::info("Election thread received stop_request");
}

void consensus_module_t::startElection()
{
    std::uint32_t                newTerm{0};
    raft::v1::RequestVoteRequest request;

    {
        absl::WriterMutexLock locker(&m_stateMutex);

        newTerm = ++m_currentTerm;
        m_state = raft::v1::NodeState::NODE_STATE_CANDIDATE;

        spdlog::info("Node={} starts election. New term={}", m_config.m_id, m_currentTerm);

        // Node in a canditate state should vote for itself.
        m_voteCount++;
        if (!updatePersistentState(std::nullopt, m_config.m_id))
        {
            spdlog::error("Node={} is unable to persist votedFor={}", m_config.m_id, m_votedFor);
        }

        // Early majority check for a single node cluster
        if (hasMajority(m_voteCount.load()))
        {
            spdlog::info(
                "Node={} achieved majority (single node cluster), becoming leader", m_config.m_id
            );
            becomeLeader();
            return;
        }

        request.set_term(m_currentTerm);
        request.set_candidate_id(m_config.m_id);
        request.set_last_log_term(getLastLogTerm());
        request.set_last_log_index(getLastLogIndex());
    }

    sendRequestVoteRPCs(request, newTerm);
}

void consensus_module_t::sendRequestVoteRPCs(
    const raft::v1::RequestVoteRequest &request, std::uint64_t newTerm
)
{
    std::vector<std::jthread> requesterThreads;
    requesterThreads.reserve(m_replicas.size());
    for (auto &[id, client] : m_replicas)
    {
        spdlog::debug(
            "Node={} is creating RequestVoteRPC thread for the "
            "peer={} during term={}",
            m_config.m_id,
            id,
            newTerm
        );

        requesterThreads.emplace_back([&client, request, this]()
                                      { sendRequestVoteAsync(request, client.value()); });
    }

    for (auto &thread : requesterThreads)
    {
        spdlog::debug("Node={} is joining RequestVoteRPC thread", m_config.m_id);
        if (thread.joinable())
        {
            thread.join();
        }
    }
}

void consensus_module_t::sendRequestVoteAsync(
    const raft::v1::RequestVoteRequest &request, raft_node_grpc_client_t &client
)
{
    raft::v1::RequestVoteResponse response;
    bool                          rpcSuccess = false;
    try
    {
        rpcSuccess = client.requestVote(request, &response);
    }
    catch (const std::exception &e)
    {
        spdlog::error("RequestVote RPC exception to peer={}: {}", client.id(), e.what());
    }

    if (!rpcSuccess)
    {
        spdlog::error("RequestVote RPC failed to peer={}", client.id());
        return;
    }

    auto responseTerm = response.term();
    auto voteGranted = response.vote_granted();

    spdlog::debug(
        "Received RequestVoteResponse in requester "
        "thread peerTerm={} "
        "voteGranted={} peer={}",
        responseTerm,
        voteGranted,
        response.responder_id()
    );

    absl::WriterMutexLock locker(&m_stateMutex);

    if (responseTerm > m_currentTerm)
    {
        becomeFollower(responseTerm);
        return;
    }

    if (voteGranted != 0 && responseTerm == m_currentTerm)
    {
        m_voteCount++;
        if (hasMajority(m_voteCount.load()))
        {
            becomeLeader();
        }
    }
}

auto consensus_module_t::getLastLogIndex() const -> uint32_t
{
    // TODO(lnikon): Add a method into wal::wal_t for: m_log->read(m_log->size() - 1)
    return m_log->empty() ? 0 : m_log->read(m_log->size() - 1)->index();
}

auto consensus_module_t::getLastLogTerm() const -> uint32_t
{
    return m_log->empty() ? 0 : m_log->read(m_log->size() - 1)->term();
}

auto consensus_module_t::hasMajority(uint32_t votes) const -> bool
{
    const uint32_t clusterSize = static_cast<uint32_t>(m_replicas.size()) + 1;
    return (votes >= ((clusterSize / 2) + 1));
}

auto consensus_module_t::getLogTerm(uint32_t index) const -> uint32_t
{
    if (index == 0 || index > m_log->size())
    {
        return 0;
    }

    return m_log->read(index - 1)->term();
}

auto consensus_module_t::findMajorityIndexMatch() const -> uint32_t
{
    std::vector<int> matchIndexes;
    // matchIndexes.reserve(m_replicas.size() + 1);

    uint32_t localLastIndex = getLastLogIndex();
    matchIndexes.emplace_back(localLastIndex);

    for (const auto &[peer, matchIdx] : m_matchIndex)
    {
        spdlog::debug("Node={} peer={} matchIdx={}", m_config.m_id, peer, matchIdx);
        matchIndexes.emplace_back(matchIdx);
    }

    if (matchIndexes.empty())
    {
        return 0;
    }

    std::ranges::sort(matchIndexes);
    // spdlog::info("Node={} matchIndexes={}", m_config.m_id, fmt::join(matchIndexes, ", "));
    return matchIndexes[matchIndexes.size() / 2];
}

auto consensus_module_t::waitForMajorityReplication(uint32_t logIndex) const -> bool
{
    auto hasMajorityLambda = [this, logIndex]() ABSL_SHARED_LOCKS_REQUIRED(m_stateMutex)
    {
        uint32_t count = 1;
        for (const auto &[peer, matchIdx] : m_matchIndex)
        {
            count += matchIdx >= logIndex;
        }
        return hasMajority(count);
    };

    spdlog::info(
        "Node={} is waiting for majority to agree on logIndex={}", m_config.m_id, logIndex
    );

    const auto timeout{absl::Seconds(generate_raft_timeout())};
    return m_stateMutex.AwaitWithTimeout(absl::Condition(&hasMajorityLambda), timeout);
}

auto consensus_module_t::initializePersistentState() -> bool
{
    if (!restorePersistentState())
    {
        spdlog::error("Unable to restore persistent state");
        return false;
    }

    // for (const auto &logEntry : m_log)
    for (std::size_t idx{0}; idx < m_log->size(); ++idx)
    {
        const auto &logEntry{m_log->read(idx)};
        // TODO(lnikon): Should I update the state machine here? Maybe a callback e.g.
        // onCommit(logEntry)?
    }

    return true;
}

auto consensus_module_t::updatePersistentState(
    std::optional<std::uint32_t> commitIndex, std::optional<std::uint32_t> votedFor
) -> bool
{
    m_commitIndex = commitIndex.has_value() ? commitIndex.value() : m_commitIndex;

    m_votedFor = votedFor.has_value() ? votedFor.value() : m_votedFor;

    return flushPersistentState();
}

auto consensus_module_t::flushPersistentState() -> bool
{

    auto          path = std::filesystem::path(constructFilename(gRaftFilename, m_config.m_id));
    std::ofstream fsa(path, std::fstream::out | std::fstream::trunc);
    if (!fsa.is_open())
    {
        spdlog::error(
            "Node={} is unable to open {} to flush "
            "commitIndex={} and votedFor={}",
            m_config.m_id,
            path.c_str(),
            m_commitIndex,
            m_votedFor
        );
        return false;
    }

    // Save: currentTerm commitIndex votedFor
    fsa << m_currentTerm << ' ' << m_commitIndex << ' ' << m_votedFor << '\n';
    if (fsa.fail())
    {
        spdlog::error(
            "Node={} is unable to write into {} the "
            "currentTerm={}, commitIndex={} and votedFor={}",
            m_config.m_id,
            path.c_str(),
            m_currentTerm,
            m_commitIndex,
            m_votedFor
        );
        return false;
    }

    return !fsa.flush().fail();
}

auto consensus_module_t::restorePersistentState() -> bool
{
    {
        auto path = std::filesystem::path(constructFilename(gRaftFilename, m_config.m_id));
        if (!std::filesystem::exists(path))
        {
            spdlog::info("Node={} is running the first time", m_config.m_id);
            return true;
        }

        std::ifstream ifs(path, std::ifstream::in);
        if (!ifs.is_open())
        {
            spdlog::error(
                "Node={} is unable to open {} to restore commitIndex "
                "and votedFor",
                m_config.m_id,
                path.c_str()
            );
            return false;
        }

        ifs >> m_currentTerm >> m_commitIndex >> m_votedFor;
        spdlog::info(
            "Node={} restored currentTerm={}, commitIndex={} and votedFor={}",
            m_config.m_id,
            m_commitIndex,
            m_commitIndex,
            m_votedFor
        );
    }

    return true;
}

void consensus_module_t::monitorPendingReplications()
{
    while (!m_shutdown.load())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        std::vector<uint64_t> completedIds;
        std::vector<uint64_t> timeoutIds;

        {
            absl::ReaderMutexLock stateLock{&m_stateMutex};
            absl::WriterMutexLock pendingLock{&m_pendingMutex};

            const auto now = std::chrono::steady_clock::now();

            for (const auto &[id, pending] : m_pendingReplications)
            {
                uint32_t logIndex = pending.logIndex;

                // Check if this entry has been commited
                if (m_commitIndex >= logIndex)
                {
                    completedIds.emplace_back(id);
                }
                // Check if timed out
                else if (now > pending.deadline)
                {
                    timeoutIds.emplace_back(id);
                }
            }
        }

        if (!completedIds.empty() || !timeoutIds.empty())
        {
            absl::WriterMutexLock pendingLock{&m_pendingMutex};

            // Handle completed
            for (uint64_t id : completedIds)
            {
                auto it = m_pendingReplications.find(id);
                if (it != m_pendingReplications.end())
                {
                    it->second.promise.set_value(true);
                    m_pendingReplications.erase(it);
                }
            }

            // Handle timeouts
            for (uint64_t id : timeoutIds)
            {
                auto it = m_pendingReplications.find(id);
                if (it != m_pendingReplications.end())
                {
                    it->second.promise.set_value(false);
                    m_pendingReplications.erase(it);
                }
            }
        }
    }
}

void consensus_module_t::applyCommittedEntries()
{
    while (m_lastApplied < m_commitIndex && !m_shutdown.load())
    {
        uint32_t applyIndex = m_lastApplied + 1;
        auto     logEntry = m_log->read(applyIndex - 1);

        if (logEntry.has_value())
        {
            m_internalThreadPool->enqueue(
                [this, entry = std::move(logEntry.value()), applyIndex]
                {
                    spdlog::info("Applying log entry {} to state machine", applyIndex);

                    try
                    {
                        if (m_onCommitCbk(entry))
                        {
                            m_lastApplied = applyIndex;
                        }
                        else
                        {
                            spdlog::error("Failed to apply log entry {}", applyIndex);
                        }
                    }
                    catch (const std::exception &e)
                    {
                        spdlog::error("Exception applying log entry {}: {}", applyIndex, e.what());
                    }
                }
            );

            m_lastApplied++;
        }
        else
        {
            spdlog::error("Failed to read log entry {} for state machine update", applyIndex);
            m_lastApplied++;
        }
    }
}

void consensus_module_t::cleanupHeartbeatThread()
{
    if (m_heartbeatThread.joinable())
    {
        spdlog::info("Node={} clearing existing heartbeat threads", m_config.m_id);
        m_heartbeatThread.request_stop();
        m_heartbeatThread.join();
        spdlog::info("Node={} existing heartbeat threads cleared", m_config.m_id);
    }
}

void consensus_module_t::cleanupElectionThread()
{
    if (m_electionThread.joinable())
    {
        spdlog::info("Joining election thread...");
        m_electionThread.request_stop();
        m_electionThread.join();
        spdlog::info("Election thread joined!");
    }
}

void consensus_module_t::setOnCommitCallback(on_commit_cbk_t onCommitCbk)
{
    m_onCommitCbk = std::move(onCommitCbk);
}

void consensus_module_t::setOnLeaderChangeCallback(on_leader_change_cbk_t onLeaderChangeCbk)
{
    m_onLeaderChangeCbk = std::move(onLeaderChangeCbk);
}

} // namespace consensus

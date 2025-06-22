#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fmt/core.h>
#include <fstream>
#include <random>
#include <ranges>
#include <thread>
#include <utility>

#include <absl/synchronization/mutex.h>
#include <absl/time/time.h>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "raft.h"
#include "Raft.pb.h"
#include "wal/wal.h"

namespace
{

const std::string_view gRaftFilename = "RAFT_PERSISTENCE";
const std::string_view gLogFilename = "RAFT_LOG";

auto constructFilename(std::string_view filename, std::uint32_t peerId) -> std::string
{
    return fmt::format("var/tkvpp/{}_NODE_{}", filename, peerId);
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

namespace raft
{

// -------------------------------
// raft_node_grpc_client_t
// -------------------------------
raft_node_grpc_client_t::raft_node_grpc_client_t(
    node_config_t config, std::unique_ptr<RaftService::StubInterface> pRaftStub
)
    : m_config{std::move(config)},
      m_stub{std::move(pRaftStub)}
{
    assert(m_config.m_id > 0);
    assert(!m_config.m_ip.empty());
}

auto raft_node_grpc_client_t::appendEntries(
    const AppendEntriesRequest &request, AppendEntriesResponse *response
) -> bool
{
    const auto          RPC_TIMEOUT = std::chrono::seconds(generate_raft_timeout());
    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + RPC_TIMEOUT);
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
    const RequestVoteRequest &request, RequestVoteResponse *response
) -> bool
{
    const auto RPC_TIMEOUT = std::chrono::seconds(generate_raft_timeout());

    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + RPC_TIMEOUT);

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
    const ReplicateEntriesRequest &request, ReplicateEntriesResponse *response
) -> bool
{
    const auto RPC_TIMEOUT = std::chrono::seconds(generate_raft_timeout());

    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + RPC_TIMEOUT);

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
    node_config_t nodeConfig, std::vector<raft_node_grpc_client_t> replicas, wal_ptr_t pLog
) noexcept
    : m_config{std::move(nodeConfig)},
      m_currentTerm{0},
      m_votedFor{0},
      m_log{std::move(pLog)},
      m_commitIndex{0},
      m_lastApplied{0},
      m_state{NodeState::FOLLOWER},
      m_leaderHeartbeatReceived{false},
      m_voteCount{0}
{
    assert(m_config.m_id > 0);

    // TODO(lnikon): This assertion is important, but also gets in the way of testing.
    // assert(replicas.size() > 0);

    assert(m_config.m_id <= replicas.size() + 1);

    for (auto &&nodeClient : replicas)
    {
        m_matchIndex[nodeClient.id()] = 0;
        m_nextIndex[nodeClient.id()] = 1;
        m_replicas[nodeClient.id()] = std::move(nodeClient);
    }
}

auto consensus_module_t::init() -> bool
{
    absl::WriterMutexLock locker{&m_stateMutex};
    if (!initializePersistentState())
    {
        spdlog::warn("Unable to initialize persistent state!");
        return false;
    }

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

    spdlog::info("Consensus module stopped gracefully!");
}

auto consensus_module_t::AppendEntries(
    grpc::ServerContext        *pContext,
    const AppendEntriesRequest *pRequest,
    AppendEntriesResponse      *pResponse
) -> grpc::Status
{
    (void)pContext;

    spdlog::debug(
        "Node={} recevied AppendEntries RPC from leader={} during term={}",
        m_config.m_id,
        pRequest->senderid(),
        pRequest->term()
    );

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
        pResponse->set_responderid(m_config.m_id);
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
    if (pRequest->prevlogindex() > 0)
    {
        const auto &logEntry{m_log->read(pRequest->prevlogindex() - 1)};
        if (!logEntry.has_value())
        {
            spdlog::error(
                "Node={} received prevlogindex={} which does not exist. logSize={}",
                m_config.m_id,
                pRequest->prevlogindex(),
                m_log->size()
            );

            pResponse->set_term(m_currentTerm);
            pResponse->set_success(false);
            pResponse->set_responderid(m_config.m_id);
            return grpc::Status::OK;
        }

        if (logEntry->term() != pRequest->prevlogterm())
        {
            spdlog::error(
                "Node={} received prevlogindex={} with mismatched prevlogterm={}. Current log "
                "term={}",
                m_config.m_id,
                pRequest->prevlogindex(),
                pRequest->prevlogterm(),
                logEntry->term()
            );

            pResponse->set_term(m_currentTerm);
            pResponse->set_success(false);
            pResponse->set_responderid(m_config.m_id);
            return grpc::Status::OK;
        }
    }

    // 3. Append new entries and remove conflicting ones
    auto newEntryStart = pRequest->prevlogindex() + 1;
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
        spdlog::info("leaderCommit={}, m_commitIndex={}", pRequest->leadercommit(), m_commitIndex);
    }

    if (pRequest->leadercommit() > m_commitIndex)
    {
        // Update m_commitIndex
        if (!updatePersistentState(
                std::min(pRequest->leadercommit(), (uint32_t)m_log->size()), std::nullopt
            ))
        {
            spdlog::error("Node={} is unable to persist commitIndex", m_config.m_id, m_commitIndex);
            pResponse->set_term(m_currentTerm);
            pResponse->set_success(false);
            pResponse->set_responderid(m_config.m_id);
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
    while (m_lastApplied < m_commitIndex)
    {
        ++m_lastApplied;
        const auto logEntry = m_log->read(m_lastApplied - 1);
        if (logEntry.has_value())
        {
            spdlog::info(
                "Follower Node={} applying logEntry->index()={} to state machine",
                m_config.m_id,
                logEntry->index()
            );
            m_onCommitCbk(logEntry.value());
        }
        else
        {
            spdlog::error(
                "Node={} failed to read log entry={} for state machine update",
                m_config.m_id,
                m_lastApplied
            );
        }
    }

    pResponse->set_term(m_currentTerm);
    pResponse->set_success(true);
    pResponse->set_responderid(m_config.m_id);
    pResponse->set_match_index(m_log->size());

    // Update @m_votedFor
    if (!updatePersistentState(std::nullopt, pRequest->senderid()))
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
    grpc::ServerContext      *pContext,
    const RequestVoteRequest *pRequest,
    RequestVoteResponse      *pResponse
) -> grpc::Status
{
    (void)pContext;

    {
        m_leaderHeartbeatReceived.store(true);
    }

    absl::WriterMutexLock locker(&m_stateMutex);

    spdlog::debug(
        "Node={} received RequestVote RPC from candidate={} during "
        "term={} peerTerm={}",
        m_config.m_id,
        pRequest->candidateid(),
        m_currentTerm,
        pRequest->term()
    );

    pResponse->set_term(m_currentTerm);
    pResponse->set_votegranted(0);
    pResponse->set_responderid(m_config.m_id);

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
    if (m_votedFor == 0 || m_votedFor == pRequest->candidateid())
    {
        spdlog::debug("votedFor={} candidateid={}", m_votedFor, pRequest->candidateid());
        if (pRequest->lastlogterm() > getLastLogTerm() ||
            (pRequest->lastlogterm() == getLastLogTerm() &&
             pRequest->lastlogindex() >= getLastLogIndex()))
        {
            if (!updatePersistentState(std::nullopt, pRequest->candidateid()))
            {
                spdlog::error("Node={} is unable to persist votedFor", m_config.m_id, m_votedFor);
            }

            spdlog::debug("Node={} votedFor={}", m_config.m_id, pRequest->candidateid());

            pResponse->set_term(m_currentTerm);
            pResponse->set_votegranted(1);
        }
    }

    return grpc::Status::OK;
}

auto consensus_module_t::replicate(std::string payload) -> raft_operation_status_k
{
    LogEntry logEntry;

    {
        absl::MutexLock locker{&m_stateMutex};

        if (m_state != NodeState::LEADER)
        {
            spdlog::error("Non-leader node={} received a put request.");
            return raft_operation_status_k::not_leader_k;
        }

        logEntry.set_term(m_currentTerm);
        logEntry.set_index(getLastLogIndex() + 1);
        logEntry.set_payload(std::move(payload));

        if (!m_log->add(logEntry))
        {
            spdlog::error("consensus_module_t::replicate: Unable to update the WAL");
            return raft_operation_status_k::wal_add_failed_k;
        }
    }

    for (auto &[id, client] : m_replicas)
    {
        sendAppendEntriesRPC(client.value(), {logEntry});
    }

    {
        absl::WriterMutexLock locker{&m_stateMutex};
        bool                  success = waitForMajorityReplication(logEntry.index());
        if (success)
        {
            spdlog::info("Node={} majority agreed on logEntry={}", m_config.m_id, logEntry.index());
        }
        else
        {
            spdlog::info(
                "Node={} majority failed to agree on logEntry={}", m_config.m_id, logEntry.index()
            );
            return raft_operation_status_k::replicate_failed_k;
        }
    }

    return raft_operation_status_k::success_k;
}

[[nodiscard]] auto consensus_module_t::forward(std::string payload) -> raft_operation_status_k
{
    LogEntry logEntry;

    {
        absl::MutexLock locker{&m_stateMutex};

        switch (m_state)
        {
        case NodeState::LEADER:
        {
            return replicate(std::move(payload));
        }
        case NodeState::FOLLOWER:
        {
            if (m_votedFor == gInvalidId)
            {
                spdlog::error(
                    "Follower node={} received a put request, but has no leader to forward it to.",
                    m_config.m_id
                );
                return raft_operation_status_k::leader_not_found_k;
            }

            ReplicateEntriesRequest request;
            request.set_term(m_currentTerm);
            request.set_senderid(m_config.m_id);
            request.add_payloads(std::move(payload));

            ReplicateEntriesResponse response;
            if (!m_replicas[m_votedFor]->replicate(request, &response))
            {
                spdlog::error(
                    "Non-leader node={} was unable to forward put RPC to leader={}",
                    m_config.m_id,
                    m_votedFor
                );
            }

            return raft_operation_status_k::success_k;
        }
        case NodeState::CANDIDATE:
        {
            spdlog::error("Candidate node={} received a put request.", m_config.m_id);
            return raft_operation_status_k::not_leader_k;
        }
        default:
        {
            spdlog::error(
                "Node={} is in an unknown state={} and received a put request.",
                m_config.m_id,
                static_cast<int>(m_state)
            );
            return raft_operation_status_k::invalid_request_k;
        }
        }
    }

    return raft_operation_status_k::success_k;
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

auto consensus_module_t::log() const -> std::vector<LogEntry>
{
    absl::ReaderMutexLock locker{&m_stateMutex};
    return m_log->records();
}

auto consensus_module_t::getState() const -> NodeState
{
    return m_state;
}

[[nodiscard]] auto consensus_module_t::getStateSafe() const -> NodeState
{
    absl::ReaderMutexLock locker{&m_stateMutex};
    return m_state;
}

void consensus_module_t::becomeFollower(uint32_t newTerm)
{
    m_currentTerm = newTerm;
    m_state = NodeState::FOLLOWER;
    spdlog::info("Node={} reverted to follower state in term={}", m_config.m_id, m_currentTerm);

    cleanupHeartbeatThread();

    if (!updatePersistentState(std::nullopt, 0))
    {
        spdlog::error("Node={} is unable to persist votedFor={}", m_config.m_id, m_votedFor);
    }
}

void consensus_module_t::becomeLeader()
{
    if (m_state == NodeState::LEADER)
    {
        spdlog::warn("Node={} is already a leader", m_config.m_id);
        return;
    }

    m_state = NodeState::LEADER;
    m_voteCount = 0;

    cleanupHeartbeatThread();
    m_heartbeatThread = std::jthread([this](std::stop_token token) { runHeartbeatThread(token); });

    spdlog::info("Node={} become a leader at term={}", m_config.m_id, m_currentTerm);
}

void consensus_module_t::runHeartbeatThread(std::stop_token token)
{
    constexpr const auto heartbeatInterval{std::chrono::milliseconds(100)};

    while (!token.stop_requested() && !m_shutdown)
    {
        // TODO(lnikon): Make these calls async
        for (auto &[id, client] : m_replicas)
        {
            spdlog::debug(
                "Node={} is creating a heartbeat thread for the peer={}", m_config.m_id, id
            );
            sendAppendEntriesRPC(client.value(), {});
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

void consensus_module_t::sendAppendEntriesRPC(
    raft_node_grpc_client_t &client, std::vector<LogEntry> logEntries, bool heartbeat /* = false */
)
{
    // TODO(lnikon): Implement upper bound for the number of retries
    // TODO(lnikon): Implement exponential backoff for retries
    while (true)
    {
        AppendEntriesRequest request;
        uint32_t             nextIndex = 1; // Default for a fresh follower

        {
            absl::MutexLock locker{&m_stateMutex};

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
                request.set_prevlogindex(nextIndex - 1);
                request.set_prevlogterm(prevEntry.has_value() ? prevEntry->term() : 0);
            }
            else
            {
                request.set_prevlogindex(0);
                request.set_prevlogterm(0);
            }

            request.set_leadercommit(m_commitIndex);
            request.set_senderid(m_config.m_id);

            // Add log entries starting from nextIndex
            for (size_t i = 0; i < logEntries.size(); ++i)
            {
                if (logEntries[i].index() >= nextIndex)
                {
                    request.add_entries()->CopyFrom(logEntries[i]);
                }
            }
        }

        if (!logEntries.empty())
        {
            spdlog::info(
                "Node={} is sending AppendEntries RPC to peer={} "
                "at term={} with {} entries",
                m_config.m_id,
                client.id(),
                m_currentTerm,
                logEntries.size()
            );
        }

        AppendEntriesResponse response;
        auto                  status = client.appendEntries(request, &response);
        if (!status)
        {
            absl::MutexLock locker{&m_stateMutex};
            spdlog::error(
                "Node={} failed to send AppendEntries RPC to peer={} "
                "at term={}",
                m_config.m_id,
                client.id(),
                m_currentTerm
            );
            return;
        }

        {
            absl::MutexLock locker{&m_stateMutex};

            if (!logEntries.empty())
            {
                spdlog::info(
                    "Node={} received AppendEntriesResponse from peer={} "
                    "at term={} success={} match_index={}",
                    m_config.m_id,
                    client.id(),
                    response.term(),
                    response.success(),
                    response.match_index()
                );
            }

            if (response.term() > m_currentTerm)
            {
                becomeFollower(response.term());
                return;
            }
        }

        // The loop will continue its execution until onSendAppendEntriesRPC
        // returns true
        if (onSendAppendEntriesRPC(client, response, logEntries.empty()))
        {
            if (!logEntries.empty())
            {
                spdlog::info(
                    "Node={} successfully sent AppendEntries RPC to peer={}",
                    m_config.m_id,
                    client.id()
                );
            }
            break;
        }
    }
}

auto consensus_module_t::onSendAppendEntriesRPC(
    raft_node_grpc_client_t     &client,
    const AppendEntriesResponse &response,
    bool                         heartbeat /* = false */
) noexcept -> bool
{
    if (response.success())
    {
        absl::WriterMutexLock locker{&m_stateMutex};
        m_matchIndex[client.id()] = response.match_index();
        m_nextIndex[client.id()] = response.match_index() + 1;

        uint32_t majorityIndex = findMajorityIndexMatch();

        // spdlog::info(
        //     "MajorityIndex={}, m_commitIndex={}, m_log->size()={}",
        //     majorityIndex,
        //     m_commitIndex,
        //     m_log->size()
        // );

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

                while (m_lastApplied < m_commitIndex)
                {
                    ++m_lastApplied;
                    spdlog::info("TODO(lnikon): Apply to state machine here");
                }
            }
        }
        else
        {
            if (!heartbeat)
            {
                spdlog::info(
                    "Did not find majorityIndex={} for "
                    "heartbeat from peer={} at term={}",
                    majorityIndex,
                    client.id(),
                    m_currentTerm
                );
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
            if (getState() == NodeState::LEADER)
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
    std::uint32_t      newTerm{0};
    RequestVoteRequest request;
    {
        absl::WriterMutexLock locker(&m_stateMutex);
        newTerm = ++m_currentTerm;

        m_state = NodeState::CANDIDATE;

        spdlog::debug("Node={} starts election. New term={}", m_config.m_id, m_currentTerm);

        // Node in a canditate state should vote for itself.
        m_voteCount++;
        if (!updatePersistentState(std::nullopt, m_config.m_id))
        {
            spdlog::error("Node={} is unable to persist votedFor={}", m_config.m_id, m_votedFor);
        }

        request.set_term(m_currentTerm);
        request.set_candidateid(m_config.m_id);
        request.set_lastlogterm(getLastLogTerm());
        request.set_lastlogindex(getLastLogIndex());
    }

    sendRequestVoteRPCs(request, newTerm);
}

void consensus_module_t::sendRequestVoteRPCs(
    const RequestVoteRequest &request, std::uint64_t newTerm
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

        requesterThreads.emplace_back(
            [&client, request, this]()
            {
                RequestVoteResponse response;
                if (!client->requestVote(request, &response))
                {
                    spdlog::error("RequestVote RPC failed in requester thread");
                    return;
                }

                auto responseTerm = response.term();
                auto voteGranted = response.votegranted();

                spdlog::debug(
                    "Received RequestVoteResponse in requester "
                    "thread peerTerm={} "
                    "voteGranted={} peer={}",
                    responseTerm,
                    voteGranted,
                    response.responderid()
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
        );
    }

    for (auto &thread : requesterThreads)
    {
        spdlog::debug("Node={} is joining RequestVoteRPC thread", m_config.m_id);
        thread.join();
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

    fsa << m_commitIndex << " " << m_votedFor << "\n";
    if (fsa.fail())
    {
        spdlog::error(
            "Node={} is unable to write into {} the "
            "commitIndex={} and votedFor={}",
            m_config.m_id,
            path.c_str(),
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

        ifs >> m_commitIndex >> m_votedFor;
        m_votedFor = 0;
        spdlog::info(
            "Node={} restored commitIndex={} and votedFor={}",
            m_config.m_id,
            m_commitIndex,
            m_votedFor
        );
    }

    return true;
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
} // namespace raft

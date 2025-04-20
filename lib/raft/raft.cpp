#include "raft.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
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

namespace
{

const std::string_view gRaftFilename = "RAFT_PERSISTENCE";
const std::string_view gLogFilename = "RAFT_LOG";

auto constructFilename(std::string_view filename, std::uint32_t peerId)
    -> std::string
{
    return fmt::format("{}_NODE_{}", filename, peerId);
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
    node_config_t config, std::unique_ptr<RaftService::StubInterface> pRaftStub)
    : m_config{std::move(config)},
      m_stub{std::move(pRaftStub)}
{
    assert(m_config.m_id > 0);
    assert(!m_config.m_ip.empty());
}

auto raft_node_grpc_client_t::appendEntries(const AppendEntriesRequest &request,
                                            AppendEntriesResponse *response)
    -> bool
{
    const auto RPC_TIMEOUT = std::chrono::seconds(generate_raft_timeout());

    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + RPC_TIMEOUT);
    grpc::Status status = m_stub->AppendEntries(&context, request, response);
    if (!status.ok())
    {
        spdlog::error("AppendEntries RPC to peer id={} ip={} failed. Error "
                      "code={} message={}",
                      m_config.m_id,
                      m_config.m_ip,
                      static_cast<int>(status.error_code()),
                      status.error_message());
        return false;
    }

    return true;
}

auto raft_node_grpc_client_t::requestVote(const RequestVoteRequest &request,
                                          RequestVoteResponse *response) -> bool
{
    const auto RPC_TIMEOUT = std::chrono::seconds(generate_raft_timeout());

    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + RPC_TIMEOUT);

    grpc::Status status = m_stub->RequestVote(&context, request, response);
    if (!status.ok())
    {
        spdlog::error("RequestVote RPC to peer id={} ip={} failed. Error "
                      "code={} message={}",
                      m_config.m_id,
                      m_config.m_ip,
                      static_cast<int>(status.error_code()),
                      status.error_message());
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
    node_config_t                        nodeConfig,
    std::vector<raft_node_grpc_client_t> replicas) noexcept
    : m_config{std::move(nodeConfig)},
      m_currentTerm{0},
      m_votedFor{0},
      m_commitIndex{0},
      m_lastApplied{0},
      m_state{NodeState::FOLLOWER},
      m_leaderHeartbeatReceived{false},
      m_voteCount{0}
{
    assert(m_config.m_id > 0);
    assert(replicas.size() > 0);
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
    m_electionThread = std::jthread([this](std::stop_token token)
                                    { runElectionThread(token); });
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

auto consensus_module_t::AppendEntries(grpc::ServerContext        *pContext,
                                       const AppendEntriesRequest *pRequest,
                                       AppendEntriesResponse      *pResponse)
    -> grpc::Status
{
    (void)pContext;
    (void)pRequest;
    (void)pResponse;

    spdlog::debug(
        "Node={} recevied AppendEntries RPC from leader={} during term={}",
        m_config.m_id,
        pRequest->senderid(),
        pRequest->term());

    absl::WriterMutexLock locker(&m_stateMutex);

    // 1. Term check
    if (pRequest->term() < m_currentTerm)
    {
        spdlog::debug("Node={} receviedTerm={} is smaller than currentTerm={}",
                      m_config.m_id,
                      pRequest->term(),
                      m_currentTerm);
        pResponse->set_term(m_currentTerm);
        pResponse->set_success(false);
        pResponse->set_responderid(m_config.m_id);
        return grpc::Status::OK;
    }

    if (pRequest->term() > m_currentTerm)
    {
        spdlog::debug("Node={} receviedTerm={} is higher than currentTerm={}. "
                      "Reverting to follower",
                      m_config.m_id,
                      pRequest->term(),
                      m_currentTerm);
        becomeFollower(pRequest->term());
    }

    // 2. Log consistency check
    if (pRequest->prevlogindex() > 0)
    {
        if (m_log.size() < pRequest->prevlogindex() ||
            (m_log[pRequest->prevlogindex() - 1].term() !=
             pRequest->prevlogterm()))
        {
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
        if (m_log.size() >= newEntryStart + idx &&
            m_log[newIdx].term() != pRequest->entries(idx).term())
        {
            m_log.resize(newIdx);
            break;
        }
    }

    m_log.insert(
        m_log.end(), pRequest->entries().begin(), pRequest->entries().end());

    if (pRequest->leadercommit() > m_commitIndex)
    {
        // Update m_commitIndex
        if (!updatePersistentState(
                std::min(pRequest->leadercommit(), (uint32_t)m_log.size()),
                std::nullopt))
        {
            spdlog::error("Node={} is unable to persist commitIndex",
                          m_config.m_id,
                          m_commitIndex);
            pResponse->set_term(m_currentTerm);
            pResponse->set_success(false);
            pResponse->set_responderid(m_config.m_id);
            return grpc::Status::OK;
        }

        while (m_lastApplied < m_commitIndex)
        {
            ++m_lastApplied;
            // TODO(lnikon): Update the state machine!
        }
    }

    pResponse->set_term(m_currentTerm);
    pResponse->set_success(true);
    pResponse->set_responderid(m_config.m_id);
    pResponse->set_match_index(m_log.size());

    // Update @m_votedFor
    if (!updatePersistentState(std::nullopt, pRequest->senderid()))
    {
        spdlog::error(
            "Node={} is unable to persist votedFor", m_config.m_id, m_votedFor);
    }

    {
        m_leaderHeartbeatReceived.store(true);
    }

    spdlog::debug("Node={} is resetting election timeout at term={}",
                  m_config.m_id,
                  m_currentTerm);

    return grpc::Status::OK;
}

auto consensus_module_t::RequestVote(grpc::ServerContext      *pContext,
                                     const RequestVoteRequest *pRequest,
                                     RequestVoteResponse      *pResponse)
    -> grpc::Status
{
    (void)pContext;

    {
        m_leaderHeartbeatReceived.store(true);
    }

    absl::WriterMutexLock locker(&m_stateMutex);

    spdlog::debug("Node={} received RequestVote RPC from candidate={} during "
                  "term={} peerTerm={}",
                  m_config.m_id,
                  pRequest->candidateid(),
                  m_currentTerm,
                  pRequest->term());

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
        spdlog::debug("receivedTerm={} is lower than currentTerm={}",
                      pRequest->term(),
                      m_currentTerm);
        return grpc::Status::OK;
    }

    // Grant vote to the candidate if the node hasn't voted yet and
    // candidates log is at least as up-to-date as receiver's log
    if (m_votedFor == 0 || m_votedFor == pRequest->candidateid())
    {
        spdlog::debug(
            "votedFor={} candidateid={}", m_votedFor, pRequest->candidateid());
        if (pRequest->lastlogterm() > getLastLogTerm() ||
            (pRequest->lastlogterm() == getLastLogTerm() &&
             pRequest->lastlogindex() >= getLastLogIndex()))
        {
            if (!updatePersistentState(std::nullopt, pRequest->candidateid()))
            {
                spdlog::error("Node={} is unable to persist votedFor",
                              m_config.m_id,
                              m_votedFor);
            }

            spdlog::debug(
                "Node={} votedFor={}", m_config.m_id, pRequest->candidateid());

            pResponse->set_term(m_currentTerm);
            pResponse->set_votegranted(1);
        }
    }

    return grpc::Status::OK;
}

auto consensus_module_t::replicate(LogEntry logEntry) -> bool
{
    uint32_t currentTerm = 0;
    uint32_t lastLogIndex = 0;
    uint32_t votedFor = 0;
    {
        absl::MutexLock locker{&m_stateMutex};
        if (m_state != NodeState::LEADER)
        {
            if (m_votedFor != gInvalidId)
            {
                votedFor = m_votedFor;
            }
            else
            {
                spdlog::error("Non-leader node={} received a put request. "
                              "Leader at current term is unkown.",
                              m_config.m_id);
                return false;
            }
        }

        currentTerm = m_currentTerm;
        lastLogIndex = getLastLogIndex() + 1;
    }

    if (votedFor != gInvalidId)
    {
        spdlog::info("Non-leader node={} received a put request. Forwarding to "
                     "leader={} during currentTerm={}",
                     m_config.m_id,
                     votedFor,
                     currentTerm);

        // if (!m_kvClients[votedFor]->put(*pRequest, pResponse))
        {
            spdlog::error(
                "Non-leader node={} was unable to forward put RPC to leader={}",
                m_config.m_id,
                votedFor);
        }

        return false;
    }

    logEntry.set_term(currentTerm);
    logEntry.set_index(lastLogIndex);
    logEntry.set_command(
        fmt::format("{}:{}", logEntry.key(), logEntry.value()));

    {
        absl::MutexLock locker{&m_stateMutex};
        m_log.emplace_back(logEntry);
    }

    for (auto &[id, client] : m_replicas)
    {
        sendAppendEntriesRPC(client.value(), {logEntry});
    }

    absl::WriterMutexLock locker{&m_stateMutex};
    bool success = waitForMajorityReplication(logEntry.index());
    if (success)
    {
        spdlog::info("Node={} majority agreed on logEntry={}",
                     m_config.m_id,
                     logEntry.index());
    }
    else
    {
        spdlog::info("Node={} majority failed to agree on logEntry={}",
                     m_config.m_id,
                     logEntry.index());
        return false;
    }

    return true;
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
    return m_log;
}
auto consensus_module_t::getState() const -> NodeState
{
    return m_state;
}

void consensus_module_t::becomeFollower(uint32_t newTerm)
{
    m_currentTerm = newTerm;
    m_state = NodeState::FOLLOWER;
    spdlog::debug("Node={} reverted to follower state in term={}",
                  m_config.m_id,
                  m_currentTerm);

    cleanupHeartbeatThread();

    if (!updatePersistentState(std::nullopt, 0))
    {
        spdlog::error("Node={} is unable to persist votedFor={}",
                      m_config.m_id,
                      m_votedFor);
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
    m_heartbeatThread = std::jthread([this](std::stop_token token)
                                     { runHeartbeatThread(token); });

    spdlog::info(
        "Node={} become a leader at term={}", m_config.m_id, m_currentTerm);
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
                "Node={} is creating a heartbeat thread for the peer={}",
                m_config.m_id,
                id);
            sendAppendEntriesRPC(client.value(), {});
        }
        std::this_thread::sleep_for(heartbeatInterval);
    }
}

auto consensus_module_t::waitForHeartbeat(std::stop_token token) -> bool
{
    // Determine the timeout duration
    const int64_t timeoutMs = generate_raft_timeout();
    const auto    deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);

    // Wake up when
    // 1) Thread should be stopped
    // 2) Leader sent a heartbeat
    // 3) Wait for the heartbeat was too long
    auto heartbeatReceivedCondition = [this, deadline, token]()
    {
        return token.stop_requested() || m_leaderHeartbeatReceived.load() ||
               std::chrono::steady_clock::now() >= deadline;
    };

    spdlog::debug("Timer thread at node={} will block for {}ms for the leader "
                  "to send a heartbeat",
                  m_config.m_id,
                  timeoutMs);

    // Wait for the condition to be met or timeout
    absl::ReaderMutexLock locker{&m_stateMutex};
    bool                  signaled = m_stateMutex.AwaitWithTimeout(
        absl::Condition(&heartbeatReceivedCondition),
        absl::Milliseconds(timeoutMs));

    if (token.stop_requested())
    {
        spdlog::info("Node={} election thread terminating due to stop request",
                     m_config.m_id);
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

void consensus_module_t::sendAppendEntriesRPC(raft_node_grpc_client_t &client,
                                              std::vector<LogEntry> logEntries)
{
    while (true)
    {
        AppendEntriesRequest request;
        {
            absl::MutexLock locker{&m_stateMutex};

            request.set_term(m_currentTerm);

            if (!logEntries.empty())
            {
                m_nextIndex[client.id()] = getLastLogIndex() + 1;
                request.set_prevlogindex(getLastLogIndex());
                request.set_prevlogterm(getLastLogTerm());
            }

            request.set_leadercommit(m_commitIndex);
            request.set_senderid(m_config.m_id);

            for (const auto &logEntry : logEntries)
            {
                request.add_entries()->CopyFrom(logEntry);
            }
        }

        AppendEntriesResponse response;
        auto                  status = client.appendEntries(request, &response);
        if (!status)
        {
            absl::MutexLock locker{&m_stateMutex};
            spdlog::error("Node={} failed to send AppendEntries RPC to peer={} "
                          "at term={}",
                          m_config.m_id,
                          client.id(),
                          m_currentTerm);
            return;
        }

        {
            absl::MutexLock locker{&m_stateMutex};
            if (response.term() > m_currentTerm)
            {
                becomeFollower(response.term());
                return;
            }
        }

        // The loop will continue its execution until onSendAppendEntriesRPC
        // returns true
        if (onSendAppendEntriesRPC(client, response))
        {
            return;
        }
    }
}

auto consensus_module_t::onSendAppendEntriesRPC(
    raft_node_grpc_client_t     &client,
    const AppendEntriesResponse &response) noexcept -> bool
{
    if (response.success())
    {
        spdlog::debug("[VAGAG] response.success");
        absl::WriterMutexLock locker{&m_stateMutex};
        m_matchIndex[client.id()] = response.match_index();
        m_nextIndex[client.id()] = response.match_index() + 1;

        uint32_t majorityIndex = findMajorityIndexMatch();
        if (majorityIndex > m_commitIndex && m_log.size() >= majorityIndex)
        {
            if (m_log[majorityIndex - 1].term() == m_currentTerm)
            {
                if (!updatePersistentState(majorityIndex, std::nullopt))
                {
                    spdlog::error("Node={} unable to persist commitIndex={}",
                                  m_config.m_id,
                                  m_commitIndex);
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
    }
    else
    {
        {
            absl::MutexLock locker{&m_stateMutex};
            m_nextIndex[client.id()] =
                std::max(1U, m_nextIndex[client.id()] - 1);
        }
        return false;
    }

    return true;
}

void consensus_module_t::runElectionThread(std::stop_token token) noexcept
{
    while (!token.stop_requested() && !m_shutdown)
    {
        {
            absl::ReleasableMutexLock locker{&m_stateMutex};
            {
                if (getState() == NodeState::LEADER)
                {
                    continue;
                }
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

        spdlog::debug("Node={} starts election. New term={}",
                      m_config.m_id,
                      m_currentTerm);

        // Node in a canditate state should vote for itself.
        m_voteCount++;
        if (!updatePersistentState(std::nullopt, m_config.m_id))
        {
            spdlog::error("Node={} is unable to persist votedFor={}",
                          m_config.m_id,
                          m_votedFor);
        }

        request.set_term(m_currentTerm);
        request.set_candidateid(m_config.m_id);
        request.set_lastlogterm(getLastLogTerm());
        request.set_lastlogindex(getLastLogIndex());
    }

    sendRequestVoteRPCs(request, newTerm);
}

void consensus_module_t::sendRequestVoteRPCs(const RequestVoteRequest &request,
                                             std::uint64_t             newTerm)
{
    std::vector<std::jthread> requesterThreads;
    requesterThreads.reserve(m_replicas.size());
    for (auto &[id, client] : m_replicas)
    {
        spdlog::debug("Node={} is creating RequestVoteRPC thread for the "
                      "peer={} during term={}",
                      m_config.m_id,
                      id,
                      newTerm);

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

                spdlog::debug("Received RequestVoteResponse in requester "
                              "thread peerTerm={} "
                              "voteGranted={} peer={}",
                              responseTerm,
                              voteGranted,
                              response.responderid());

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
            });
    }

    for (auto &thread : requesterThreads)
    {
        spdlog::debug("Node={} is joining RequestVoteRPC thread",
                      m_config.m_id);
        thread.join();
    }
}

auto consensus_module_t::getLastLogIndex() const -> uint32_t
{
    return m_log.empty() ? 0 : m_log.back().index();
}

auto consensus_module_t::getLastLogTerm() const -> uint32_t
{
    return m_log.empty() ? 0 : m_log.back().term();
}

auto consensus_module_t::hasMajority(uint32_t votes) const -> bool
{
    const uint32_t clusterSize = static_cast<uint32_t>(m_replicas.size()) + 1;
    return (votes >= ((clusterSize / 2) + 1));
}

auto consensus_module_t::getLogTerm(uint32_t index) const -> uint32_t
{
    if (index == 0 || index > m_log.size())
    {
        return 0;
    }

    return m_log[index - 1].term();
}

auto consensus_module_t::findMajorityIndexMatch() const -> uint32_t
{
    std::vector<int> matchIndexes;
    matchIndexes.resize(m_replicas.size() + 1);

    uint32_t localLastIndex = getLastLogIndex();
    matchIndexes.emplace_back(localLastIndex);

    for (const auto &[peer, matchIdx] : m_matchIndex)
    {
        matchIndexes.emplace_back(matchIdx);
    }

    if (matchIndexes.empty())
    {
        return 0;
    }

    std::ranges::sort(matchIndexes);

    return matchIndexes[matchIndexes.size() / 2];
}

auto consensus_module_t::waitForMajorityReplication(uint32_t logIndex) const
    -> bool
{
    auto hasMajorityLambda = [this, logIndex]()
                                 ABSL_SHARED_LOCKS_REQUIRED(m_stateMutex)
    {
        uint32_t count = 1;
        for (const auto &[peer, matchIdx] : m_matchIndex)
        {
            count += matchIdx >= logIndex;
        }
        return hasMajority(count);
    };

    spdlog::info("Node={} is waiting for majority to agree on logIndex={}",
                 m_config.m_id,
                 logIndex);

    const auto timeout{absl::Seconds(generate_raft_timeout())};
    return m_stateMutex.AwaitWithTimeout(absl::Condition(&hasMajorityLambda),
                                         timeout);
}

auto consensus_module_t::initializePersistentState() -> bool
{
    if (!restorePersistentState())
    {
        spdlog::error("Unable to restore persistent state");
        return false;
    }

    for (const auto &logEntry : m_log)
    {
        (void)logEntry;
        // TODO(lnikon): Should I update the state machine here?
        // m_kv[logEntry.key()] = logEntry.value();
    }

    return true;
}

auto consensus_module_t::updatePersistentState(
    std::optional<std::uint32_t> commitIndex,
    std::optional<std::uint32_t> votedFor) -> bool
{
    m_commitIndex =
        commitIndex.has_value() ? commitIndex.value() : m_commitIndex;
    m_votedFor = votedFor.has_value() ? votedFor.value() : m_votedFor;
    /*return true;*/
    return flushPersistentState();
}

auto consensus_module_t::flushPersistentState() -> bool
{
    // Flush commitIndex and votedFor
    {
        auto path = std::filesystem::path(
            constructFilename(gRaftFilename, m_config.m_id));
        std::ofstream fsa(path, std::fstream::out | std::fstream::trunc);
        if (!fsa.is_open())
        {
            spdlog::error("Node={} is unable to open {} to flush "
                          "commitIndex={} and votedFor={}",
                          m_config.m_id,
                          path.c_str(),
                          m_commitIndex,
                          m_votedFor);
            return false;
        }

        fsa << m_commitIndex << " " << m_votedFor << "\n";
        if (fsa.fail())
        {
            spdlog::error("Node={} is unable to write into {} the "
                          "commitIndex={} and votedFor={}",
                          m_config.m_id,
                          path.c_str(),
                          m_commitIndex,
                          m_votedFor);
            return false;
        }

        fsa.flush();
        if (fsa.fail())
        {
            return false;
        }
    }

    // Flush the log
    {
        auto path = std::filesystem::path(
            constructFilename(gLogFilename, m_config.m_id));
        std::ofstream fsa(path, std::fstream::out | std::fstream::trunc);
        if (!fsa.is_open())
        {
            spdlog::error("Node={} is unable to open {} to flush the log",
                          m_config.m_id,
                          path.c_str());
            return false;
        }

        for (const auto &entry : m_log)
        {
            fsa << entry.key() << " " << entry.value() << " " << entry.term()
                << "\n";

            if (fsa.fail())
            {
                spdlog::error("Node={} is unable to write into {} the "
                              "commitIndex={} and votedFor={}",
                              m_config.m_id,
                              path.c_str(),
                              m_commitIndex,
                              m_votedFor);
                return false;
            }
        }
        fsa.flush();
    }

    return true;
}

auto consensus_module_t::restorePersistentState() -> bool
{
    {
        auto path = std::filesystem::path(
            constructFilename(gRaftFilename, m_config.m_id));
        if (!std::filesystem::exists(path))
        {
            spdlog::info("Node={} is running the first time", m_config.m_id);
            return true;
        }

        std::ifstream ifs(path, std::ifstream::in);
        if (!ifs.is_open())
        {
            spdlog::error("Node={} is unable to open {} to restore commitIndex "
                          "and votedFor",
                          m_config.m_id,
                          path.c_str());
            return false;
        }

        ifs >> m_commitIndex >> m_votedFor;
        m_votedFor = 0;
        spdlog::info("Node={} restored commitIndex={} and votedFor={}",
                     m_config.m_id,
                     m_commitIndex,
                     m_votedFor);
    }

    {
        auto path = std::filesystem::path(
            constructFilename(gLogFilename, m_config.m_id));
        std::ifstream ifs(path, std::ifstream::in);
        if (!ifs.is_open())
        {
            spdlog::error("Node={} is unable to open {} to restore log",
                          m_config.m_id,
                          path.c_str());
            return false;
        }

        std::string logLine;
        while (std::getline(ifs, logLine))
        {
            std::stringstream sst(logLine);

            std::string   key;
            std::string   value;
            std::uint32_t term = 0;

            sst >> key >> value >> term;

            LogEntry logEntry;
            logEntry.set_key(key);
            logEntry.set_value(value);
            logEntry.set_term(term);
            m_log.emplace_back(logEntry);

            spdlog::info(
                "Node={} restored logEntry=[key={}, value={}, term={}]",
                m_config.m_id,
                key,
                value,
                term);
        }
    }

    return true;
}

void consensus_module_t::cleanupHeartbeatThread()
{
    if (m_heartbeatThread.joinable())
    {
        spdlog::info("Node={} clearing existing heartbeat threads",
                     m_config.m_id);
        m_heartbeatThread.request_stop();
        m_heartbeatThread.join();
        spdlog::info("Node={} existing heartbeat threads cleared",
                     m_config.m_id);
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

} // namespace raft

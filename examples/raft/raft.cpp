#include "raft.h"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/support/status.h>

#include <absl/synchronization/mutex.h>
#include <absl/time/time.h>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

static bool gFirstElection = true;

namespace
{

const std::string_view gRaftFilename = "RAFT_PERSISTENCE";
const std::string_view gLogFilename = "RAFT_LOG";

auto constructFilename(std::string_view filename, std::uint32_t id) -> std::string
{
    return fmt::format("{}_NODE_{}", filename, id);
}

} // namespace

NodeClient::NodeClient(ID nodeId, IP nodeIp)
    : m_id{nodeId},
      m_ip{std::move(nodeIp)},
      m_channel(grpc::CreateChannel(m_ip, grpc::InsecureChannelCredentials())),
      m_stub(RaftService::NewStub(m_channel)),
      m_kvStub(TinyKVPPService::NewStub(m_channel))
{
    assert(m_id > 0);
    assert(!m_ip.empty());

    if (!m_channel)
    {
        throw std::runtime_error(fmt::format("Failed to establish a gRPC channel for node={} ip={}", m_id, m_ip));
    }

    if (!m_stub)
    {
        throw std::runtime_error(fmt::format("Failed to create a stub for node={} ip={}", m_id, m_ip));
    }

    if (!m_kvStub)
    {
        throw std::runtime_error(fmt::format("Failed to create a KV stub for node={} ip={}", m_id, m_ip));
    }
}

auto NodeClient::appendEntries(const AppendEntriesRequest &request, AppendEntriesResponse *response) -> bool
{
    const auto RPC_TIMEOUT = std::chrono::seconds(generateRandomTimeout());

    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + RPC_TIMEOUT);
    grpc::Status status = m_stub->AppendEntries(&context, request, response);
    if (!status.ok())
    {
        spdlog::error("AppendEntries RPC call failed");
        return false;
    }

    return true;
}

auto NodeClient::requestVote(const RequestVoteRequest &request, RequestVoteResponse *response) -> bool
{
    const auto RPC_TIMEOUT = std::chrono::seconds(generateRandomTimeout());

    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + RPC_TIMEOUT);
    grpc::Status status = m_stub->RequestVote(&context, request, response);
    if (!status.ok())
    {
        spdlog::error("RequestVote RPC call failed. Error code={} and message={}",
                      static_cast<int>(status.error_code()),
                      status.error_message());
        return false;
    }

    return true;
}

auto NodeClient::put(const PutRequest &request, PutResponse *pResponse) -> bool
{
    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(generateRandomTimeout()));
    grpc::Status status = m_kvStub->Put(&context, request, pResponse);
    if (!status.ok())
    {
        spdlog::error("Put RPC call failed. Error code={} and message={}",
                      static_cast<int>(status.error_code()),
                      status.error_message());
        return false;
    }

    return true;
}

auto NodeClient::getId() const -> ID
{
    return m_id;
}

ConsensusModule::ConsensusModule(ID nodeId, std::vector<IP> replicas)
    : m_id{nodeId},
      m_currentTerm{0},
      m_votedFor{0},
      m_state{NodeState::FOLLOWER},
      m_commitIndex{0},
      m_lastApplied{0},
      m_voteCount{0}
{
    assert(m_id > 0);
    assert(replicas.size() > 0);
    assert(m_id <= replicas.size());

    m_ip = replicas[m_id - 1];

    grpc::ServerBuilder builder;
    builder.AddListeningPort(m_ip, grpc::InsecureServerCredentials());

    auto *raftService = dynamic_cast<RaftService::Service *>(this);
    if (raftService == nullptr)
    {
        throw std::runtime_error(fmt::format("Failed to dynamic_cast ConsensusModule to RaftService"));
    }
    builder.RegisterService(raftService);

    auto *tkvppService = dynamic_cast<TinyKVPPService::Service *>(this);
    if (tkvppService == nullptr)
    {
        throw std::runtime_error(fmt::format("Failed to dynamic_cast ConsensusModule to TinyKVPPService"));
    }
    builder.RegisterService(tkvppService);

    m_raftServer = builder.BuildAndStart();
    if (!m_raftServer)
    {
        throw std::runtime_error(fmt::format("Failed to create a gRPC server for node={} ip={}", m_id, m_ip));
    }

    std::size_t id{1};
    for (const auto &ip : replicas)
    {
        if (id != m_id)
        {
            m_replicas[id] = NodeClient(id, ip);
            m_matchIndex[id] = 0;
            m_nextIndex[id] = 1;
        }

        ++id;
    }
}

auto ConsensusModule::AppendEntries(grpc::ServerContext        *pContext,
                                    const AppendEntriesRequest *pRequest,
                                    AppendEntriesResponse      *pResponse) -> grpc::Status
{
    (void)pContext;
    (void)pRequest;
    (void)pResponse;

    spdlog::debug("Recevied AppendEntries RPC from leader={} during term={}", pRequest->senderid(), pRequest->term());

    absl::MutexLock locker(&m_stateMutex);

    // 1. Term check
    if (pRequest->term() < m_currentTerm)
    {
        pResponse->set_term(m_currentTerm);
        pResponse->set_success(false);
        pResponse->set_responderid(m_id);
        return grpc::Status::OK;
    }

    if (pRequest->term() > m_currentTerm)
    {
        becomeFollower(pRequest->term());
    }

    // 2. Log consistency check
    if (pRequest->prevlogindex() > 0)
    {
        if (m_log.size() < pRequest->prevlogindex() ||
            (m_log[pRequest->prevlogindex() - 1].term() != pRequest->prevlogterm()))
        {
            pResponse->set_term(m_currentTerm);
            pResponse->set_success(false);
            pResponse->set_responderid(m_id);
            return grpc::Status::OK;
        }
    }

    // 3. Append new entries and remove conflicting ones
    auto newEntryStart = pRequest->prevlogindex() + 1;
    for (auto idx{0}; idx < pRequest->entries().size(); ++idx)
    {
        const auto newIdx{newEntryStart + idx - 1};
        if (m_log.size() >= newEntryStart + idx && m_log[newIdx].term() != pRequest->entries(idx).term())
        {
            m_log.resize(newIdx);
            break;
        }
    }

    m_log.insert(m_log.end(), pRequest->entries().begin(), pRequest->entries().end());

    if (pRequest->leadercommit() > m_commitIndex)
    {
        updatePersistentState(std::min(pRequest->leadercommit(), (uint32_t)m_log.size()), std::nullopt);

        while (m_lastApplied < m_commitIndex)
        {
            ++m_lastApplied;
            m_kv[m_log[m_lastApplied - 1].key()] = m_log[m_lastApplied - 1].value();
        }
    }

    pResponse->set_term(m_currentTerm);
    pResponse->set_success(true);
    pResponse->set_responderid(m_id);
    pResponse->set_match_index(m_log.size());

    updatePersistentState(std::nullopt, pRequest->senderid());

    m_leaderHeartbeatReceived.store(true);

    spdlog::debug("Node={} is resetting election timeout at term={}", m_id, m_currentTerm);

    return grpc::Status::OK;
}

auto ConsensusModule::RequestVote(grpc::ServerContext      *pContext,
                                  const RequestVoteRequest *pRequest,
                                  RequestVoteResponse      *pResponse) -> grpc::Status
{
    (void)pContext;

    absl::WriterMutexLock locker(&m_stateMutex);

    spdlog::debug("Received RequestVote RPC from candidate={} during term={} peerTerm={}",
                  pRequest->candidateid(),
                  m_currentTerm,
                  pRequest->term());

    pResponse->set_term(m_currentTerm);
    pResponse->set_votegranted(0);
    pResponse->set_responderid(m_id);

    // Become follower if candidates has higher term
    if (pRequest->term() > m_currentTerm)
    {
        becomeFollower(pRequest->term());
    }

    // Don't grant vote to the candidate if the nodes term is higher
    if (pRequest->term() < m_currentTerm)
    {
        return grpc::Status::OK;
    }

    // Grant vote to the candidate if the node hasn't voted yet and
    // candidates log is at least as up-to-date as receiver's log
    if (m_votedFor == 0 || m_votedFor == pRequest->candidateid())
    {
        if (pRequest->lastlogterm() > getLastLogTerm() ||
            (pRequest->lastlogterm() == getLastLogTerm() && pRequest->lastlogindex() >= getLastLogIndex()))
        {

            updatePersistentState(std::nullopt, pRequest->candidateid());
            pResponse->set_term(m_currentTerm);
            pResponse->set_votegranted(1);
            m_leaderHeartbeatReceived.store(true);
        }
    }

    return grpc::Status::OK;
}

auto ConsensusModule::Put(grpc::ServerContext *pContext, const PutRequest *pRequest, PutResponse *pResponse)
    -> grpc::Status
{
    (void)pContext;

    spdlog::info("Node={} received Put request", m_id);

    uint32_t currentTerm = 0;
    uint32_t lastLogIndex = 0;
    uint32_t votedFor = 0;
    {
        absl::MutexLock locker{&m_stateMutex};
        if (m_state != NodeState::LEADER)
        {
            if (m_votedFor != invalidId)
            {
                votedFor = m_votedFor;
            }
            else
            {
                spdlog::error("Non-leader node={} received a put request. Leader at current term is unkown.", m_id);
                pResponse->set_status("");
                return grpc::Status::OK;
            }
        }

        currentTerm = m_currentTerm;
        lastLogIndex = getLastLogIndex() + 1;
    }

    if (votedFor != invalidId)
    {
        spdlog::info("Non-leader node={} received a put request. Forwarding to leader={} during currentTerm={}",
                     m_id,
                     votedFor,
                     currentTerm);

        if (!m_replicas[votedFor]->put(*pRequest, pResponse))
        {
            spdlog::error("Non-leader node={} was unable to forward put RPC to leader={}", m_id, votedFor);
        }

        return grpc::Status::OK;
    }

    LogEntry logEntry;
    logEntry.set_term(currentTerm);
    logEntry.set_index(lastLogIndex);
    logEntry.set_command(fmt::format("put:{}:{}", pRequest->key(), pRequest->value()));
    logEntry.set_key(pRequest->key());
    logEntry.set_value(pRequest->value());

    {
        absl::MutexLock locker{&m_stateMutex};
        m_log.push_back(logEntry);
    }

    for (auto &[id, client] : m_replicas)
    {
        sendAppendEntriesRPC(client.value(), {logEntry});
    }

    absl::MutexLock locker{&m_stateMutex};
    bool            success = waitForMajorityReplication(logEntry.index());
    if (success)
    {
        spdlog::info("Node={} majority agreed on logEntry={}", m_id, logEntry.index());
    }
    else
    {
        spdlog::info("Node={} majority failed to agree on logEntry={}", m_id, logEntry.index());
    }

    return grpc::Status::OK;
}

auto ConsensusModule::Get(grpc::ServerContext *pContext, const GetRequest *pRequest, GetResponse *pResponse)
    -> grpc::Status
{
    (void)pContext;

    spdlog::info("Node={} recevied get request for key={}", m_id, pRequest->key());

    absl::MutexLock locker{&m_stateMutex};
    if (auto it = m_kv.find(pRequest->key()); it != m_kv.end())
    {
        pResponse->set_value(it->second);
    }
    else
    {
        pResponse->set_value(std::string());
    }

    return grpc::Status::OK;
}

auto ConsensusModule::init() -> bool
{
    absl::MutexLock locker{&m_stateMutex};
    if (!initializePersistentState())
    {
        spdlog::warn("Unable to initialize persistent state!");
        return false;
    }

    return true;
}

void ConsensusModule::start()
{
    m_electionThread = std::jthread(
        [this](std::stop_token token)
        {
            while (!m_stopElection)
            {
                if (token.stop_requested())
                {
                    spdlog::info("Stopping election timer thread");
                    return;
                }

                {
                    absl::MutexLock locker(&m_stateMutex);
                    if (getState() == NodeState::LEADER)
                    {
                        continue;
                    }
                }

                auto currentTimeMs = []
                {
                    return std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::high_resolution_clock::now().time_since_epoch())
                        .count();
                };

                // Wait until heartbeat timeouts or timer CV gets signaled

                {
                    absl::MutexLock locker(&m_timerMutex); // Lock the mutex using Abseil's MutexLock

                    // Determine the timeout duration
                    int64_t timeToWaitMs = gFirstElection ? generateRandomTimeout() : 1'000'000'000;
                    int64_t timeToWaitDeadlineMs = currentTimeMs() + timeToWaitMs;

                    // Define the condition to wait for leader's heartbeat
                    auto heartbeatReceivedCondition = [this, &timeToWaitDeadlineMs, currentTimeMs]()
                    { return m_leaderHeartbeatReceived.load() || currentTimeMs() >= timeToWaitDeadlineMs; };

                    spdlog::debug("Timer thread at node={} will block for {}ms for the leader to send a heartbeat",
                                  m_id,
                                  timeToWaitMs);

                    // Wait for the condition to be met or timeout
                    bool heartbeatReceived = m_timerMutex.AwaitWithTimeout(absl::Condition(&heartbeatReceivedCondition),
                                                                           absl::Milliseconds(timeToWaitMs));

                    // If timer CV gets signaled, then node has received the heartbeat from the leader.
                    // Otherwise, heartbeat timed out and node needs to start the new leader election
                    if (heartbeatReceived && m_leaderHeartbeatReceived.load())
                    {
                        gFirstElection = false;
                        spdlog::debug("Node={} received heartbeat", m_id);
                        m_leaderHeartbeatReceived.store(false);
                    }
                    else
                    {
                        startElection();
                    }
                }
            }
        });

    {
        assert(m_raftServer);
        spdlog::debug("Listening for RPC requests on ");
        m_raftServer->Wait();
    }
}

void ConsensusModule::startServer()
{
    m_raftServer->Wait();
}

void ConsensusModule::stop()
{
    absl::WriterMutexLock locker{&m_stateMutex};

    m_stopElection = true;

    m_electionThread.request_stop();
    m_electionThread.join();

    for (auto &heartbeatThread : m_heartbeatThreads)
    {
        heartbeatThread.request_stop();
        heartbeatThread.join();
    }
    m_heartbeatThreads.clear();

    if (m_raftServer)
    {
        m_raftServer->Shutdown();
    }

    /*if (m_serverThread.joinable())*/
    {
        m_serverThread.request_stop();
        m_serverThread.join();
    }
}

auto ConsensusModule::initializePersistentState() -> bool
{
    if (!restorePersistentState())
    {
        spdlog::error("Unable to restore persistent state");
        return false;
    }

    for (const auto &logEntry : m_log)
    {
        m_kv[logEntry.key()] = logEntry.value();
    }

    return true;
}

void ConsensusModule::startElection()
{
    RequestVoteRequest request;
    {
        absl::WriterMutexLock locker(&m_stateMutex);
        m_currentTerm++;
        m_state = NodeState::CANDIDATE;

        spdlog::debug("Node={} starts election. New term={}", m_id, m_currentTerm);

        // Node in a canditate state should vote for itself.
        m_voteCount++;
        updatePersistentState(std::nullopt, m_id);

        request.set_term(m_currentTerm);
        request.set_candidateid(m_id);
        request.set_lastlogterm(getLastLogTerm());
        request.set_lastlogindex(getLastLogIndex());
    }

    std::vector<std::jthread> requesterThreads;
    requesterThreads.reserve(m_replicas.size());
    for (auto &[id, client] : m_replicas)
    {
        spdlog::debug("Node={} is creating RequestVoteRPC thread for the peer={}", m_id, id);
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
                    "Received RequestVoteResponse in requester thread peerTerm={} voteGranted={} responseTerm={}",
                    responseTerm,
                    voteGranted,
                    response.responderid());

                absl::MutexLock locker(&m_stateMutex);
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
        spdlog::debug("Node={} is joining RequestVoteRPC thread", m_id);
        thread.join();
    }
}

void ConsensusModule::becomeFollower(uint32_t newTerm)
{
    m_currentTerm = newTerm;
    m_state = NodeState::FOLLOWER;
    updatePersistentState(std::nullopt, 0);

    for (auto &heartbeatThread : m_heartbeatThreads)
    {
        heartbeatThread.request_stop();
        heartbeatThread.join();
    }
    m_heartbeatThreads.clear();

    spdlog::debug("Server reverted to follower state in term={}", m_currentTerm);
}

auto ConsensusModule::hasMajority(uint32_t votes) const -> bool
{
    constexpr const double HALF_OF_THE_REPLICAS = 2.0;
    return votes > static_cast<double>(m_replicas.size()) / HALF_OF_THE_REPLICAS;
}

void ConsensusModule::becomeLeader()
{
    assert((m_state != NodeState::LEADER && m_heartbeatThreads.empty()) ||
           (m_state == NodeState::LEADER && !m_heartbeatThreads.empty()));

    if (m_state == NodeState::LEADER)
    {
        spdlog::warn("Node={} is already a leader", m_id);
        return;
    }

    m_state = NodeState::LEADER;
    m_voteCount = 0;

    spdlog::info("Node={} become a leader at term={}", m_id, m_currentTerm);

    for (auto &[id, client] : m_replicas)
    {
        spdlog::debug("Node={} is creating a heartbeat thread for the peer={}", m_id, id);
        sendHeartbeat(client.value());
    }
}

void ConsensusModule::sendHeartbeat(NodeClient &client)
{
    constexpr const auto heartbeatInterval{std::chrono::milliseconds(100)};
    constexpr const int  maxRetries{3};

    m_heartbeatThreads.emplace_back(
        [this, maxRetries, &client, heartbeatInterval](std::stop_token token)
        {
            spdlog::debug("Node={} is starting a heartbeat thread for client={}", m_id, client.getId());

            int consecutiveFailures = 0;
            while (!token.stop_requested())
            {
                AppendEntriesRequest request;
                {
                    absl::ReaderMutexLock locker(&m_stateMutex);
                    if (m_state != NodeState::LEADER)
                    {
                        spdlog::debug("Node={} is no longer a leader. Stopping the heartbeat thread");
                        break;
                    }

                    request.set_term(m_currentTerm);
                    request.set_prevlogterm(getLastLogTerm());
                    request.set_prevlogindex(getLastLogIndex());
                    request.set_leadercommit(m_commitIndex);
                    request.set_senderid(m_id);
                }

                {
                    AppendEntriesResponse response;
                    if (!client.appendEntries(request, &response))
                    {
                        consecutiveFailures++;

                        spdlog::error("AppendEntriesRequest failed during heartbeat. Attempt {}/{}",
                                      consecutiveFailures,
                                      maxRetries);
                        if (consecutiveFailures >= maxRetries)
                        {
                            return;
                        }
                        consecutiveFailures = 0;

                        continue;
                    }

                    consecutiveFailures = 0;

                    auto responseTerm = response.term();
                    auto success = response.success();

                    spdlog::debug(
                        "Received AppendEntriesResponse in requester thread peerTerm={} success={} responderId={}",
                        responseTerm,
                        success,
                        response.responderid());

                    {
                        absl::WriterMutexLock locker(&m_stateMutex);

                        if (responseTerm > m_currentTerm)
                        {
                            becomeFollower(responseTerm);
                            break;
                        }
                    }
                }

                std::this_thread::sleep_for(heartbeatInterval);
            }

            spdlog::debug("Stopping heartbeat thread for on the node={} for the client={}", m_id, client.getId());
        });
}

void ConsensusModule::sendAppendEntriesRPC(NodeClient &client, std::vector<LogEntry> logEntries)
{
    std::thread(
        [this](NodeClient &client, std::vector<LogEntry> logEntries)
        {
            AppendEntriesRequest request;
            {
                absl::MutexLock locker{&m_stateMutex};

                request.set_term(m_currentTerm);
                request.set_prevlogterm(logEntries.front().term());
                request.set_prevlogindex(getLogTerm(logEntries.front().index() - 1));
                request.set_leadercommit(m_commitIndex);
                request.set_senderid(m_id);

                for (auto logEntry : logEntries)
                {
                    request.add_entries()->CopyFrom(logEntry);
                }
            }

            AppendEntriesResponse response;
            auto                  status = client.appendEntries(request, &response);
            if (!status)
            {
                absl::MutexLock locker{&m_stateMutex};
                spdlog::error("Node={} failed to send AppendEntries RPC to peer={} at term={}",
                              m_id,
                              client.getId(),
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

            if (response.success())
            {
                absl::MutexLock locker{&m_stateMutex};
                m_matchIndex[client.getId()] = response.match_index();
                m_nextIndex[client.getId()] = response.match_index() + 1;

                uint32_t majorityIndex = findMajorityIndexMatch();
                if (majorityIndex > m_commitIndex && m_log[majorityIndex - 1].term() == m_currentTerm)
                {
                    updatePersistentState(majorityIndex, std::nullopt);

                    // Apply successfull replication to the state machine e.g. in-memory hash-table
                    while (m_lastApplied < m_commitIndex)
                    {
                        ++m_lastApplied;
                        m_kv[m_log[m_lastApplied - 1].key()] = m_log[m_lastApplied - 1].value();
                    }

                    return;
                }
            }

            if (!response.success())
            {
                {
                    absl::MutexLock locker{&m_stateMutex};
                    m_nextIndex[client.getId()] = std::max(1U, m_nextIndex[client.getId()] - 1);
                }
                sendAppendEntriesRPC(client, {});
            }
        },
        std::ref(client),
        logEntries)
        .detach();
}

auto ConsensusModule::getLogTerm(uint32_t index) const -> uint32_t
{
    if (index == 0 || index > m_log.size())
    {
        return 0;
    }

    return m_log[index - 1].term();
}

auto ConsensusModule::findMajorityIndexMatch() -> uint32_t
{
    std::vector<int> matchIndexes;
    matchIndexes.resize(m_replicas.size() + 1);
    for (const auto &[peer, matchIdx] : m_matchIndex)
    {
        matchIndexes.emplace_back(matchIdx);
    }
    matchIndexes.emplace_back(m_log.back().index());

    std::sort(std::begin(matchIndexes), std::end(matchIndexes));

    return matchIndexes[matchIndexes.size() / 2];
}

auto ConsensusModule::waitForMajorityReplication(uint32_t logIndex) -> bool
{
    constexpr const auto replicationTimeout{5};

    auto hasMajority = [this, logIndex]() ABSL_SHARED_LOCKS_REQUIRED(m_stateMutex)
    {
        uint32_t count = 1;
        for (const auto &[peer, matchIdx] : m_matchIndex)
        {
            count += matchIdx >= logIndex;
        }
        return count >= (m_replicas.size() + 1) / 2 + 1;
    };

    spdlog::info("Node={} is waiting for majority to agree on logIndex={}", m_id, logIndex);
    return m_stateMutex.AwaitWithTimeout(absl::Condition(&hasMajority), absl::Seconds(replicationTimeout));
}

auto ConsensusModule::getLastLogIndex() const -> uint32_t
{
    return m_log.empty() ? 0 : m_log.back().index();
}

auto ConsensusModule::getLastLogTerm() const -> uint32_t
{
    return m_log.empty() ? 0 : m_log.back().term();
}

auto ConsensusModule::getState() -> NodeState
{
    return m_state;
}

auto ConsensusModule::updatePersistentState(std::optional<std::uint32_t> commitIndex,
                                            std::optional<std::uint32_t> votedFor) -> bool
{
    m_commitIndex = commitIndex.has_value() ? commitIndex.value() : m_commitIndex;
    m_votedFor = votedFor.has_value() ? votedFor.value() : m_votedFor;
    return flushPersistentState();
}

auto ConsensusModule::flushPersistentState() -> bool
{
    // Flush commitIndex and votedFor
    {
        auto          path = std::filesystem::path(constructFilename(gRaftFilename, m_id));
        std::ofstream fsa(path, std::fstream::out | std::fstream::trunc);
        if (!fsa.is_open())
        {
            spdlog::error("Node={} is unable to open {} to flush commitIndex={} and votedFor={}",
                          m_id,
                          path.c_str(),
                          m_commitIndex,
                          m_votedFor);
            return false;
        }
        fsa << m_commitIndex << " " << m_votedFor << "\n";
        fsa.flush();
        // TODO(lnikon): ::fsync(fsa->handle());
    }

    // Flush the log
    {
        auto          path = std::filesystem::path(constructFilename(gLogFilename, m_id));
        std::ofstream fsa(path, std::fstream::out | std::fstream::trunc);
        if (!fsa.is_open())
        {
            spdlog::error("Node={} is unable to open {} to flush the log", m_id, path.c_str());
            return false;
        }

        for (const auto &entry : m_log)
        {
            fsa << entry.key() << " " << entry.value() << " " << entry.term() << "\n";
        }
        fsa.flush();
        // TODO(lnikon): ::fsync(fsa->handle());
    }

    return true;
}

auto ConsensusModule::restorePersistentState() -> bool
{
    {
        auto path = std::filesystem::path(constructFilename(gRaftFilename, m_id));
        if (!std::filesystem::exists(path))
        {
            spdlog::info("Node={} is running the first time", m_id);
            return true;
        }

        std::ifstream ifs(path, std::ifstream::in);
        if (!ifs.is_open())
        {
            spdlog::error("Node={} is unable to open {} to restore commitIndex and votedFor", m_id, path.c_str());
            return false;
        }

        ifs >> m_commitIndex >> m_votedFor;
        spdlog::info("Node={} restored commitIndex={} and votedFor={}", m_id, m_commitIndex, m_votedFor);
    }

    {
        auto          path = std::filesystem::path(constructFilename(gLogFilename, m_id));
        std::ifstream ifs(path, std::ifstream::in);
        if (!ifs.is_open())
        {
            spdlog::error("Node={} is unable to open {} to restore log", m_id, path.c_str());
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

            spdlog::info("Node={} restored logEntry=[key={}, value={}, term={}]", m_id, key, value, term);
        }
    }

    return true;
}

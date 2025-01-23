#include "raft.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <random>
#include <ranges>
#include <stdexcept>
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

auto constructFilename(std::string_view filename, std::uint32_t peerId) -> std::string
{
    return fmt::format("{}_NODE_{}", filename, peerId);
}

auto generate_random_timeout() -> int
{
    const int minTimeout{150};
    const int maxTimeout{300};

    std::random_device              randomDevice;
    std::mt19937                    gen(randomDevice());
    std::uniform_int_distribution<> dist(minTimeout, maxTimeout);

    return dist(gen);
}

} // namespace

namespace raft
{

/// tkvpp_node_grpc_client_t
// tkvpp_node_grpc_client_t::tkvpp_node_grpc_client_t(node_config_t                                   config,
//                                                    std::unique_ptr<TinyKVPPService::StubInterface> pStub)
//     : m_config{std::move(config)},
//       m_stub{std::move(pStub)}
// {
//     assert(m_config.m_id > 0);
//     assert(!m_config.m_ip.empty());
// }
//
// auto tkvpp_node_grpc_client_t::put(const PutRequest &request, PutResponse *pResponse) -> bool
// {
//     grpc::ClientContext context;
//     context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(generate_random_timeout()));
//
//     grpc::Status status = m_stub->Put(&context, request, pResponse);
//     if (!status.ok())
//     {
//         spdlog::error("Put RPC call failed. Error code={} and message={}",
//                       static_cast<int>(status.error_code()),
//                       status.error_message());
//         return false;
//     }
//
//     return true;
// }
//
// auto tkvpp_node_grpc_client_t::id() const -> id_t
// {
//     return m_config.m_id;
// }
//
// auto tkvpp_node_grpc_client_t::ip() const -> ip_t
// {
//     return m_config.m_ip;
// }
//

/// raft_node_grpc_client_t
raft_node_grpc_client_t::raft_node_grpc_client_t(node_config_t                               config,
                                                 std::unique_ptr<RaftService::StubInterface> pRaftStub)
    : m_config{std::move(config)},
      m_stub{std::move(pRaftStub)}
{
    assert(m_config.m_id > 0);
    assert(!m_config.m_ip.empty());
}

auto raft_node_grpc_client_t::appendEntries(const AppendEntriesRequest &request, AppendEntriesResponse *response)
    -> bool
{
    const auto RPC_TIMEOUT = std::chrono::seconds(generate_random_timeout());

    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + RPC_TIMEOUT);
    grpc::Status status = m_stub->AppendEntries(&context, request, response);
    if (!status.ok())
    {
        spdlog::error("AppendEntries RPC to peer id={} ip={} failed. Error code={} message={}",
                      m_config.m_id,
                      m_config.m_ip,
                      static_cast<int>(status.error_code()),
                      status.error_message());
        return false;
    }

    return true;
}

auto raft_node_grpc_client_t::requestVote(const RequestVoteRequest &request, RequestVoteResponse *response) -> bool
{
    const auto RPC_TIMEOUT = std::chrono::seconds(generate_random_timeout());

    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + RPC_TIMEOUT);

    grpc::Status status = m_stub->RequestVote(&context, request, response);
    if (!status.ok())
    {
        spdlog::error("RequestVote RPC to peer id={} ip={} failed. Error code={} message={}",
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

consensus_module_t::consensus_module_t(node_config_t nodeConfig, std::vector<raft_node_grpc_client_t> replicas)
    : m_config{std::move(nodeConfig)},
      m_currentTerm{0},
      m_votedFor{0},
      m_commitIndex{0},
      m_lastApplied{0},
      m_state{NodeState::FOLLOWER},
      m_voteCount{0}
{
    assert(m_config.m_id > 0);
    assert(replicas.size() > 0);
    assert(m_config.m_id <= replicas.size() + 1);

    grpc::ServerBuilder builder;
    builder.AddListeningPort(m_config.m_ip, grpc::InsecureServerCredentials());

    auto *raftService = dynamic_cast<RaftService::Service *>(this);
    if (raftService == nullptr)
    {
        throw std::runtime_error(fmt::format("Failed to dynamic_cast ConsensusModule to RaftService"));
    }
    builder.RegisterService(raftService);

    m_raftServer = builder.BuildAndStart();
    if (!m_raftServer)
    {
        throw std::runtime_error(
            fmt::format("Failed to create a gRPC server for node={} ip={}", m_config.m_id, m_config.m_ip));
    }

    for (auto &&nodeClient : replicas)
    {
        m_matchIndex[nodeClient.id()] = 0;
        m_nextIndex[nodeClient.id()] = 1;
        m_replicas[nodeClient.id()] = std::move(nodeClient);
    }
}

auto consensus_module_t::AppendEntries(grpc::ServerContext        *pContext,
                                       const AppendEntriesRequest *pRequest,
                                       AppendEntriesResponse      *pResponse) -> grpc::Status
{
    (void)pContext;
    (void)pRequest;
    (void)pResponse;

    spdlog::debug("Node={} recevied AppendEntries RPC from leader={} during term={}",
                  m_config.m_id,
                  pRequest->senderid(),
                  pRequest->term());

    absl::WriterMutexLock locker(&m_stateMutex);

    // 1. Term check
    if (pRequest->term() < m_currentTerm)
    {
        spdlog::debug(
            "Node={} receviedTerm={} is smaller than currentTerm={}", m_config.m_id, pRequest->term(), m_currentTerm);
        pResponse->set_term(m_currentTerm);
        pResponse->set_success(false);
        pResponse->set_responderid(m_config.m_id);
        return grpc::Status::OK;
    }

    if (pRequest->term() > m_currentTerm)
    {
        spdlog::debug("Node={} receviedTerm={} is higher than currentTerm={}. Reverting to follower",
                      m_config.m_id,
                      pRequest->term(),
                      m_currentTerm);
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
            pResponse->set_responderid(m_config.m_id);
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
        // Update m_commitIndex
        if (!updatePersistentState(std::min(pRequest->leadercommit(), (uint32_t)m_log.size()), std::nullopt))
        {
            spdlog::error("Node={} is unable to persist commitIndex", m_config.m_id, m_commitIndex);
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
        spdlog::error("Node={} is unable to persist votedFor", m_config.m_id, m_votedFor);
    }

    {
        absl::WriterMutexLock locker{&m_timerMutex};
        m_leaderHeartbeatReceived.store(true);
    }

    spdlog::debug("Node={} is resetting election timeout at term={}", m_config.m_id, m_currentTerm);

    return grpc::Status::OK;
}

auto consensus_module_t::RequestVote(grpc::ServerContext      *pContext,
                                     const RequestVoteRequest *pRequest,
                                     RequestVoteResponse      *pResponse) -> grpc::Status
{
    (void)pContext;

    absl::WriterMutexLock locker(&m_stateMutex);

    spdlog::debug("Node={} received RequestVote RPC from candidate={} during term={} peerTerm={}",
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
        spdlog::debug("receivedTerm={} is lower than currentTerm={}", pRequest->term(), m_currentTerm);
        return grpc::Status::OK;
    }

    // Grant vote to the candidate if the node hasn't voted yet and
    // candidates log is at least as up-to-date as receiver's log
    if (m_votedFor == 0 || m_votedFor == pRequest->candidateid())
    {
        spdlog::debug("votedFor={} candidateid={}", m_votedFor, pRequest->candidateid());
        if (pRequest->lastlogterm() > getLastLogTerm() ||
            (pRequest->lastlogterm() == getLastLogTerm() && pRequest->lastlogindex() >= getLastLogIndex()))
        {
            if (!updatePersistentState(std::nullopt, pRequest->candidateid()))
            {
                spdlog::error("Node={} is unable to persist votedFor", m_config.m_id, m_votedFor);
            }

            spdlog::debug("Node={} votedFor={}", m_config.m_id, pRequest->candidateid());

            {
                absl::WriterMutexLock locker{&m_timerMutex};
                m_leaderHeartbeatReceived.store(true);
            }

            pResponse->set_term(m_currentTerm);
            pResponse->set_votegranted(1);
        }
    }

    return grpc::Status::OK;
}

// auto consensus_module_t::Put(grpc::ServerContext *pContext, const PutRequest *pRequest, PutResponse *pResponse)
//     -> grpc::Status
// {
//     (void)pContext;
//
//     spdlog::info("Node={} received Put request", m_config.m_id);
//
//     uint32_t currentTerm = 0;
//     uint32_t lastLogIndex = 0;
//     uint32_t votedFor = 0;
//     {
//         absl::MutexLock locker{&m_stateMutex};
//         if (m_state != NodeState::LEADER)
//         {
//             if (m_votedFor != gInvalidId)
//             {
//                 votedFor = m_votedFor;
//             }
//             else
//             {
//                 spdlog::error("Non-leader node={} received a put request. Leader at current term is unkown.",
//                               m_config.m_id);
//                 pResponse->set_status("");
//                 return grpc::Status::OK;
//             }
//         }
//
//         currentTerm = m_currentTerm;
//         lastLogIndex = getLastLogIndex() + 1;
//     }
//
//     if (votedFor != gInvalidId)
//     {
//         spdlog::info("Non-leader node={} received a put request. Forwarding to leader={} during currentTerm={}",
//                      m_config.m_id,
//                      votedFor,
//                      currentTerm);
//
//         // TODO(lnikon): How to handle redirects?
//         // if (!m_kvClients[votedFor]->put(*pRequest, pResponse))
//         {
//             spdlog::error("Non-leader node={} was unable to forward put RPC to leader={}", m_config.m_id, votedFor);
//         }
//
//         return grpc::Status::OK;
//     }
//
//     LogEntry logEntry;
//     logEntry.set_term(currentTerm);
//     logEntry.set_index(lastLogIndex);
//     logEntry.set_command(fmt::format("put:{}:{}", pRequest->key(), pRequest->value()));
//     logEntry.set_key(pRequest->key());
//     logEntry.set_value(pRequest->value());
//
//     {
//         absl::MutexLock locker{&m_stateMutex};
//         m_log.push_back(logEntry);
//     }
//
//     for (auto &[id, client] : m_replicas)
//     {
//         sendAppendEntriesRPC(client.value(), {logEntry});
//     }
//
//     absl::WriterMutexLock locker{&m_stateMutex};
//     bool                  success = waitForMajorityReplication(logEntry.index());
//     if (success)
//     {
//         spdlog::info("Node={} majority agreed on logEntry={}", m_config.m_id, logEntry.index());
//     }
//     else
//     {
//         spdlog::info("Node={} majority failed to agree on logEntry={}", m_config.m_id, logEntry.index());
//     }
//
//     return grpc::Status::OK;
// }

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
    m_electionThread = std::jthread(
        [this](std::stop_token token)
        {
            id_t currentLeaderId{gInvalidId};
            while (!token.stop_requested())
            {
                if (m_shutdown)
                {
                    break;
                }

                {
                    absl::ReaderMutexLock locker{&m_stateMutex};
                    currentLeaderId = m_config.m_id;
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
                    absl::WriterMutexLock locker(&m_timerMutex); // Lock the mutex using Abseil's MutexLock

                    // Determine the timeout duration
                    int64_t timeToWaitMs = generate_random_timeout();
                    int64_t timeToWaitDeadlineMs = currentTimeMs() + timeToWaitMs;

                    // Wake up when
                    // 1) Thread should be stopped
                    // 2) Leader sent a heartbeat
                    // 3) Wait for the heartbeat was too long
                    auto heartbeatReceivedCondition = [this, &timeToWaitDeadlineMs, &token, currentTimeMs]()
                    {
                        return token.stop_requested() || m_leaderHeartbeatReceived.load() ||
                               currentTimeMs() >= timeToWaitDeadlineMs;
                    };

                    spdlog::debug("Timer thread at node={} will block for {}ms for the leader={} to send a heartbeat",
                                  m_config.m_id,
                                  timeToWaitMs,
                                  currentLeaderId);

                    // Wait for the condition to be met or timeout
                    bool heartbeatReceived = m_timerMutex.AwaitWithTimeout(absl::Condition(&heartbeatReceivedCondition),
                                                                           absl::Milliseconds(timeToWaitMs));

                    if (token.stop_requested())
                    {
                        spdlog::info("Node={} election thread terminating due to stop request.", m_config.m_id);
                        break;
                    }

                    // If timer CV gets signaled, then node has received the heartbeat from the leader.
                    // Otherwise, heartbeat timed out and node needs to start the new leader election
                    if (heartbeatReceived && m_leaderHeartbeatReceived.load())
                    {
                        spdlog::debug("Node={} received heartbeat from leader={}", m_config.m_id, currentLeaderId);
                        m_leaderHeartbeatReceived.store(false);
                    }
                    else
                    {
                        startElection();
                    }
                }
            }

            spdlog::info("Election thread received stop_request");
        });

    m_serverThread = std::jthread(
        [this]()
        {
            assert(m_raftServer);
            spdlog::debug("Node={} listening for RPC requests on {}", m_config.m_id, m_config.m_ip);
            m_raftServer->Wait();
        });
}

void consensus_module_t::stop()
{
    absl::ReaderMutexLock locker{&m_stateMutex};

    spdlog::info("Shutting down consensus module");

    m_shutdown = true;

    if (m_electionThread.joinable())
    {
        spdlog::info("Joining election thread...");
        m_electionThread.request_stop();
        m_electionThread.join();
        spdlog::info("Election thread joined!");
    }

    for (auto &heartbeatThread : m_heartbeatThreads)
    {
        spdlog::info("Joining heartbeat thread...");
        heartbeatThread.request_stop();
        heartbeatThread.join();
        spdlog::info("Heartbeat thread joined!");
    }
    m_heartbeatThreads.clear();

    if (m_raftServer)
    {
        spdlog::info("Shutting down Raft gRPC server...");
        m_raftServer->Shutdown();
        spdlog::info("Raft gRPC server shutdown!");
    }

    if (m_serverThread.joinable())
    {
        spdlog::info("Joining server thread...");
        m_serverThread.request_stop();
        m_serverThread.join();
        spdlog::info("Server thread joined!");
    }

    spdlog::info("Consensus module stopped gracefully!");
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

void consensus_module_t::startElection()
{
    RequestVoteRequest request;
    {
        absl::WriterMutexLock locker(&m_stateMutex);
        m_currentTerm++;
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

    std::vector<std::jthread> requesterThreads;
    requesterThreads.reserve(m_replicas.size());
    for (auto &[id, client] : m_replicas)
    {
        spdlog::debug("Node={} is creating RequestVoteRPC thread for the peer={}", m_config.m_id, id);
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

                spdlog::debug("Received RequestVoteResponse in requester thread peerTerm={} voteGranted={} peer={}",
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
        spdlog::debug("Node={} is joining RequestVoteRPC thread", m_config.m_id);
        thread.join();
    }
}

void consensus_module_t::becomeFollower(uint32_t newTerm)
{
    m_currentTerm = newTerm;
    m_state = NodeState::FOLLOWER;
    spdlog::debug("Node={} reverted to follower state in term={}", m_config.m_id, m_currentTerm);

    if (!updatePersistentState(std::nullopt, 0))
    {
        spdlog::error("Node={} is unable to persist votedFor={}", m_config.m_id, m_votedFor);
    }

    spdlog::debug("Follower node={} is joining heartbeat threads");
    m_shutdownHeartbeatThreads = true;
    for (auto &heartbeatThread : m_heartbeatThreads)
    {
        heartbeatThread.request_stop();
        heartbeatThread.join();
    }
    m_heartbeatThreads.clear();
    spdlog::debug("Follower node={} is joining heartbeat threads finished");
}

auto consensus_module_t::hasMajority(uint32_t votes) const -> bool
{
    constexpr const double HALF_OF_THE_REPLICAS = 2.0;
    return votes > static_cast<double>(m_replicas.size()) / HALF_OF_THE_REPLICAS;
}

void consensus_module_t::becomeLeader()
{
    assert((m_state != NodeState::LEADER && m_heartbeatThreads.empty()) ||
           (m_state == NodeState::LEADER && !m_heartbeatThreads.empty()));

    if (m_state == NodeState::LEADER)
    {
        spdlog::warn("Node={} is already a leader", m_config.m_id);
        return;
    }

    m_state = NodeState::LEADER;
    m_voteCount = 0;

    spdlog::info("Node={} become a leader at term={}", m_config.m_id, m_currentTerm);

    for (auto &[id, client] : m_replicas)
    {
        spdlog::debug("Node={} is creating a heartbeat thread for the peer={}", m_config.m_id, id);
        sendHeartbeat(client.value());
    }
}

void consensus_module_t::sendHeartbeat(raft_node_grpc_client_t &client)
{
    constexpr const auto heartbeatInterval{std::chrono::milliseconds(100)};
    constexpr const int  maxRetries{3};

    m_heartbeatThreads.emplace_back(
        [this, maxRetries, &client, heartbeatInterval](std::stop_token token)
        {
            spdlog::debug("Node={} is starting a heartbeat thread for client={}", m_config.m_id, client.id());

            int consecutiveFailures = 0;
            while (!token.stop_requested())
            {
                if (m_shutdown || m_shutdownHeartbeatThreads)
                {
                    break;
                }

                AppendEntriesRequest request;
                {
                    absl::ReaderMutexLock locker{&m_stateMutex};
                    if (m_state != NodeState::LEADER)
                    {
                        spdlog::debug("Node={} is no longer a leader. Stopping the heartbeat thread");
                        break;
                    }

                    request.set_term(m_currentTerm);
                    request.set_prevlogterm(getLastLogTerm());
                    request.set_prevlogindex(getLastLogIndex());
                    request.set_leadercommit(m_commitIndex);
                    request.set_senderid(m_config.m_id);
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
                            spdlog::error(
                                "Stopping heartbeat thread due to too much failed AppendEntries RPC attempts");
                            return;
                        }
                    }

                    consecutiveFailures = 0;

                    auto responseTerm = response.term();
                    auto success = response.success();

                    spdlog::debug("Node={} received AppendEntriesResponse in requester thread peerTerm={} success={} "
                                  "responderId={}",
                                  m_config.m_id,
                                  responseTerm,
                                  success,
                                  response.responderid());

                    {
                        if (token.stop_requested())
                        {
                            return;
                        }

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
        });
}

void consensus_module_t::sendAppendEntriesRPC(raft_node_grpc_client_t &client, std::vector<LogEntry> logEntries)
{
    std::thread(
        [this](raft_node_grpc_client_t &client, std::vector<LogEntry> logEntries)
        {
            AppendEntriesRequest request;
            {
                absl::MutexLock locker{&m_stateMutex};

                request.set_term(m_currentTerm);
                request.set_prevlogterm(logEntries.front().term());
                request.set_prevlogindex(getLogTerm(logEntries.front().index() - 1));
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
                spdlog::error("Node={} failed to send AppendEntries RPC to peer={} at term={}",
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

            if (response.success())
            {
                absl::MutexLock locker{&m_stateMutex};
                m_matchIndex[client.id()] = response.match_index();
                m_nextIndex[client.id()] = response.match_index() + 1;

                uint32_t majorityIndex = findMajorityIndexMatch();
                if (majorityIndex > m_commitIndex && m_log[majorityIndex - 1].term() == m_currentTerm)
                {
                    if (!updatePersistentState(majorityIndex, std::nullopt))
                    {
                        spdlog::error("Node={} is unable to persist commitIndex={}", m_config.m_id, m_commitIndex);
                        return;
                    }

                    // Apply successfull replication to the state machine e.g. in-memory hash-table
                    while (m_lastApplied < m_commitIndex)
                    {
                        ++m_lastApplied;
                        // TODO(lnikon): Update the state machine
                        // m_kv[m_log[m_lastApplied - 1].key()] = m_log[m_lastApplied - 1].value();
                    }

                    return;
                }
            }

            if (!response.success())
            {
                {
                    absl::MutexLock locker{&m_stateMutex};
                    m_nextIndex[client.id()] = std::max(1U, m_nextIndex[client.id()] - 1);
                }
                sendAppendEntriesRPC(client, {});
            }
        },
        std::ref(client),
        logEntries)
        .detach();
}

auto consensus_module_t::getLogTerm(uint32_t index) const -> uint32_t
{
    if (index == 0 || index > m_log.size())
    {
        return 0;
    }

    return m_log[index - 1].term();
}

auto consensus_module_t::findMajorityIndexMatch() -> uint32_t
{
    std::vector<int> matchIndexes;
    matchIndexes.resize(m_replicas.size() + 1);
    for (const auto &[peer, matchIdx] : m_matchIndex)
    {
        matchIndexes.emplace_back(matchIdx);
    }
    matchIndexes.emplace_back(m_log.back().index());

    std::ranges::sort(matchIndexes);

    return matchIndexes[matchIndexes.size() / 2];
}

auto consensus_module_t::waitForMajorityReplication(uint32_t logIndex) -> bool
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

    spdlog::info("Node={} is waiting for majority to agree on logIndex={}", m_config.m_id, logIndex);
    return m_stateMutex.AwaitWithTimeout(absl::Condition(&hasMajority), absl::Seconds(replicationTimeout));
}

auto consensus_module_t::getLastLogIndex() const -> uint32_t
{
    return m_log.empty() ? 0 : m_log.back().index();
}

auto consensus_module_t::getLastLogTerm() const -> uint32_t
{
    return m_log.empty() ? 0 : m_log.back().term();
}

auto consensus_module_t::getState() -> NodeState
{
    return m_state;
}

auto consensus_module_t::updatePersistentState(std::optional<std::uint32_t> commitIndex,
                                               std::optional<std::uint32_t> votedFor) -> bool
{
    m_commitIndex = commitIndex.has_value() ? commitIndex.value() : m_commitIndex;
    m_votedFor = votedFor.has_value() ? votedFor.value() : m_votedFor;
    /*return true;*/
    return flushPersistentState();
}

auto consensus_module_t::flushPersistentState() -> bool
{
    // Flush commitIndex and votedFor
    {
        auto          path = std::filesystem::path(constructFilename(gRaftFilename, m_config.m_id));
        std::ofstream fsa(path, std::fstream::out | std::fstream::trunc);
        if (!fsa.is_open())
        {
            spdlog::error("Node={} is unable to open {} to flush commitIndex={} and votedFor={}",
                          m_config.m_id,
                          path.c_str(),
                          m_commitIndex,
                          m_votedFor);
            return false;
        }

        fsa << m_commitIndex << " " << m_votedFor << "\n";
        if (fsa.fail())
        {
            spdlog::error("Node={} is unable to write into {} the commitIndex={} and votedFor={}",
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
        // TODO(lnikon): ::fsync(fsa->handle());
    }

    // Flush the log
    {
        auto          path = std::filesystem::path(constructFilename(gLogFilename, m_config.m_id));
        std::ofstream fsa(path, std::fstream::out | std::fstream::trunc);
        if (!fsa.is_open())
        {
            spdlog::error("Node={} is unable to open {} to flush the log", m_config.m_id, path.c_str());
            return false;
        }

        for (const auto &entry : m_log)
        {
            fsa << entry.key() << " " << entry.value() << " " << entry.term() << "\n";

            if (fsa.fail())
            {
                spdlog::error("Node={} is unable to write into {} the commitIndex={} and votedFor={}",
                              m_config.m_id,
                              path.c_str(),
                              m_commitIndex,
                              m_votedFor);
                return false;
            }
        }
        fsa.flush();
        // TODO(lnikon): ::fsync(fsa->handle());
    }

    return true;
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
                "Node={} is unable to open {} to restore commitIndex and votedFor", m_config.m_id, path.c_str());
            return false;
        }

        ifs >> m_commitIndex >> m_votedFor;
        m_votedFor = 0;
        spdlog::info("Node={} restored commitIndex={} and votedFor={}", m_config.m_id, m_commitIndex, m_votedFor);
    }

    {
        auto          path = std::filesystem::path(constructFilename(gLogFilename, m_config.m_id));
        std::ifstream ifs(path, std::ifstream::in);
        if (!ifs.is_open())
        {
            spdlog::error("Node={} is unable to open {} to restore log", m_config.m_id, path.c_str());
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

            spdlog::info("Node={} restored logEntry=[key={}, value={}, term={}]", m_config.m_id, key, value, term);
        }
    }

    return true;
}

} // namespace raft

#include "raft.h"

#include <cstdlib>
#include <memory>
#include <ranges>
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

#include <spdlog/spdlog.h>

NodeClient::NodeClient(const ID id, const IP ip)
    : m_id{id},
      m_ip{ip},
      m_channel(grpc::CreateChannel(m_ip, grpc::InsecureChannelCredentials())),
      m_stub(RaftService::NewStub(m_channel))
{
    assert(m_id > 0);
}

auto NodeClient::appendEntries(const AppendEntriesRequest &request, AppendEntriesResponse *response) -> bool
{
    grpc::ClientContext context;

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
    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(generateRandomTimeout()));
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

auto NodeClient::getId() const -> ID
{
    return m_id;
}

ConsensusModule::ConsensusModule(const ID id, std::vector<IP> replicas)
    : m_id{id},
      m_currentTerm{0},
      m_votedFor{0},
      m_state{NodeState::FOLLOWER}
{
    assert(m_id > 0);
    assert(replicas.size() > 0);
    assert(m_id <= replicas.size());

    m_ip = replicas[m_id - 1];

    grpc::ServerBuilder builder;
    builder.AddListeningPort(m_ip, grpc::InsecureServerCredentials());
    builder.RegisterService(this);

    m_server = builder.BuildAndStart();

    for (auto [id, ip] : std::ranges::views::enumerate(replicas))
    {
        if (id + 1 == m_id)
        {
            continue;
        }

        m_replicas.emplace(id + 1, NodeClient(id + 1, ip));
    }

    m_nextIndex.resize(m_replicas.size());
    m_matchIndex.resize(m_replicas.size());
}

auto ConsensusModule::AppendEntries(grpc::ServerContext        *pContext,
                                    const AppendEntriesRequest *pRequest,
                                    AppendEntriesResponse      *pResponse) -> grpc::Status
{
    (void)pContext;
    (void)pRequest;
    (void)pResponse;

    spdlog::info("Recevied AppendEntries RPC from leader={} during term={}", pRequest->senderid(), pRequest->term());

    absl::MutexLock locker(&m_stateMutex);
    absl::MutexLock timerLocker(&m_timerMutex);

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

    pResponse->set_term(m_currentTerm);
    pResponse->set_success(true);
    pResponse->set_responderid(m_id);

    spdlog::info("Node={} is resetting election timeout at term={}", m_id, m_currentTerm);
    resetElectionTimer();

    m_leaderHeartbeatReceived.store(true);

    return grpc::Status::OK;
}

auto ConsensusModule::RequestVote(grpc::ServerContext      *pContext,
                                  const RequestVoteRequest *pRequest,
                                  RequestVoteResponse      *pResponse) -> grpc::Status
{
    (void)pContext;

    spdlog::info(
        "Received RequestVote RPC from candidate={} during term={}", pRequest->candidateid(), pRequest->term());

    absl::WriterMutexLock locker(&m_stateMutex);
    if (pRequest->term() > m_currentTerm)
    {
        becomeFollower(pRequest->term());
    }

    if (pRequest->term() < m_currentTerm)
    {
        pResponse->set_term(m_currentTerm);
        pResponse->set_votegranted(0);
        pResponse->set_responderid(m_id);

        return grpc::Status::OK;
    }

    if (m_votedFor == 0 || m_votedFor == pRequest->candidateid())
    {
        m_votedFor = pRequest->candidateid();
        m_currentTerm = pRequest->term();

        pResponse->set_term(m_currentTerm);
        pResponse->set_votegranted(1);
        pResponse->set_responderid(m_id);

        return grpc::Status::OK;
    }

    /*if (pRequest->lastlogterm() < getLastLogTerm() ||*/
    /*    (pRequest->lastlogterm() == getLastLogTerm() && pRequest->lastlogindex() < getLastLogIndex()))*/
    /*{*/
    /*    pResponse->set_votegranted(0);*/
    /*    return grpc::Status::OK;*/
    /*}*/
    /**/
    return grpc::Status::OK;
}

auto ConsensusModule::init() -> bool
{
    if (!initializePersistentState())
    {
        spdlog::warn("Unable to initialize persistent state!");
        return false;
    }

    if (!initializeVolatileState())
    {
        spdlog::warn("Unable to initialize volatile state!");
        return false;
    }

    return true;
}

void ConsensusModule::start()
{
    m_electionTimerThread = std::jthread(
        [this](std::stop_token token)
        {
            while (!m_stopElectionTimer)
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
                absl::WriterMutexLock locker(&m_timerMutex);
                int64_t               timeToWaitMs = generateRandomTimeout();
                int64_t               timeToWaitDeadlineMs = currentTimeMs() + timeToWaitMs;

                while (!m_leaderHeartbeatReceived.load() && timeToWaitMs > 0)
                {
                    spdlog::info("Timer thread at node={} will block for {}ms for the leader to send a heartbeat",
                                 m_id,
                                 timeToWaitMs);

                    m_timerCV.WaitWithTimeout(&m_timerMutex, absl::Milliseconds(m_electionTimeout.load()));
                    timeToWaitMs = timeToWaitDeadlineMs - currentTimeMs();
                }

                // If timer CV gets signaled, then node has received the heartbeat from the leader.
                // Otherwise, heartbeat timed out and node needs to start the new leader election
                if (m_leaderHeartbeatReceived.load())
                {
                    {
                        spdlog::info("Node={} received heartbeat", m_id);
                    }

                    m_leaderHeartbeatReceived.store(false);
                    continue;
                }
                else if (timeToWaitMs <= 0)
                {
                    startElection();
                }
            }
        });

    {
        assert(m_server);
        spdlog::info("Listening for RPC requests on ");
        m_server->Wait();
    }
}

void ConsensusModule::startServer()
{
    m_server->Wait();
}

void ConsensusModule::stop()
{
    absl::WriterMutexLock locker{&m_stateMutex};

    m_stopElectionTimer = false;
    m_timerCV.SignalAll();

    m_electionTimerThread.request_stop();
    m_electionTimerThread.join();

    for (auto &heartbeatThread : m_heartbeatThreads)
    {
        heartbeatThread.request_stop();
        heartbeatThread.join();
    }
    m_heartbeatThreads.clear();

    m_serverThread.request_stop();
    m_serverThread.join();
}

auto ConsensusModule::initializePersistentState() -> bool
{
    // TODO: Init m_currentTerm, m_votedFor, m_log from disk. Setting dummy values for now.
    m_commitIndex = 0;
    m_lastApplied = 0;

    return true;
}

auto ConsensusModule::initializeVolatileState() -> bool
{
    m_commitIndex = 0;
    m_lastApplied = 0;

    return true;
}

void ConsensusModule::resetElectionTimer()
{
    m_electionTimeout.store(generateRandomTimeout());
    m_electionTimeoutResetTime = std::chrono::high_resolution_clock::now();
    m_timerCV.Signal();
}

void ConsensusModule::startElection()
{
    RequestVoteRequest request;
    {
        absl::WriterMutexLock locker(&m_stateMutex);
        m_currentTerm++;
        m_state = NodeState::CANDIDATE;

        spdlog::info("Node={} starts election. New term={}", m_id, m_currentTerm);

        // Node in a canditate state should vote for itself.
        m_voteCount++;
        m_votedFor = m_id;

        m_electionInProgress = true;

        request.set_term(m_currentTerm);
        request.set_candidateid(m_id);
        request.set_lastlogterm(getLastLogTerm());
        request.set_lastlogindex(getLastLogIndex());
    }

    std::vector<std::jthread> requesterThreads;
    // TODO(lnikon): Is it possible to broadcast unary RPC or consider async?
    for (auto &[id, client] : m_replicas)
    {
        RequestVoteResponse response;
        if (!client.requestVote(request, &response))
        {
            spdlog::error("RequestVote RPC failed in requester thread");
            return;
        }

        auto responseTerm = response.term();
        auto voteGranted = response.votegranted();

        spdlog::info("Received RequestVoteResponse in requester thread peerTerm={} voteGranted={} responseTerm={}",
                     responseTerm,
                     voteGranted,
                     response.responderid());

        absl::MutexLock locker(&m_stateMutex);
        if (responseTerm > m_currentTerm)
        {
            becomeFollower(responseTerm);
            return;
        }

        if ((voteGranted != 0) && responseTerm == m_currentTerm)
        {
            m_voteCount++;
            if (hasMajority(m_voteCount.load()))
            {
                becomeLeader();
            }
        }
    }

    for (auto &thread : requesterThreads)
    {
        thread.join();
    }
}

void ConsensusModule::becomeFollower(const uint32_t newTerm)
{
    m_currentTerm = newTerm;
    m_state = NodeState::FOLLOWER;
    m_votedFor = 0;

    m_electionInProgress = false;

    for (auto &heartbeatThread : m_heartbeatThreads)
    {
        heartbeatThread.request_stop();
        heartbeatThread.join();
    }
    m_heartbeatThreads.clear();

    resetElectionTimer();

    spdlog::info("Server reverted to FOLLOWER state in term={}", m_currentTerm);
}

auto ConsensusModule::hasMajority(const uint32_t votes) const -> bool
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
    m_electionInProgress = false;

    spdlog::info("Node={} become a leader at term={}", m_id, m_currentTerm);

    for (auto &[id, client] : m_replicas)
    {
        sendHeartbeat(client);
    }
}

void ConsensusModule::sendHeartbeat(NodeClient &client)
{
    constexpr const auto heartbeatInterval{std::chrono::milliseconds(100)};

    m_heartbeatThreads.emplace_back(
        [this, &client, heartbeatInterval](std::stop_token token)
        {
            spdlog::info("Node={} is starting a heartbeat thread for client={}", m_id, client.getId());
            while (!token.stop_requested())
            {
                AppendEntriesRequest request;
                {
                    absl::ReaderMutexLock locker(&m_stateMutex);

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
                        spdlog::error("AppendEntriesRequest failed during heartbeat");
                        continue;
                    }

                    auto responseTerm = response.term();
                    auto success = response.success();

                    spdlog::info(
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

                        if (!success)
                        {
                            decrementNextIndex(client.getId());
                        }
                    }
                }

                std::this_thread::sleep_for(heartbeatInterval);
            }

            spdlog::info("Stopping heartbeat thread for on the node={} for the client={}", m_id, client.getId());
        });
}

void ConsensusModule::decrementNextIndex(ID id)
{
    (void)id;
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
};

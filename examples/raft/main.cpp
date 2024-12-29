#include <absl/synchronization/mutex.h>
#include <absl/time/time.h>
#include <cxxopts.hpp>
#include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>

#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/support/status.h>
#include <cstdlib>
#include <chrono>
#include <memory>
#include <ranges>
#include <thread>
#include <utility>
#include <vector>
#include <random>

#include <spdlog/spdlog.h>

#include "Raft.grpc.pb.h"
#include "Raft.pb.h"

namespace
{

auto generateRandomTimeout() -> int
{
    const int minTimeout{150};
    const int maxTimeout{300};

    std::random_device              randomDevice;
    std::mt19937                    gen(randomDevice());
    std::uniform_int_distribution<> dist(minTimeout, maxTimeout);

    return dist(gen);
}

} // namespace

using IP = std::string;
using ID = uint32_t;

// Valid IDs start from 1
constexpr const ID invalidId = 0;

class NodeClient
{
  public:
    NodeClient(const ID id, const IP ip)
        : m_id{id},
          m_ip{ip},
          m_channel(grpc::CreateChannel(m_ip, grpc::InsecureChannelCredentials())),
          m_stub(RaftService::NewStub(m_channel))
    {
        assert(m_id > 0);
    }

    auto appendEntries(const AppendEntriesRequest &request, AppendEntriesResponse *response) -> bool
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

    auto requestVote(const RequestVoteRequest &request, RequestVoteResponse *response) -> bool
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

    [[nodiscard]] auto getId() const -> ID
    {
        return m_id;
    }

  private:
    ID m_id{invalidId};
    IP m_ip;

    std::shared_ptr<grpc::ChannelInterface> m_channel{nullptr};
    std::unique_ptr<RaftService::Stub>      m_stub{nullptr};
    /*grpc::CompletionQueue                   m_cq;*/
};

class ConsensusModule : public RaftService::Service, std::enable_shared_from_this<ConsensusModule>
{
  public:
    // @id is the ID of the current node. Order of RaftServices in @replicas is important!
    ConsensusModule(const ID id, std::vector<IP> replicas)
        : m_id{id},
          m_currentTerm{0}
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

    auto AppendEntries(grpc::ServerContext        *pContext,
                       const AppendEntriesRequest *pRequest,
                       AppendEntriesResponse      *pResponse) -> grpc::Status override
    {
        (void)pContext;
        (void)pRequest;
        (void)pResponse;

        spdlog::info(
            "Recevied AppendEntries RPC from leader={} during term={}", pRequest->senderid(), pRequest->term());

        absl::MutexLock timerLocker(&m_timerMutex);
        absl::MutexLock locker(&m_electionMutex);

        pResponse->set_term(m_currentTerm);
        pResponse->set_success(true);
        pResponse->set_responderid(m_id);

        spdlog::info("Node={} is resetting election timeout at term={}", m_id, m_currentTerm);
        resetElectionTimer();

        m_leaderHeartbeatReceived.store(true);

        return grpc::Status::OK;
    }

    auto RequestVote(grpc::ServerContext *pContext, const RequestVoteRequest *pRequest, RequestVoteResponse *pResponse)
        -> grpc::Status override
    {
        (void)pContext;

        spdlog::info(
            "Received RequestVote RPC from candidate={} during term={}", pRequest->candidateid(), pRequest->term());

        absl::WriterMutexLock locker(&m_electionMutex);
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

        return grpc::Status::OK;
    }

    auto init() -> bool
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

    void start()
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
                        absl::MutexLock locker(&m_electionMutex);
                        if (m_state == NodeState::LEADER)
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

    void startServer()
    {
        m_server->Wait();
    }

    void stop()
    {
        m_stopElectionTimer = false;
        m_timerCV.SignalAll();
        m_electionTimerThread.request_stop();
        m_electionTimerThread.join();

        m_serverThread.request_stop();
        m_serverThread.join();
    }

  private:
    // State initialization
    auto initializePersistentState() -> bool
    {
        // TODO: Init m_currentTerm, m_votedFor, m_log from disk. Setting dummy values for now.
        m_commitIndex = 0;
        m_lastApplied = 0;

        return true;
    }

    auto initializeVolatileState() -> bool
    {
        m_commitIndex = 0;
        m_lastApplied = 0;

        return true;
    }

    // Timer handling
    // Called every time 'AppendEntries' received.
    void resetElectionTimer()
    {
        m_electionTimeout.store(generateRandomTimeout());
        m_electionTimeoutResetTime = std::chrono::high_resolution_clock::now();
        m_timerCV.Signal();
    }

    // The logic behind election
    void startElection()
    {
        RequestVoteRequest request;
        {
            absl::WriterMutexLock locker(&m_electionMutex);
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

            absl::MutexLock locker(&m_electionMutex);
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

    void becomeFollower(const uint32_t newTerm)
    {
        m_electionMutex.AssertHeld();

        m_currentTerm = newTerm;
        m_state = NodeState::FOLLOWER;
        m_votedFor = 0;

        m_electionInProgress = false;

        resetElectionTimer();

        spdlog::info("Server reverted to FOLLOWER state in term={}", m_currentTerm);
    }

    auto hasMajority(const uint32_t votes) const -> bool
    {
        return votes > static_cast<double>(m_replicas.size()) / 2.0;
    }

    void becomeLeader()
    {
        m_electionMutex.AssertHeld();

        m_state = NodeState::LEADER;
        m_electionInProgress = false;

        spdlog::info("Node={} become a leader at term={}", m_id, m_currentTerm);

        for (auto &[id, client] : m_replicas)
        {
            sendHeartbeat(client);
        }
    }

    void sendHeartbeat(NodeClient &client)
    {
        constexpr const auto heartbeatInterval{std::chrono::milliseconds(100)};
        std::thread(
            [this, &client, heartbeatInterval]()
            {
                while (m_state == NodeState::LEADER)
                {
                    AppendEntriesRequest request;
                    {
                        absl::ReaderMutexLock locker(&m_electionMutex);

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
                            return;
                        }

                        auto responseTerm = response.term();
                        auto success = response.success();

                        spdlog::info(
                            "Received AppendEntriesResponse in requester thread peerTerm={} success={} responderId={}",
                            responseTerm,
                            success,
                            response.responderid());

                        {
                            absl::WriterMutexLock locker(&m_electionMutex);

                            if (responseTerm > m_currentTerm)
                            {
                                becomeFollower(responseTerm);
                                return;
                            }

                            if (!success)
                            {
                                decrementNextIndex(client.getId());
                            }
                        }
                    }

                    std::this_thread::sleep_for(heartbeatInterval);
                }
            })
            .detach();
    }

    void decrementNextIndex(ID id)
    {
        (void)id;
    }

    [[nodiscard]] auto getLastLogIndex() const -> uint32_t
    {
        return m_log.empty() ? 0 : m_log.back().index();
    }

    [[nodiscard]] auto getLastLogTerm() const -> uint32_t
    {
        return m_log.empty() ? 0 : m_log.back().term();
    }

    void appendEntriesRPC()
    {
    }

    // Id of the current node. Received from outside.
    ID m_id{invalidId};

    // IP of the current node. Received from outside.
    IP m_ip;

    // gRPC server for the current node
    std::unique_ptr<grpc::Server> m_server;

    // Each server starts as a follower.
    NodeState m_state{NodeState::FOLLOWER};

    absl::Mutex m_electionMutex;

    // Persistent state on all servers
    uint32_t m_currentTerm ABSL_GUARDED_BY(m_electionMutex);
    uint32_t               m_votedFor{0};
    std::vector<LogEntry>  m_log;

    // Volatile state on all servers. Reseted on each server start.
    uint32_t m_commitIndex{0};
    uint32_t m_lastApplied{0};

    // Volatile state on leaders
    std::unordered_map<ID, NodeClient> m_replicas;
    std::vector<uint32_t>              m_nextIndex;
    std::vector<uint32_t>              m_matchIndex;

    // Election and election timer related fields.
    std::atomic<bool>                              m_leaderHeartbeatReceived{false};
    absl::Mutex                                    m_timerMutex;
    absl::CondVar                                  m_timerCV;
    std::atomic<bool>                              m_stopElectionTimer{false};
    std::atomic<int>                               m_electionTimeout{generateRandomTimeout()};
    std::chrono::high_resolution_clock::time_point m_electionTimeoutResetTime{
        std::chrono::high_resolution_clock::now()};
    std::jthread          m_electionTimerThread;
    std::atomic<uint32_t> m_voteCount{0};
    std::atomic<bool>     m_electionInProgress{false};

    std::jthread m_serverThread;
};

int main(int argc, char *argv[])
{
    cxxopts::Options options("raft");
    options.add_options()("id", "id of the node", cxxopts::value<ID>())(
        "nodes", "ip addresses of replicas in a correct order", cxxopts::value<std::vector<IP>>());

    auto parsedOptions = options.parse(argc, argv);
    if ((parsedOptions.count("help") != 0U) || (parsedOptions.count("id") == 0U) ||
        (parsedOptions.count("nodes") == 0U))
    {
        spdlog::info("{}", options.help());
        return EXIT_SUCCESS;
    }

    auto nodeId = parsedOptions["id"].as<ID>();
    auto nodeIps = parsedOptions["nodes"].as<std::vector<IP>>();
    for (auto ip : nodeIps)
    {
        spdlog::info(ip);
    }

    spdlog::info("nodeIps.size()={}", nodeIps.size());

    if (nodeId == 0)
    {
        spdlog::error("ID of the node should be positve integer");
        return EXIT_FAILURE;
    }

    if (nodeIps.empty())
    {
        spdlog::error("List of node IPs can't be empty");
        return EXIT_FAILURE;
    }

    ConsensusModule cm(nodeId, nodeIps);
    if (!cm.init())
    {
        spdlog::error("Failed to initialize the state machine");
        return EXIT_FAILURE;
    }

    cm.start();

    cm.stop();

    return EXIT_SUCCESS;
}

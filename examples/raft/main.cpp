#include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>

#include <iterator>
#include <cstdlib>
#include <chrono>
#include <memory>
#include <mutex>
#include <ranges>
#include <stdexcept>
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

        grpc::Status status = m_stub->RequestVote(&context, request, response);
        if (!status.ok())
        {
            spdlog::error("RequestVote RPC call failed");
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

class ConsensusModule : public RaftService::Service
{
  public:
    // @id is the ID of the current node. Order of RaftServices in @replicas is important!
    ConsensusModule(const ID id, std::vector<IP> replicas)
        : m_id{id}
    {
        assert(m_id > 0);
        assert(m_replicas.size() > 0);

        for (auto [id, ip] : std::ranges::views::enumerate(replicas))
        {
            m_replicas.emplace(id, NodeClient(id, ip));
        }

        m_nextIndex.resize(m_replicas.size());
        m_matchIndex.resize(m_replicas.size());
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
        m_electionTimerThread = std::jthread(&ConsensusModule::electionTimeoutTask, this);
    }

    void stop()
    {
        m_stopElectionTimer = false;
        m_timerCV.notify_all();
        m_electionTimerThread.request_stop();
        m_electionTimerThread.join();
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

    // Request handling
    void handleAppendEntriesRPC()
    {
        // TODO(vbejanyan): Staff to do

        resetElectionTimer();

        // TODO(vbejanyan): Work continues
    }

    // Timer handling
    // Called every time 'AppendEntries' received.
    void resetElectionTimer()
    {
        std::lock_guard lock(m_timerMutex);
        m_electionTimeout = generateRandomTimeout();
        m_timerCV.notify_all();
    }

    // The logic behind election
    void startElection()
    {
        std::lock_guard locker(m_electionMutex);
        auto            previousTerm = m_currentTerm++;
        auto            previousState = m_state;
        m_state = NodeState::CANDIDATE;

        // Node in a canditate state should vote for itself.
        m_voteCount++;
        m_votedFor = m_id;

        m_electionInProgress = true;

        RequestVoteRequest request;
        request.set_term(m_currentTerm);
        request.set_candidateid(m_id);
        request.set_lastlogterm(getLastLogTerm());
        request.set_lastlogindex(getLastLogIndex());

        std::vector<std::jthread> requesterThreads;
        for (auto &[id, client] : m_replicas)
        {

            requesterThreads
                .emplace_back(
                    [request, this](NodeClient &client)
                    {
                        RequestVoteResponse response;
                        if (!client.requestVote(request, &response))
                        {
                            spdlog::error("RequestVote RPC failed in requester thread");
                        }

                        auto responseTerm = response.term();
                        auto voteGranted = response.votegranted();

                        spdlog::info("Received RequestVoteResponse in requester thread peerTerm={} voteGranted={}",
                                     responseTerm,
                                     voteGranted);

                        std::lock_guard locker(m_electionMutex);
                        if (responseTerm > m_currentTerm)
                        {
                            becomeFollower(responseTerm);
                            return;
                        }

                        if (voteGranted && responseTerm == m_currentTerm)
                        {
                            m_voteCount++;
                            if (hasMajority(m_voteCount.load()))
                            {
                                becomeLeader();
                            }
                        }
                    },
                    std::ref(client))
                .detach();
        }
    }

    void becomeFollower(const uint32_t newTerm)
    {
        std::lock_guard locker(m_electionMutex);

        m_currentTerm = newTerm;
        m_state = NodeState::FOLLOWER;
        m_votedFor = 0;

        m_electionInProgress = false;

        resetElectionTimer();

        spdlog::info("Server reverted to FOLLOWER state in term={}", m_currentTerm);
    }

    // A task to monitor the election timeout and start a new election if needed.
    void electionTimeoutTask(std::stop_token token)
    {
        while (!m_stopElectionTimer)
        {
            if (token.stop_requested())
            {
                return;
            }

            std::unique_lock lock(m_timerMutex);
            if (m_timerCV.wait_for(lock,
                                   std::chrono::milliseconds(m_electionTimeout),
                                   [this]() { return m_leaderHeartbeatReceived.load(); }))
            {
                m_leaderHeartbeatReceived.store(false);
            }
            else
            {
                startElection();
            }
        }
    }

    auto hasMajority(const uint32_t votes) const -> bool
    {
        return votes > m_replicas.size() / 2;
    }

    void becomeLeader()
    {
        m_state = NodeState::LEADER;
        m_electionInProgress = false;

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
                        std::lock_guard locker(m_electionMutex);

                        request.set_term(m_currentTerm);
                        request.set_prevlogterm(getLastLogTerm());
                        request.set_prevlogindex(getLastLogIndex());
                        request.set_leadercommit(m_commitIndex);
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

                        spdlog::info("Received RequestVoteResponse in requester thread peerTerm={} success={}",
                                     responseTerm,
                                     success);

                        {
                            std::lock_guard locker(m_electionMutex);

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
    }

    [[nodiscard]] auto getLastLogIndex() const -> uint32_t
    {
        return m_log.empty() ? 0 : m_log.back().index();
    }

    [[nodiscard]] auto getLastLogTerm() const -> uint32_t
    {
        return m_log.empty() ? 0 : m_log.back().term();
    }

    void revertToFollower(uint32_t newTerm)
    {
    }

    void appendEntriesRPC()
    {
    }

    // Id of the current node. Received from outside.
    uint32_t m_id{invalidId};

    // Each server starts as a follower.
    NodeState m_state{NodeState::FOLLOWER};

    // Persistent state on all servers
    uint32_t              m_currentTerm{0};
    uint32_t              m_votedFor{0};
    std::vector<LogEntry> m_log{};

    // Volatile state on all servers. Reseted on each server start.
    uint32_t m_commitIndex{0};
    uint32_t m_lastApplied{0};

    // Volatile state on leaders
    std::unordered_map<ID, NodeClient> m_replicas;
    std::vector<uint32_t>              m_nextIndex;
    std::vector<uint32_t>              m_matchIndex;

    // Election and election timer related fields.
    std::atomic<bool>       m_leaderHeartbeatReceived{false};
    std::mutex              m_timerMutex;
    std::condition_variable m_timerCV;
    std::atomic<bool>       m_stopElectionTimer{false};
    int                     m_electionTimeout{0};
    std::jthread            m_electionTimerThread;
    std::mutex              m_electionMutex;
    std::atomic<uint32_t>   m_voteCount{0};
    std::atomic<bool>       m_electionInProgress{false};
};

int main()
{
    ConsensusModule cm;
    if (!cm.init())
    {
        spdlog::error("Failed to initialize the state machine");
        return EXIT_FAILURE;
    }

    cm.start();

    cm.stop();

    return EXIT_SUCCESS;
}

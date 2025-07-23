#include <absl/synchronization/mutex.h>
#include <spdlog/spdlog.h>

#include "db.h"
#include "db/manifest/manifest.h"
#include "memtable.h"
#include "raft/raft.h"

namespace db
{

db_t::db_t(
    config::shared_ptr_t                      config,
    manifest::shared_ptr_t                    pManifest,
    lsmtree_ptr_t                             pLsmtree,
    std::shared_ptr<raft::consensus_module_t> pConsensusModule
) noexcept
    : m_config{std::move(config)},
      m_pManifest{std::move(pManifest)},
      m_pLSMtree{std::move(pLsmtree)},
      m_pConsensusModule{std::move(pConsensusModule)}
{
    assert(m_config);
    assert(m_pManifest);
    assert(m_pLSMtree);
}

db_t::db_t(db_t &&other) noexcept
    : m_config{std::move(other.m_config)},
      m_pManifest{std::move(other.m_pManifest)},
      m_pLSMtree{std::move(other.m_pLSMtree)},
      m_pConsensusModule{std::move(other.m_pConsensusModule)}
{
}

auto db_t::operator=(db_t &&other) noexcept -> db_t &
{
    if (this != &other)
    {
        db_t temp{std::move(other)};
        swap(temp);
    }
    return *this;
}

auto db_t::start() -> bool
{
    spdlog::info("Starting the database");

    if (!open())
    {
        spdlog::error("Unable to open the database");
        return false;
    }

    m_requestProcessor = std::thread([this] { processRequests(); });
    m_pendingMonitor = std::thread([this] { monitorPendingRequests(); });

    m_isLeader.store(m_pConsensusModule->isLeader());

    spdlog::info("Database successfully started");
}

void db_t::stop()
{
    spdlog::info("Shutting down the database");

    m_shutdown.store(true);

    // Stop accepting new requests
    m_requestQueue.shutdown();

    // Stop thread pool
    m_requestPool.shutdown();

    // Join threads
    if (m_requestProcessor.joinable())
    {
        m_requestProcessor.join();
    }

    if (m_pendingMonitor.joinable())
    {
        m_pendingMonitor.join();
    }

    // Complete pending requests
    {
        absl::WriterMutexLock locker{&m_pendingMutex};
        for (auto &[id, request] : m_pendingRequests)
        {
            request.promise.set_value(false);
        }
        m_pendingRequests.clear();
    }

    spdlog::info("Database stopped");
}

auto db_t::open() -> bool
{
    return prepare_directory_structure();
}

[[nodiscard]] auto db_t::put(const PutRequest *pRequest, PutResponse *pResponse) noexcept
    -> db_op_result_t
{
    if (m_shutdown.load())
    {
        spdlog::warn("Database is shutting down. Can not accept new request.");
        return {.status = db_op_status_k::failure_k, .message = {}, .leaderAddress = {}};
    }

    if (!m_isLeader.load())
    {
        return forwardToLeader();
    }

    client_request_t request{
        .type = client_request_type_k::put_k,
        .key = {pRequest->key().data(), pRequest->key().size()},
        .value = {pRequest->value().data(), pRequest->value().size()},
        .promise = {},
        .requestId = m_requestIdCounter.fetch_add(1)
    };

    auto future = request.promise.get_future();

    // Enqueue request
    if (!m_requestQueue.push(std::move(request)))
    {
        return {
            .status = db_op_status_k::request_queue_full_k,
            .message = std::string{"Request queue full"},
            .leaderAddress = {},
        };
    }

    if (future.wait_for(m_config->DatabaseConfig.requestTimeout) == std::future_status::ready)
    {
        if (!future.get())
        {
            return {
                .status = db_op_status_k::failed_to_replicate_k,
                .message = std::string{"Failed to replicate"},
                .leaderAddress = {}
            };
        }
    }
    else
    {
        return {
            .status = db_op_status_k::request_timeout_k,
            .message = std::string{"Request timeout"},
            .leaderAddress = {}
        };
    }

    return {
        .status = db_op_status_k::success_k,
        .message = std::string{"Request success"},
        .leaderAddress = {}
    };
}

auto db_t::get(const key_t &key) -> std::optional<record_t>
{
    // TODO(lnikon): Should lock a ReadMutex here!
    return m_pLSMtree->get(key);
}

auto db_t::config() const noexcept -> config::shared_ptr_t
{
    return m_config;
}

auto db_t::prepare_directory_structure() -> bool
{
    // Create database directory
    if (!std::filesystem::exists(m_config->DatabaseConfig.DatabasePath))
    {
        spdlog::info(
            "Creating database directory at {}", m_config->DatabaseConfig.DatabasePath.c_str()
        );
        if (!std::filesystem::create_directory(m_config->DatabaseConfig.DatabasePath))
        {
            spdlog::error(
                "Failed to create database directory at {}",
                m_config->DatabaseConfig.DatabasePath.c_str()
            );
            return false;
        }
    }
    else
    {
        spdlog::info("Opening database at {}", m_config->DatabaseConfig.DatabasePath.c_str());
    }

    // Create segments directory inside database directory
    const auto &segmentsPath{m_config->datadir_path()};
    if (!std::filesystem::exists(segmentsPath))
    {
        spdlog::info("Creating segments directory at {}", segmentsPath.c_str());
        if (!std::filesystem::create_directory(segmentsPath))
        {
            spdlog::error("Failed to create segments directory at {}", segmentsPath.c_str());
            return false;
        }
    }

    return true;
}

void db_t::swap(db_t &other) noexcept
{
    using std::swap;

    swap(m_config, other.m_config);
    swap(m_pManifest, other.m_pManifest);
    swap(m_pLSMtree, other.m_pLSMtree);
    swap(m_pConsensusModule, other.m_pConsensusModule);
}

void db_t::processRequests()
{
}

void db_t::handleClientRequest(client_request_t request)
{
}

void db_t::monitorPendingRequests()
{
}

auto db_t::onRaftCommit(const LogEntry &entry) -> bool
{
}

void db_t::onLeaderChange(bool isLeader)
{
}

auto db_t::serializeOperation(const client_request_t &request) -> std::string
{
}

auto db_t::deserializeAndApply(const std::string &payload)
{
}

auto db_t::forwardToLeader() -> db_op_result_t
{
    m_requestsForwarded.fetch_add(1);

    auto leaderAddress = getLeaderAddress();
    return leaderAddress.empty()
               ? db_op_result_t{.status = db_op_status_k::leader_not_found_k, .message = "Leader not found", .leaderAddress = {}}
               : db_op_result_t{
                     .status = db_op_status_k::forward_to_leader_k,
                     .message = fmt::format("Not leader. Leader is at {}", leaderAddress),
                     .leaderAddress = std::move(leaderAddress)
                 };
}

auto db_t::getLeaderAddress() -> std::string
{
}

} // namespace db

#include <chrono>
#include <thread>

#include <absl/synchronization/mutex.h>
#include <spdlog/spdlog.h>
#include <magic_enum/magic_enum.hpp>

#include "db.h"
#include "db/manifest/manifest.h"
#include "lsmtree.h"
#include "memtable.h"
#include "raft/raft.h"

namespace
{

auto serializeOperation(const db::client_request_t &request) -> std::string
{
    tinykvpp::v1::DatabaseOperation op;
    op.set_request_id(request.requestId);

    switch (request.type)
    {
    case db::client_request_type_k::put_k:
    {
        op.set_type(tinykvpp::v1::DatabaseOperation::TYPE_PUT);
        op.set_key(request.key);
        op.set_value(request.value);
        break;
    }
    case db::client_request_type_k::delete_k:
    {
        spdlog::critical("DELETE is not implemented");
        break;
    }
    case db::client_request_type_k::batch_k:
    {
        spdlog::critical("BATCH is not implemented");
        break;
    }
    default:
    {
        spdlog::critical("Unknown operation type: {}", magic_enum::enum_name(request.type));
        break;
    }
    }

    return op.SerializeAsString();
}

} // namespace

namespace db
{

db_t::db_t(
    config::shared_ptr_t                           config,
    manifest::shared_ptr_t                         pManifest,
    lsmtree_ptr_t                                  pLsmtree,
    std::shared_ptr<consensus::consensus_module_t> pConsensusModule
) noexcept
    : m_config{std::move(config)},
      m_pManifest{std::move(pManifest)},
      m_pLSMtree{std::move(pLsmtree)},
      m_pConsensusModule{std::move(pConsensusModule)},
      m_requestPool{4, std::string{"db_request_pool"}}
{
    assert(m_config);
    assert(m_pManifest);
    assert(m_pLSMtree);
}

auto db_t::start() -> bool
{
    spdlog::info("Starting the database");

    if (!open())
    {
        spdlog::error("Unable to open the database");
        return false;
    }

    m_requestProcessor = std::thread(
        [this]
        {
            pthread_setname_np(pthread_self(), "requests_processor");
            processRequests();
        }
    );

    m_pendingMonitor = std::thread(
        [this]
        {
            pthread_setname_np(pthread_self(), "pending_requests_monitor");
            monitorPendingRequests();
        }
    );

    m_isLeader.store(m_pConsensusModule->isLeader());

    spdlog::info("Database successfully started");

    return true;
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

[[nodiscard]] auto
db_t::put(const tinykvpp::v1::PutRequest *pRequest, tinykvpp::v1::PutResponse *pResponse) noexcept
    -> db_op_result_t
{
    if (m_shutdown.load())
    {
        spdlog::warn("Database is shutting down. Can not accept new request.");
        pResponse->set_success(false);
        return {.status = db_op_status_k::failure_k, .message = {}, .leaderAddress = {}};
    }

    if (!m_isLeader.load())
    {
        pResponse->set_success(false);
        return forwardToLeader();
    }

    client_request_t request{
        .type = client_request_type_k::put_k,
        .key = {pRequest->key().data(), pRequest->key().size()},
        .value = {pRequest->value().data(), pRequest->value().size()},
        .promise = {},
        .requestId = m_requestIdCounter.fetch_add(1),
        .deadline = std::chrono::steady_clock::now()
    };

    auto future = request.promise.get_future();

    // Enqueue request
    if (!m_requestQueue.push(std::move(request)))
    {
        pResponse->set_success(false);
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
            pResponse->set_success(false);
            return {
                .status = db_op_status_k::failed_to_replicate_k,
                .message = std::string{"Failed to replicate"},
                .leaderAddress = {}
            };
        }
    }
    else
    {
        pResponse->set_success(false);
        return {
            .status = db_op_status_k::request_timeout_k,
            .message = std::string{"Request timeout"},
            .leaderAddress = {}
        };
    }

    pResponse->set_success(true);
    return {
        .status = db_op_status_k::success_k,
        .message = std::string{"Request success"},
        .leaderAddress = {}
    };
}

auto db_t::get(const tinykvpp::v1::GetRequest *pRequest, tinykvpp::v1::GetResponse *pResponse)
    -> db_op_result_t
{
    if (m_shutdown.load())
    {
        spdlog::warn("Database is shutting down. Can not accept new request.");
        return {.status = db_op_status_k::failure_k, .message = {}, .leaderAddress = {}};
    }

    // TODO(lnikon): Linearizable reads
    // if (pRequest->linearizable() && !m_isLeader.load())
    // {
    //   return forwardToLeader();
    // }

    if (auto record =
            m_pLSMtree->get(structures::memtable::memtable_t::record_t::key_t{pRequest->key()});
        record.has_value())
    {
        pResponse->set_found(true);
        pResponse->set_value(record.value().m_value.m_value);
        return {.status = db_op_status_k::success_k, .message = {}, .leaderAddress = {}};
    }

    return {
        .status = db_op_status_k::key_not_found_k, .message = {"Key not found"}, .leaderAddress = {}
    };
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
    while (!m_shutdown.load())
    {
        auto request = m_requestQueue.pop();
        if (!request.has_value())
        {
            continue;
        }

        m_requestPool.enqueue([this, &request]
                              { handleClientRequest(std::move(request.value())); });
    }
}

void db_t::handleClientRequest(client_request_t request)
{
    // Check if still leader
    if (!m_isLeader.load())
    {
        request.promise.set_value(false);
        return;
    }

    // Serialize operation
    std::string  payload = serializeOperation(request);
    request_id_t requestId = request.requestId;
    auto         deadline = request.deadline;

    // Store pending request
    {
        absl::WriterMutexLock locker{&m_pendingMutex};
        m_pendingRequests[request.requestId] = std::move(request);
    }

    // Submit to Raft
    auto raftFuture = m_pConsensusModule->replicateAsync(std::move(payload));

    // Handle Raft result asynchronously
    m_requestPool.enqueue(
        [this, requestId, deadline, raftFuture = std::move(raftFuture)] mutable
        {
            // Check if already timed out
            auto timeout = deadline - std::chrono::steady_clock::now();
            if (timeout <= std::chrono::milliseconds(0))
            {
                absl::WriterMutexLock locker{&m_pendingMutex};
                requestFailed(requestId);
                return;
            }

            // Wait with timeout
            if (raftFuture.wait_for(timeout) == std::future_status::ready)
            {
                // Future completed,
                bool replicated = raftFuture.get();
                // but replication failed
                if (!replicated)
                {
                    // Replication failed - complete request immediately
                    absl::WriterMutexLock locker{&m_pendingMutex};
                    requestFailed(requestId);
                }
                // If replicated=true, wait for commit callback to complete the request
            }
            else
            {
                // Timed out waiting for replication
                absl::WriterMutexLock locker{&m_pendingMutex};
                requestFailed(requestId);
            }
        }
    );
}

void db_t::monitorPendingRequests()
{
    while (!m_shutdown.load())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        std::vector<request_id_t> timedOut;

        {
            absl::ReaderMutexLock locker{&m_pendingMutex};
            auto                  now = std::chrono::steady_clock::now();
            for (const auto &[id, request] : m_pendingRequests)
            {
                if (now > request.deadline)
                {
                    timedOut.push_back(id);
                }
            }
        }

        if (!timedOut.empty())
        {
            absl::ReaderMutexLock locker{&m_pendingMutex};
            for (auto id : timedOut)
            {
                requestFailed(id);
            }
        }
    }
}

auto db_t::onRaftCommit(const raft::v1::LogEntry &entry) -> bool
{
    tinykvpp::v1::DatabaseOperation op;
    op.ParseFromString(entry.payload());

    structures::memtable::memtable_t::record_t record;
    record.m_key.m_key = op.key();
    record.m_value.m_value = op.value();

    switch (op.type())
    {
    case tinykvpp::v1::DatabaseOperation::TYPE_PUT:
    {
        if (m_pLSMtree->put(std::move(record)) ==
            structures::lsmtree::lsmtree_status_k::put_failed_k)
        {
            spdlog::error("Failed to update the lsmtree");
            return false;
        }
        break;
    }
    case tinykvpp::v1::DatabaseOperation::TYPE_DELETE:
    {
        spdlog::critical("DELETE is not implemented");
        return true;
        break;
    }
    case tinykvpp::v1::DatabaseOperation::TYPE_BATCH:
    {
        spdlog::critical("BATCH is not implemented");
        return true;
        break;
    }
    default:
    {
        spdlog::critical("Unknown operation type: {}", magic_enum::enum_name(op.type()));
        return false;
    }
    }

    // Mark request as success and remove from pending requests
    absl::WriterMutexLock locker{&m_pendingMutex};
    requestSuccess(op.request_id());

    return true;
}

void db_t::onLeaderChange(bool isLeader)
{
    m_isLeader.store(isLeader);

    if (!isLeader)
    {
        absl::WriterMutexLock locker{&m_pendingMutex};
        for (auto &[id, request] : m_pendingRequests)
        {
            (void)id;
            request.promise.set_value(false);
        }
        m_pendingRequests.clear();
    }
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
    std::string hint = m_pConsensusModule->getLeaderHint();
    if (!hint.empty())
    {
        absl::WriterMutexLock locker{&m_leaderMutex};
        m_leaderAddress = std::move(hint);
        return m_leaderAddress;
    }

    absl::ReaderMutexLock locker{&m_leaderMutex};
    return m_leaderAddress;
}

void db_t::requestSuccess(request_id_t id)
{
    if (auto it = m_pendingRequests.find(id); it != m_pendingRequests.end())
    {
        it->second.promise.set_value(true);
        m_pendingRequests.erase(it);
        m_requestsProcessed.fetch_add(1);
    }
}

void db_t::requestFailed(request_id_t id)
{
    if (auto it = m_pendingRequests.find(id); it != m_pendingRequests.end())
    {
        it->second.promise.set_value(false);
        m_pendingRequests.erase(it);
        m_requestsFailed.fetch_add(1);
    }
}

} // namespace db

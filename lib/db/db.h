#pragma once

#include <future>
#include <memory>

#include <absl/base/thread_annotations.h>
#include <absl/container/flat_hash_map.h>
#include <grpcpp/server_context.h>

#include "db/manifest/manifest.h"
#include "config/config.h"
#include "structures/lsmtree/lsmtree.h"
#include "raft/raft.h"
#include "concurrency/thread_safe_queue.h"
#include "tinykvpp/v1/tinykvpp_service.pb.h"

namespace db
{

// This enum misses get_k because its processing flow is quite simple
enum class client_request_type_k : int8_t
{
    undefined_k = -1,
    put_k,
    delete_k,
    batch_k,
};

enum class db_op_status_k : int8_t
{
    undefined_k = -1,
    success_k,
    failure_k,
    request_timeout_k,
    failed_to_replicate_k,
    request_queue_full_k,
    key_not_found_k,
    forward_to_leader_k,
    leader_not_found_k,
};

struct db_op_result_t final
{
    db_op_status_k status{db_op_status_k::undefined_k};
    std::string    message;
    std::string    leaderAddress;
};

using request_id_t = uint64_t;
struct client_request_t final
{
    client_request_type_k                 type{client_request_type_k::undefined_k};
    std::string_view                      key;
    std::string_view                      value;
    std::promise<bool>                    promise;
    request_id_t                          requestId;
    std::chrono::steady_clock::time_point deadline;
};

class db_t final
{
  public:
    using record_t = structures::memtable::memtable_t::record_t;
    using key_t = record_t::key_t;
    using value_t = record_t::value_t;
    using lsmtree_ptr_t = std::shared_ptr<structures::lsmtree::lsmtree_t>;

    explicit db_t(
        config::shared_ptr_t                           config,
        manifest::shared_ptr_t                         pManifest,
        lsmtree_ptr_t                                  pLsmtree,
        std::shared_ptr<consensus::consensus_module_t> pConsensusModule
    ) noexcept;

    db_t(db_t &&other) = delete;
    auto operator=(db_t &&other) -> db_t & = delete;

    db_t(const db_t &) = delete;
    auto operator=(const db_t &) -> db_t & = delete;

    ~db_t() noexcept = default;

    [[nodiscard]] auto start() -> bool;
    void               stop();

    [[nodiscard]] auto open() -> bool;

    [[nodiscard]] auto
    put(const tinykvpp::v1::PutRequest *pRequest, tinykvpp::v1::PutResponse *pResponse) noexcept
        -> db_op_result_t;
    [[nodiscard]] auto
    get(const tinykvpp::v1::GetRequest *pRequest, tinykvpp::v1::GetResponse *pResponse)
        -> db_op_result_t;

    [[nodiscard]] auto config() const noexcept -> config::shared_ptr_t;

  private:
    [[nodiscard]] auto prepare_directory_structure() -> bool;

    void swap(db_t &other) noexcept;

    // Async request processing
    void processRequests();
    void handleClientRequest(client_request_t request);
    void monitorPendingRequests();

    // Raft commit callback
    auto onRaftCommit(const raft::v1::LogEntry &entry) -> bool;
    void onLeaderChange(bool isLeader);

    // Helper methods
    auto forwardToLeader() -> db_op_result_t;
    auto getLeaderAddress() -> std::string;
    void requestSuccess(request_id_t id) ABSL_SHARED_LOCKS_REQUIRED(m_pendingMutex);
    void requestFailed(request_id_t id) ABSL_SHARED_LOCKS_REQUIRED(m_pendingMutex);

    // Core components
    config::shared_ptr_t                           m_config;
    manifest::shared_ptr_t                         m_pManifest;
    lsmtree_ptr_t                                  m_pLSMtree;
    std::shared_ptr<consensus::consensus_module_t> m_pConsensusModule;

    // Request handling
    concurrency::thread_safe_queue_t<client_request_t> m_requestQueue;
    concurrency::thread_pool_t                         m_requestPool;
    std::thread                                        m_requestProcessor;
    std::thread                                        m_pendingMonitor;

    // Pending request tracking
    mutable absl::Mutex m_pendingMutex;
    absl::flat_hash_map<request_id_t, client_request_t>
        m_pendingRequests     ABSL_GUARDED_BY(m_pendingMutex);
    std::atomic<request_id_t> m_requestIdCounter{0};

    // Leader tracking
    std::atomic<bool>           m_isLeader{false};
    mutable absl::Mutex         m_leaderMutex;
    std::string m_leaderAddress ABSL_GUARDED_BY(m_leaderMutex);

    // Shutdown flag
    std::atomic<bool> m_shutdown{false};

    // Metrics
    std::atomic<request_id_t> m_requestsProcessed{0};
    std::atomic<request_id_t> m_requestsFailed{0};
    std::atomic<request_id_t> m_requestsForwarded{0};
};

using shared_ptr_t = std::shared_ptr<db_t>;
template <typename... Args> auto make_shared(Args &&...args)
{
    return std::make_shared<db_t>(std::forward<Args>(args)...);
}

} // namespace db

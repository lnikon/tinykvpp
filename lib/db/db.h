#pragma once

#include "config/config.h"
#include "raft/raft.h"
#include "wal/wal.h"
#include "structures/lsmtree/lsmtree.h"
#include "db/manifest/manifest.h"
#include <memory>

namespace db
{

enum class db_put_context_k
{
    none_k,             // No context
    do_not_replicate_k, // Do not replicate this put operation
    replicate_k         // Replicate this put operation
};

class db_t final
{
  public:
    using record_t = structures::memtable::memtable_t::record_t;
    using wal_ptr_t = std::shared_ptr<wal::wal_t<wal::wal_entry_t>>;
    using lsmtree_ptr_t = std::shared_ptr<structures::lsmtree::lsmtree_t>;
    using key_t = structures::lsmtree::key_t;
    using value_t = structures::lsmtree::value_t;

    explicit db_t(
        config::shared_ptr_t   config,
        wal_ptr_t              pWal,
        manifest::shared_ptr_t pManifest,
        lsmtree_ptr_t          pLsmtree
    ) noexcept;

    db_t(db_t &&other) noexcept;
    auto operator=(db_t &&other) noexcept -> db_t &;

    db_t(const db_t &) = delete;
    auto operator=(const db_t &) -> db_t & = delete;

    ~db_t() noexcept = default;

    [[nodiscard]] auto open() -> bool;

    [[nodiscard]] auto put(key_t key, value_t value, db_put_context_k context) noexcept -> bool;
    [[nodiscard]] auto put(record_t record, db_put_context_k context) noexcept -> bool;

    [[nodiscard]] auto get(const key_t &key) -> std::optional<record_t>;

    [[nodiscard]] auto config() const noexcept -> config::shared_ptr_t;

  private:
    [[nodiscard]] auto prepare_directory_structure() -> bool;

    void swap(db_t &other) noexcept;

    config::shared_ptr_t                      m_config;
    wal_ptr_t                                 m_pWal;
    manifest::shared_ptr_t                    m_pManifest;
    structures::lsmtree::shared_ptr_t         m_pLsmtree;
    std::shared_ptr<raft::consensus_module_t> m_pConsensusModule;
};

using shared_ptr_t = std::shared_ptr<db_t>;
template <typename... Args> auto make_shared(Args &&...args)
{
    return std::make_shared<db_t>(std::forward<Args>(args)...);
}

class db_builder_t
{
  public:
    [[nodiscard]] auto build(
        config::shared_ptr_t   config,
        db_t::wal_ptr_t        pWal,
        manifest::shared_ptr_t pManifest,
        db_t::lsmtree_ptr_t    pLSMTree
    ) -> std::optional<db_t>
    {
        return std::make_optional(
            db_t{std::move(config), std::move(pWal), std::move(pManifest), std::move(pLSMTree)}
        );
    }
};
} // namespace db

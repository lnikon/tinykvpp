#pragma once

#include "structures/memtable/memtable.h"
#include "wal/wal.h"
#include <config/config.h>
#include <structures/lsmtree/lsmtree.h>
#include <db/manifest/manifest.h>

namespace db
{

class db_t
{
  public:
    /**
     * @brief Construct a new db_t object
     *
     * @param config
     */
    explicit db_t(config::shared_ptr_t config, wal::wal_wrapper_t wal);

    /**
     * @brief Open database
     *
     * @return true
     * @return false
     */
    [[nodiscard]] auto open() -> bool;

    /**
     * @brief Put key-value pair into the database
     *
     * @param key
     * @param value
     */
    void put(const structures::lsmtree::key_t &key, const structures::lsmtree::value_t &value);

    /**
     * @brief Get key-value pair from database
     *
     * @param key
     * @return std::optional<structures::memtable::memtable_t::record_t>
     */
    auto get(const structures::lsmtree::key_t &key) -> std::optional<structures::memtable::memtable_t::record_t>;

    /**
     *
     * @return Return configuration passed to the database during the start
     */
    auto config() const noexcept -> config::shared_ptr_t;

  private:
    auto prepare_directory_structure() -> bool;

    config::shared_ptr_t           m_config;
    manifest::shared_ptr_t         m_manifest;
    wal::wal_wrapper_t             m_wal;
    structures::lsmtree::lsmtree_t m_lsmTree;
};

using shared_ptr_t = std::shared_ptr<db_t>;

template <typename... Args> auto make_shared(Args &&...args)
{
    return std::make_shared<db_t>(std::forward<Args>(args)...);
}

// Updated database builder
class db_builder_t
{
  public:
    // For persistent storage backends
    template <wal::log::TStorageBackendConcept TBackend>
    [[nodiscard]] auto build(config::shared_ptr_t pConfig) -> std::optional<db_t>
    {
        std::optional<wal::wal_wrapper_t> wal_wrapper_opt = std::nullopt;
        if (pConfig->WALConfig.storageType == wal::log_storage_type_k::in_memory_k)
        {
            std::expected<wal::wal_wrapper_t, wal::wal_builder_error_t> &&wal =
                wal::wal_builder_t<wal::log::storage_tags::in_memory_tag>{}.build();
            if (!wal.has_value())
            {
                return std::nullopt;
            }
            wal_wrapper_opt.emplace(std::move(wal.value()));
        }
        else if (pConfig->WALConfig.storageType == wal::log_storage_type_k::file_based_persistent_k)
        {
            std::expected<wal::wal_wrapper_t, wal::wal_builder_error_t> &&wal =
                wal::wal_builder_t<wal::log::storage_tags::file_backend_tag>{}.set_file_path(pConfig->WALConfig.path).build();
            if (!wal.has_value())
            {
                return std::nullopt;
            }
            wal_wrapper_opt.emplace(std::move(wal.value()));
        }

        if (!wal_wrapper_opt.has_value())
        {
            return std::nullopt;
        }

        return std::make_optional<db_t>(pConfig, std::move(wal_wrapper_opt.value()));
    }
};
} // namespace db

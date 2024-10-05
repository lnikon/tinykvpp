#ifndef STRUCTURES_LSMTREE_LSMTREE_T_H
#define STRUCTURES_LSMTREE_LSMTREE_T_H

#include "structures/memtable/memtable.h"
#include <db/manifest/manifest.h>
#include <db/wal/wal.h>
#include <structures/lsmtree/levels/levels.h>
#include <structures/lsmtree/lsmtree_config.h>
#include <structures/lsmtree/lsmtree_types.h>
#include "concurrency/thread_safe_queue.h"

#include <cassert>
#include <optional>

namespace structures::lsmtree
{

class lsmtree_t
{
  public:
    /**
     * @brief Constructs an instance of lsmtree_t.
     *
     * @param pConfig Shared pointer to the configuration object.
     * @param pManifest Shared pointer to the database manifest.
     * @param pWal Shared pointer to the write-ahead log.
     */
    explicit lsmtree_t(const config::shared_ptr_t &pConfig,
                       db::manifest::shared_ptr_t  pManifest,
                       db::wal::shared_ptr_t       pWal) noexcept;

    /**
     * @brief Deleted default constructor for the lsmtree_t class.
     *
     * This constructor is explicitly deleted to prevent the creation of
     * lsmtree_t objects without proper initialization. Users must use
     * other constructors provided by the class to create instances.
     */
    lsmtree_t() = delete;

    /**
     * @brief Deleted copy constructor to prevent copying of lsmtree_t instances.
     *
     * The copy constructor for the lsmtree_t class is explicitly deleted to avoid
     * unintended copying of instances. This ensures that each instance of lsmtree_t
     * is unique and cannot be copied, which is important for maintaining the integrity
     * and consistency of the data structure.
     */
    lsmtree_t(const lsmtree_t &) = delete;

    /**
     * @brief Deleted copy assignment operator to prevent copying of lsmtree_t instances.
     *
     * This operator is explicitly deleted to ensure that instances of lsmtree_t
     * cannot be copied. This is typically done to avoid unintended copying of
     * resources or to enforce unique ownership semantics.
     */
    auto operator=(const lsmtree_t &) -> lsmtree_t & = delete;

    /**
     * @brief Deleted move constructor for lsmtree_t.
     *
     * This constructor is explicitly deleted to prevent moving instances of
     * lsmtree_t. This ensures that the internal state of the LSM tree is not
     * inadvertently altered or corrupted by move operations.
     */
    lsmtree_t(lsmtree_t &&) = delete;

    /**
     * @brief Destructor for the lsmtree_t class.
     *
     * This destructor ensures that the flushing thread is properly joined before
     * the object is destroyed. If the flushing thread is joinable, it will be
     * joined. If it is not joinable, an error message will be logged using spdlog.
     */
    ~lsmtree_t() noexcept;

    /**
     * @brief Deleted move assignment operator.
     *
     * This move assignment operator is explicitly deleted to prevent
     * moving of lsmtree_t instances. This ensures that the resources
     * managed by lsmtree_t are not inadvertently transferred or
     * invalidated.
     *
     * @return This function does not return anything as it is deleted.
     */
    auto operator=(lsmtree_t &&) -> lsmtree_t & = delete;

    /**
     * @brief Inserts a key-value pair into the LSM tree.
     *
     * This function records the addition of a new key into the Write-Ahead Log (WAL)
     * and adds the record into the memtable. If the size of the memtable exceeds the
     * configured threshold after the addition, the memtable is flushed to disk.
     *
     * @param key The key to be inserted.
     * @param value The value associated with the key.
     */
    void put(const key_t &key, const value_t &value) noexcept;

    /**
     * @brief Retrieves the record associated with the given key.
     *
     * This function attempts to find the record associated with the specified key.
     * It first checks the in-memory table for the record. If the record is not found
     * in the in-memory table, it then searches the on-disk segments.
     *
     * @param key The key for which the record is to be retrieved.
     * @return std::optional<record_t> The record associated with the key, or std::nullopt if the record is not found.
     */
    [[nodiscard]] auto get(const key_t &key) noexcept -> std::optional<record_t>;

    /**
     * @brief Recovers the LSM tree from persistent storage.
     *
     * This function performs the recovery process for the LSM tree by restoring
     * its state from the manifest file and the Write-Ahead Log (WAL). During the
     * recovery phase, updates to the manifest are disabled to ensure consistency.
     *
     * @return true if the recovery process is successful, false otherwise.
     *
     * @note This function assumes that the configuration and manifest objects
     *       (m_pConfig and m_manifest) are already initialized.
     */
    auto recover() noexcept -> bool;

  private:
    /**
     * @brief Restores the manifest from the persistent storage.
     *
     * This function iterates over the records in the manifest and applies the necessary operations
     * to restore the state of the LSM tree. It handles both segment and level records, performing
     * operations such as adding or removing segments and creating levels. After applying all
     * modifications, it restores the in-memory indices for all levels.
     *
     * @return true if the manifest is successfully restored, false otherwise.
     */
    auto restore_manifest() noexcept -> bool;

    /**
     * @brief Restores the Write-Ahead Log (WAL) for the LSM tree.
     *
     * This function iterates through the records in the WAL and applies the operations
     * to the in-memory table (memtable). It supports adding records to the memtable
     * but does not support recovery of delete operations.
     *
     * @return true if the WAL was successfully restored.
     */
    auto restore_wal() noexcept -> bool;

    // Configuration shared throughout the database
    const config::shared_ptr_t m_pConfig;

    // Data storage. Protected by @m_mutex.
    absl::Mutex                         m_mutex;
    std::optional<memtable::memtable_t> m_table;
    db::manifest::shared_ptr_t          m_pManifest;
    db::wal::shared_ptr_t               m_pWal;
    levels::levels_t                    m_levels;

    // Communication channels. Thread-safe queues for inter-thread communication.
    std::jthread                                           m_flushing_thread;
    concurrency::thread_safe_queue_t<memtable::memtable_t> m_flushing_queue;
};

} // namespace structures::lsmtree

#endif // STRUCTURES_LSMTREE_LSMTREE_T_H

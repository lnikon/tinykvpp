#ifndef ZKV_SEGMENT_INTERFACE_H
#define ZKV_SEGMENT_INTERFACE_H

#include <structures/hashindex/hashindex.h>
#include <structures/lsmtree/lsmtree_types.h>
#include <structures/lsmtree/segments/types.h>

#include <filesystem>
#include <memory>
#include <string>

namespace structures::lsmtree::segments::interface
{

namespace types = lsmtree::segments::types;

enum class state_t
{
    ReadyToFlush,
    Flushed,
};

/**
 * @class segment_interface_t
 * @brief 
 *
 */
class segment_interface_t
{
  public:
    /**
     * @brief 
     */
    segment_interface_t() = default;

    /**
     * @brief 
     */
    virtual ~segment_interface_t() noexcept = default;

    /**
     * @brief 
     */
    segment_interface_t(const segment_interface_t &) = delete;

    /**
     * @brief 
     *
     * @return 
     */
    segment_interface_t &operator=(const segment_interface_t &) = delete;

    /**
     * @brief 
     */
    segment_interface_t(segment_interface_t &&) = delete;
    
    /**
     * @brief 
     *
     * @return 
     */
    segment_interface_t &operator=(segment_interface_t &&) = delete;

    /**
     * @brief 
     */
    [[nodiscard]] virtual types::name_t get_name() const = 0;

    /**
     * @brief 
     */
    [[nodiscard]] virtual types::path_t get_path() const = 0;

    /**
     * @brief 
     *
     * @param key 
     */
    [[nodiscard]] virtual std::vector<std::optional<lsmtree::record_t>> record(const lsmtree::key_t &key) = 0;

    /**
     * @brief 
     *
     * @param offset 
     */
    [[nodiscard]] virtual std::optional<lsmtree::record_t> record(const hashindex::hashindex_t::offset_t &offset) = 0;

    /**
     * @brief 
     */
    virtual void flush() = 0;

    /**
     * @brief 
     */
    virtual std::optional<memtable::memtable_t> memtable() = 0;

    /**
     * @brief 
     */
    virtual void restore() = 0;

    /**
     * @brief 
     */
    virtual std::filesystem::file_time_type last_write_time() = 0;

    /**
     * @brief 
     */
    [[nodiscard]] virtual std::optional<record_t::key_t> min() const noexcept = 0;

    /**
     * @brief 
     */
    [[nodiscard]] virtual std::optional<record_t::key_t> max() const noexcept = 0;
};

using shared_ptr_t = std::shared_ptr<segment_interface_t>;

} // namespace structures::lsmtree::segments::interface

#endif // ZKV_SEGMENT_INTERFACE_H

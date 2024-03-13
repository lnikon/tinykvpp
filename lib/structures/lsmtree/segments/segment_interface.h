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

class segment_interface_t
{
   public:
    segment_interface_t() = default;
    virtual ~segment_interface_t() noexcept = default;

    segment_interface_t(const segment_interface_t &) = delete;
    segment_interface_t &operator=(const segment_interface_t &) = delete;

    segment_interface_t(segment_interface_t &&) = delete;
    segment_interface_t &operator=(segment_interface_t &&) = delete;

    [[nodiscard]] virtual types::name_t get_name() const = 0;
    [[nodiscard]] virtual types::path_t get_path() const = 0;

    [[nodiscard]] virtual std::optional<lsmtree::record_t> record(
        const lsmtree::key_t &key) = 0;
    virtual void flush() = 0;
    virtual memtable::unique_ptr_t memtable() = 0;
    virtual void restore() = 0;
};

using shared_ptr_t = std::shared_ptr<segment_interface_t>;

}  // namespace structures::lsmtree::segments::interface

#endif  // ZKV_SEGMENT_INTERFACE_H

//
// Created by nikon on 2/6/22.
//

#include <optional>
#include <structures/lsmtree/segments/lsmtree_mock_segment.h>

#include <cassert>

namespace structures::lsmtree::segments::mock_segment
{

mock_segment_t::mock_segment_t([[maybe_unused]] std::filesystem::path path, memtable::memtable_t pMemtable) noexcept
    : segment_interface_t{},
      m_pMemtable{std::move(pMemtable)}
{
}

[[nodiscard]] std::vector<std::optional<lsmtree::record_t>> mock_segment_t::record(const lsmtree::key_t &)
{
    return {};
}

std::optional<record_t> mock_segment_t::record(const hashindex::hashindex_t::offset_t &)
{
    return std::nullopt;
}

void mock_segment_t::flush()
{
    assert(m_pMemtable);
    std::stringstream ss;
    for (const auto &kv : *m_pMemtable)
    {
        // kv.write(ss);
    }
}

types::name_t mock_segment_t::get_name() const
{
    return structures::lsmtree::segments::types::name_t();
}

types::path_t mock_segment_t::get_path() const
{
    return structures::lsmtree::segments::types::path_t();
}

std::optional<memtable::memtable_t> mock_segment_t::memtable()
{
    return std::nullopt;
}

std::filesystem::file_time_type mock_segment_t::last_write_time()
{
    return std::filesystem::file_time_type::min();
}

void mock_segment_t::restore()
{
}

[[nodiscard]] std::optional<record_t::key_t> mock_segment_t::min() const noexcept
{
    return std::nullopt;
}

[[nodiscard]] std::optional<record_t::key_t> mock_segment_t::max() const noexcept
{
    return std::nullopt;
}

} // namespace structures::lsmtree::segments::mock_segment

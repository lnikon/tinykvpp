//
// Created by nikon on 2/6/22.
//

#include <structures/lsmtree/lsmtree_types.h>
#include <structures/lsmtree/segments/lsmtree_regular_segment.h>

#include <cassert>
#include <fstream>
#include <optional>

namespace structures::lsmtree::segments::regular_segment
{

regular_segment_t::regular_segment_t(std::filesystem::path path, types::name_t name, memtable::memtable_t memtable)
    : segment_interface_t{},
      m_path{std::move(path)},
      m_name{std::move(name)},
      m_memtable{std::make_optional<memtable_t>(std::move(memtable))}
{
}

[[nodiscard]] std::vector<std::optional<record_t>> regular_segment_t::record(const lsmtree::key_t &key)
{
    assert(!m_hashIndex.empty());
    std::vector<std::optional<record_t>> result;
    for (const auto offsets{m_hashIndex.offset(key)}; const auto &offset : offsets)
    {
        result.emplace_back(record(offset));
    }
    return result;
}

std::optional<record_t> regular_segment_t::record(const hashindex::hashindex_t::offset_t &offset)
{
    std::fstream ss{get_path(), std::ios::in};
    ss.seekg(offset);

    memtable_t::record_t record;
    record.read(ss);

    return std::make_optional(std::move(record));
}

void regular_segment_t::flush()
{
    // Skip execution if for some reason the memtable is emptu
    if (m_memtable->empty())
    {
        std::cerr << "can not flush empty memtable" << std::endl;
        return;
    }

    // Serialize into stringstream and build hash index
    std::stringstream ss;
    for (const auto &kv : m_memtable.value())
    {
        std::size_t ss_before = ss.tellp();
        kv.write(ss);
        m_hashIndex.emplace(kv, static_cast<std::size_t>(ss.tellp()) - ss_before);
    }
    ss << std::endl;

    // Flush the segment onto the disk
    std::fstream stream(get_path(), std::fstream::trunc | std::fstream::out);
    if (!stream.is_open())
    {
        std::cerr << "[regular_segment_t::flush]: "
                  << "unable to open \"" << get_path() << "\"" << std::endl;
        return;
    }

    assert(!ss.str().empty());
    stream << ss.str();
    stream.flush();

    std::cout << "[regular_segment_t::flush]: "
              << "Successfully flushed segment: \"" << get_path() << "\"" << std::endl;

    // Free the memory occupied by the segment on successful flush
    // m_memtable = memtable_t{};
}

std::filesystem::file_time_type regular_segment_t::last_write_time()
{
    return std::filesystem::exists(get_path()) ? std::filesystem::last_write_time(get_path())
                                               : std::filesystem::file_time_type::min();
}

std::optional<record_t::key_t> regular_segment_t::min() const noexcept
{
    return m_memtable->min();
}

std::optional<record_t::key_t> regular_segment_t::max() const noexcept
{

    return m_memtable->max();
}

types::name_t regular_segment_t::get_name() const
{
    return m_name;
}

types::path_t regular_segment_t::get_path() const
{
    return m_path;
}

std::optional<memtable::memtable_t> regular_segment_t::memtable()
{
    return std::move(m_memtable);
}

void regular_segment_t::restore()
{
    m_memtable = memtable::memtable_t{};
    for (const auto &[_, offset] : m_hashIndex)
    {
        if (auto recordOpt{record(offset)}; recordOpt.has_value())
        {
            m_memtable->emplace(recordOpt.value());
        }
    }
}

} // namespace structures::lsmtree::segments::regular_segment

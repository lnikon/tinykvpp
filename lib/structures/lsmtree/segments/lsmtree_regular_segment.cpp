//
// Created by nikon on 2/6/22.
//

#include <structures/lsmtree/lsmtree_types.h>
#include <structures/lsmtree/segments/lsmtree_regular_segment.h>

#include <cassert>
#include <fstream>
#include <optional>

#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/device/file_descriptor.hpp>

namespace structures::lsmtree::segments::regular_segment
{

regular_segment_t::regular_segment_t(std::filesystem::path path,
                                     types::name_t name,
                                     memtable::unique_ptr_t pMemtable)
    : segment_interface_t{},
      m_path{std::move(path)},
      m_name{std::move(name)},
      m_pMemtable{std::move(pMemtable)}
{
}

[[nodiscard]] std::vector<std::optional<record_t>> regular_segment_t::record(
    const lsmtree::key_t &key)
{
    // TODO(lnikon): Check `prepopulate_segment_index`. If set, skip building
    // the index.

    assert(!m_hashIndex.empty());

    std::vector<std::optional<record_t>> result;
    for (const auto offsets{m_hashIndex.offset(key)};
         const auto &offset : offsets)
    {
        result.emplace_back(record(offset));
    }

    return result;
}

std::optional<record_t> regular_segment_t::record(
    const hashindex::hashindex_t::offset_t &offset)
{
    // TODO(lnikon): Consider memory mapping
    std::fstream segmentStream{get_path(), std::ios::in};
    segmentStream.seekg(offset);

    // TODO(lnikon): Check that offset isn't greater than the stream
    std::size_t keySz{0};
    lsmtree::record_t::key_t::storage_type_t keyFromDisk;
    std::size_t valueSz{0};
    std::string valueStr;
    lsmtree::record_t::value_t::underlying_value_type_t value;

    segmentStream >> keySz;
    segmentStream >> keyFromDisk;
    segmentStream >> valueSz;

    // TODO(lnikon): Do dynamic dispatch based on the user type of the
    // value
    segmentStream >> valueStr;
    value = valueStr;

    return std::make_optional(
        record_t{key_t{std::move(keyFromDisk)}, value_t{std::move(value)}});
}

void regular_segment_t::flush()
{
    // If m_pMemtable is null, then segment has been flushed
    if (!m_pMemtable)
    {
        return;
    }

    std::stringstream ss;
    for (const auto &kv : *m_pMemtable)
    {
        std::size_t ss_before = ss.tellp();
        ss << kv;
        m_hashIndex.emplace(kv,
                            static_cast<std::size_t>(ss.tellp()) - ss_before);
    }
    ss << std::endl;

    // TODO(vahag): Use fadvise() and O_DIRECT
    // TODO(vahag): Async IO?
    boost::iostreams::stream<boost::iostreams::file_descriptor_sink> stream(
        get_path());
    if (!stream.is_open())
    {
        // TODO(vahag): How to handle situation when it's impossible to
        // flush memtable into disk?
        std::cerr << "[regular_segment_t::flush]: "
                  << "unable to open \"" << get_path() << "\"" << std::endl;
        return;
    }

    assert(!ss.str().empty());
    stream << ss.str();
    stream.flush();
    ::fsync(stream->handle());

    std::cout << "[regular_segment_t::flush]: "
              << "Successfully flushed segment: \"" << get_path() << "\""
              << std::endl;

    // Free the memory occupied by the segment on successful flush
    m_pMemtable.reset();
    m_pMemtable = nullptr;
}

std::filesystem::file_time_type regular_segment_t::last_write_time()
{
    return std::filesystem::exists(get_path())
               ? std::filesystem::last_write_time(get_path())
               : std::filesystem::file_time_type::min();
}

types::name_t regular_segment_t::get_name() const
{
    return m_name;
}

types::path_t regular_segment_t::get_path() const
{
    return m_path;
}

memtable::unique_ptr_t regular_segment_t::memtable()
{
    return std::move(m_pMemtable);
}

void regular_segment_t::restore()
{
    // TODO: Check that the segment is in appropriate state to be restored
    if (m_pMemtable)
    {
        return;
    }
    
    m_pMemtable.reset();
    m_pMemtable = memtable::make_unique();

    for (const auto &[_, offset] : m_hashIndex)
    {
        if (auto recordOpt{record(offset)}; recordOpt.has_value())
        {
            m_pMemtable->emplace(recordOpt.value());
        }
    }
}

}  // namespace structures::lsmtree::segments::regular_segment

#include "fs/append_only_file.h"
#include <iostream>
#include <structures/lsmtree/lsmtree_types.h>
#include <structures/lsmtree/segments/lsmtree_regular_segment.h>

#include <cassert>
#include <fstream>
#include <optional>
#include <ios>
#include <stdexcept>
#include <string>

namespace structures::lsmtree::segments::regular_segment
{

const auto footerSize{128}; // bytes

regular_segment_t::regular_segment_t(fs::path_t path, types::name_t name, memtable::memtable_t memtable)
    : m_path{std::move(path)},
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
    // Skip execution if for some reason the memtable is empty
    if (m_memtable->empty())
    {
        // std::cerr << "can not flush empty memtable" << std::endl;
        return;
    }

    // Serialize memtable into stringstream and build hash index
    std::stringstream ss;
    for (const auto &kv : m_memtable.value())
    {
        std::size_t ss_before = ss.tellp();
        kv.write(ss);
        m_hashIndex.emplace(kv, static_cast<std::size_t>(ss.tellp()) - ss_before);
    }
    ss << std::endl;

    // Calcuate datablock size
    // const auto dataBlockSize{ss.tellp() - dataBlockOffset};

    // Serialize hashindex
    const auto hashIndexBlockOffset{ss.tellp()};
    for (const auto &[key, offset] : m_hashIndex)
    {
        ss << key.m_key << ' ' << offset << std::endl;
    }

    // Calculate size of the index block
    const auto hashIndexBlockSize{ss.tellp() - hashIndexBlockOffset};

    // Get offset into footer section
    const auto footerBlockOffset{ss.tellp()};

    // Serialize footer
    // TODO(lnikon): First "big enough" number that came to my mind :)
    ss << hashIndexBlockOffset << ' ' << hashIndexBlockSize << std::endl;

    const auto footerPaddingSize{footerSize - (ss.tellp() - footerBlockOffset)};
    ss << std::string(footerPaddingSize, ' ') << std::endl;

    // Flush the segment onto the disk
    std::fstream stream(get_path(), std::fstream::trunc | std::fstream::out);
    if (!stream.is_open())
    {
        throw std::runtime_error("unable to flush segment for path " + m_path.string());
    }

    assert(!ss.str().empty());
    stream << ss.str();
    stream.flush();

    // std::cout << "[regular_segment_t::flush]: "
    //   << "Successfully flushed segment: \"" << get_path() << "\"" << std::endl;

    // Free the memory occupied by the segment on successful flush
    // m_memtable = memtable_t{};
}

void regular_segment_t::purge()
{
    if (std::filesystem::exists(get_path()))
    {
        std::filesystem::remove(get_path());
    }

    // TODO(lnikon): Remove in-memory components as well?
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

std::optional<memtable::memtable_t> &regular_segment_t::memtable()
{
    return m_memtable;
}

std::optional<memtable::memtable_t> regular_segment_t::moved_memtable()
{
    return m_memtable.has_value() ? std::move(m_memtable) : std::nullopt;
}

void regular_segment_t::restore()
{
    // TODO(lnikon): Do we need this condition?
    if (!m_memtable->empty())
    {
        return;
    }

    // Recover hashindex if its empty
    if (m_hashIndex.empty())
    {
        restore_index();
    }

    // Prepare an empty memtable and start filling it up with help of hashindex
    m_memtable = memtable::memtable_t{};
    for (const auto &[_, offset] : m_hashIndex)
    {
        if (auto recordOpt{record(offset)}; recordOpt.has_value())
        {
            m_memtable->emplace(recordOpt.value());
        }
    }
}

// TODO(lnikon): Add validations on file size. Need 'RandomAccessFile'.
void regular_segment_t::restore_index()
{
    std::fstream sst(m_path);
    if (!sst.is_open())
    {
        // TODO(lnikon): Better way to handle this case. Wihout exceptions.
        throw std::runtime_error("unable to open SST " + m_path.string());
    }

    // Seek to the beggining of the footer
    sst.seekg(-footerSize - 1, std::ios_base::end);

    // Read index block offset and size
    std::size_t indexBlockOffset{0};
    std::size_t indexBlockSize{0};
    sst >> indexBlockOffset;
    sst >> indexBlockSize;

    // Seek to the begging of the index block
    sst.seekg(indexBlockOffset, std::ios_base::beg);

    // Start reading <key, offset> pairs
    std::size_t bytesRead{0};
    std::string key;
    std::size_t offset;
    while (bytesRead < indexBlockSize)
    {
        auto start = sst.tellg();
        sst >> key;
        auto end = sst.tellg();
        bytesRead += end - start;

        start = sst.tellg();
        sst >> offset;
        end = sst.tellg();
        bytesRead += end - start;

        m_hashIndex.emplace(structures::lsmtree::record_t{key_t{key}, value_t{}}, offset);
    }
}

} // namespace structures::lsmtree::segments::regular_segment

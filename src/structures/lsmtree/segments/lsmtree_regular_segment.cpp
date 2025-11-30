#include <cassert>
#include <cstdint>
#include <fstream>
#include <optional>
#include <ios>
#include <stdexcept>
#include <string>
#include <iostream>

#include <spdlog/spdlog.h>
#include <magic_enum/magic_enum.hpp>
#include <libassert/assert.hpp>

#include "serialization/buffer_writer.h"
#include "serialization/common.h"
#include "serialization/endian_integer.h"
#include "structures/lsmtree/lsmtree_types.h"
#include "structures/memtable/memtable.h"
#include "structures/lsmtree/segments/lsmtree_regular_segment.h"

namespace
{

constexpr const std::uint32_t SEGMENT_HEADER_MAGIC_NUMBER{0xDEADBEEF};
constexpr const std::uint32_t SEGMENT_VERSION_NUMBER{1};

[[nodiscard]] auto calculate_header_size_bytes() noexcept -> std::uint32_t
{
    std::uint32_t headerSize{0};
    headerSize += 4; // magic
    headerSize += 4; // version
    headerSize += 4; // body_offset
    headerSize += 4; // index_offset
    headerSize += 4; // size
    headerSize += 4; // crc32
    return headerSize;
}

[[nodiscard]] auto
calculate_body_size_bytes(const structures::memtable::memtable_t &memtable) noexcept
    -> std::uint32_t
{
    std::uint32_t memtableSize{0};
    for (const auto &record : memtable)
    {
        memtableSize += serialization::varint_size(record.m_key.m_key.length());
        memtableSize += record.m_key.m_key.length();
        memtableSize += serialization::varint_size(record.m_value.m_value.length());
        memtableSize += record.m_value.m_value.length();
        memtableSize += sizeof(std::uint64_t); // u64 timestamp
    }
    return memtableSize;
}

[[nodiscard]] auto
calculate_index_size_bytes(const structures::memtable::memtable_t &memtable) noexcept
    -> std::uint32_t
{
    std::uint32_t indexSize{0};
    for (const auto &record : memtable)
    {
        indexSize += serialization::varint_size(record.m_key.m_key.length());
        indexSize += record.m_key.m_key.length();
        indexSize += sizeof(std::uint32_t); // u32 body_offset
    }
    return indexSize;
}

// TODO(lnikon): Use strong types for headerSize, bodySize, bufferSize, as they are consecutive
// u32s and can be mixed up
[[nodiscard]] auto serialize_header(
    serialization::buffer_writer_t &writer,
    std::uint32_t                   headerSize,
    std::uint32_t                   bodySize,
    std::uint32_t                   bufferSize
) noexcept -> std::optional<serialization::serialization_error_k>
{
    return writer
        .write_endian_integer(
            serialization::le_uint32_t{SEGMENT_HEADER_MAGIC_NUMBER}
        )                                                                         // magic_number
        .write_endian_integer(serialization::le_uint32_t{SEGMENT_VERSION_NUMBER}) // version
        .write_endian_integer(serialization::le_uint32_t{headerSize})             // body_offset
        .write_endian_integer(serialization::le_uint32_t{headerSize + bodySize})  // index_ofset
        .write_endian_integer(serialization::le_uint32_t{bufferSize})             // size
        .error();
}

[[nodiscard]] auto serialize_body(
    serialization::buffer_writer_t &writer, const structures::memtable::memtable_t &memtable
) noexcept -> std::optional<serialization::serialization_error_k>
{
    for (const auto &record : memtable)
    {
        if (writer.write_varint(record.m_key.m_key.length())
                .write_string(record.m_key.m_key)
                .write_varint(record.m_value.m_value.length())
                .write_string(record.m_value.m_value)
                .write_endian_integer(
                    serialization::le_uint64_t{static_cast<std::uint32_t>(
                        record.m_timestamp.m_value.time_since_epoch().count()
                    )}
                )
                .has_error())
        {
            spdlog::debug(
                "Failed to serialize memtable record: Error={}, key={}, value={}, "
                "sequenceNumber={}, "
                "timestamp={}",
                magic_enum::enum_name(writer.error().value()),
                record.m_key.m_key,
                record.m_value.m_value,
                record.m_sequenceNumber,
                static_cast<std::uint32_t>(record.m_timestamp.m_value.time_since_epoch().count())
            );
            return writer.error();
        }
    }
    return std::nullopt;
}

[[nodiscard]] auto serialize_index(
    serialization::buffer_writer_t         &writer,
    const structures::memtable::memtable_t &memtable,
    std::uint32_t                           bodyOffset
) noexcept -> std::optional<serialization::serialization_error_k>
{
    std::uint32_t nextIndexEntryOffsetInBody{bodyOffset};
    for (const auto &record : memtable)
    {
        if (writer.write_varint(record.m_key.m_key.length())
                .write_string(record.m_key.m_key)
                .write_endian_integer(serialization::le_uint64_t{nextIndexEntryOffsetInBody})
                .has_error())
        {
            spdlog::error(
                "Failed to serialize index entry. Error={}, key={}, offset={}",
                magic_enum::enum_name(writer.error().value()),
                record.m_key.m_key,
                nextIndexEntryOffsetInBody
            );
            return writer.error();
        }

        nextIndexEntryOffsetInBody += serialization::varint_size(record.m_key.m_key.length()) +
                                      record.m_key.m_key.length() +
                                      serialization::varint_size(record.m_value.m_value.length()) +
                                      record.m_value.m_value.length() + sizeof(std::uint64_t);
    }
    return std::nullopt;
}

[[nodiscard]] auto serialize_segment(const structures::memtable::memtable_t &memtable) noexcept
    -> std::optional<std::vector<std::byte>>
{
    // Calculate number of bytes being serialized
    const auto headerSize{calculate_header_size_bytes()};
    const auto bodySize{calculate_body_size_bytes(memtable)};
    const auto indexSize{calculate_index_size_bytes(memtable)};
    const auto bufferSize{headerSize + bodySize + indexSize};

    // Calculate offsets into body and index
    const auto bodyOffset{headerSize};

    // Allocate the buffer
    std::vector<std::byte> buffer;
    buffer.reserve(bufferSize);
    buffer.resize(bufferSize);

    // Serialize into the buffer
    serialization::buffer_writer_t writer{buffer};
    ASSERT(writer.bytes_written(), bufferSize);

    if (auto error = serialize_header(writer, headerSize, bodySize, bufferSize))
    {
        spdlog::error(
            "Failed to serialize segment header. Error={}",
            magic_enum::enum_name(writer.error().value())
        );
        return std::nullopt;
    }

    // TODO(lnikon): With a change to the binary_writer_t interface can combine these loops into a
    // one, need to supply write_pos to the writer with a proper offset for index start, which
    // initially should be index_offset=headerSize+memtableSize
    if (auto error = serialize_body(writer, memtable); error.has_value())
    {
        spdlog::error(
            "Failed to serialize segment body. Error={}",
            magic_enum::enum_name(writer.error().value())
        );
        return std::nullopt;
    }

    if (auto error = serialize_index(writer, memtable, bodyOffset); error.has_value())
    {
        spdlog::error(
            "Failed to serialize segment index. Error={}",
            magic_enum::enum_name(writer.error().value())
        );
        return std::nullopt;
    }

    return buffer;
}

} // namespace

namespace structures::lsmtree::segments::regular_segment
{

// TODO(lnikon): This is horrible.
const auto footerSize{128}; // bytes

regular_segment_t::regular_segment_t(
    fs::path_t path, types::name_t name, memtable::memtable_t memtable
) noexcept
    : m_path{std::move(path)},
      m_name{std::move(name)},
      m_memtable{std::make_optional<memtable_t>(std::move(memtable))}
{
}

[[nodiscard]] auto regular_segment_t::record(const key_t &key)
    -> std::vector<std::optional<record_t>>
{
    if (m_hashIndex.empty())
    {
        spdlog::warn("Hash index is empty for segment {}", m_path.c_str());
        // restore_index();
        assert(!m_hashIndex.empty());
    }

    const auto offsets{m_hashIndex.offset(key)};
    if (offsets.empty())
    {
        return {};
    }

    std::vector<std::optional<record_t>> result;
    result.reserve(offsets.size());
    for (const auto &offset : offsets)
    {
        result.emplace_back(record(offset));
    }

    return result;
}

auto regular_segment_t::record(const hashindex::hashindex_t::offset_t &offset)
    -> std::optional<record_t>
{
    std::fstream ss{get_path(), std::ios::in | std::fstream::binary};
    ss.seekg(offset);

    // TODO(lnikon): Use binary_buffer_reader_t here
    // memtable_t::record_t record;
    // record.read(ss);
    // return std::make_optional(std::move(record));

    return std::nullopt;
}

void regular_segment_t::flush()
{
    if (!m_memtable.has_value())
    {
        spdlog::warn("Can not flush empty memtable at segment {}", m_path.c_str());
        return;
    }

    const auto &memtable{m_memtable.value()};
    if (memtable.empty())
    {
        spdlog::warn("Can not flush memtable of size 0 at segment {}", m_path.c_str());
        return;
    }

    auto bufferOpt{serialize_segment(memtable)};
    if (!bufferOpt.has_value())
    {
        spdlog::error("Failed to serialize segment");
        return;
    }

    std::ofstream binaryStream(get_path(), std::ios::binary);
    if (!binaryStream.is_open())
    {
        spdlog::error(
            "Unable to open binary stream to write segment. Path={}", get_path().string()
        );
        return;
    }

    binaryStream.write(
        reinterpret_cast<const char *>(bufferOpt.value().data()), bufferOpt.value().size()
    );
}

void regular_segment_t::remove_from_disk() const noexcept
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

auto regular_segment_t::min() const noexcept -> std::optional<record_t::key_t>
{
    return m_memtable->min();
}

std::optional<record_t::key_t> regular_segment_t::max() const noexcept
{

    return m_memtable->max();
}

auto regular_segment_t::num_of_bytes_used() const -> std::size_t
{
    assert(m_memtable.has_value());
    return m_hashIndex.num_of_bytes_used() + m_memtable->num_of_bytes_used();
}

types::name_t regular_segment_t::get_name() const
{
    return m_name;
}

auto regular_segment_t::get_path() const -> fs::path_t
{
    return m_path;
}

auto regular_segment_t::memtable() -> std::optional<memtable::memtable_t> &
{
    return m_memtable;
}

auto regular_segment_t::moved_memtable() -> std::optional<memtable::memtable_t>
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

    // Recover hashindex if it is empty
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
    std::fstream sst(m_path, std::fstream::binary);
    if (!sst.is_open())
    {
        // TODO(lnikon): Better way to handle this case. Without exceptions.
        throw std::runtime_error("unable to open SST " + m_path.string());
    }

    // Seek to the beginning of the footer
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
    std::size_t offset{0};
    std::string line;
    while (bytesRead <= indexBlockSize - 1 && std::getline(sst, line))
    {
        std::istringstream lineStream{line};
        //        auto start = sst.tellg();
        lineStream >> key;
        //        auto end = sst.tellg();
        //        bytesRead += end - start;

        //        start = sst.tellg();
        lineStream >> offset;
        //        end = sst.tellg();
        //        bytesRead += end - start;
        bytesRead += line.size() + 1;

        m_hashIndex.emplace(
            structures::lsmtree::record_t{key_t{key}, value_t{}, sequence_number_t{}}, offset
        );
    }

    if (m_hashIndex.empty())
    {
        spdlog::warn("(restore_index): Hash index is empty for segment {}", m_path.c_str());
        assert(!m_hashIndex.empty());
    }
}

} // namespace structures::lsmtree::segments::regular_segment

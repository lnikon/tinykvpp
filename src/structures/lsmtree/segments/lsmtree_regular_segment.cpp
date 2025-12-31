#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <unistd.h>
#include <sys/mman.h>

#include <spdlog/spdlog.h>
#include <magic_enum/magic_enum.hpp>
#include <libassert/assert.hpp>

#include "structures/lsmtree/segments/lsmtree_regular_segment.h"
#include "posix_wrapper/open_flag.h"
#include "structures/lsmtree/lsmtree_types.h"
#include "structures/memtable/memtable.h"
#include "serialization/buffer_reader.h"
#include "serialization/buffer_writer.h"
#include "serialization/common.h"
#include "serialization/crc32.h"
#include "serialization/endian_integer.h"
#include "fs/random_access_file.h"

namespace
{

constexpr const std::uint32_t SEGMENT_HEADER_MAGIC_NUMBER{0xDEADBEEF};
constexpr const std::uint32_t SEGMENT_VERSION_NUMBER{1};

[[nodiscard]] auto calculate_header_size_bytes() noexcept -> std::uint32_t
{
    std::uint32_t headerSize{0};
    headerSize += sizeof(std::uint32_t); // magic
    headerSize += sizeof(std::uint32_t); // version
    headerSize += sizeof(std::uint32_t); // body_offset
    headerSize += sizeof(std::uint32_t); // index_offset
    headerSize += sizeof(std::uint32_t); // size
    headerSize += sizeof(std::uint32_t); // crc32
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
        memtableSize += sizeof(std::uint64_t); // sequence_number
        memtableSize += serialization::varint_size(record.m_value.m_value.length());
        memtableSize += record.m_value.m_value.length();
        memtableSize += sizeof(std::int64_t); // u64 timestamp
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
        .write_endian_integer(serialization::le_uint32_t{0}) // crc32, calculated later
        .error();
}

[[nodiscard]] auto serialize_crc32(
    serialization::buffer_writer_t   &writer,
    const std::span<const std::byte> &buffer,
    std::uint32_t                     bufferSize,
    std::uint32_t                     headerSize,
    std::uint32_t                     bodyOffset
) noexcept -> std::optional<serialization::serialization_error_k>
{
    const auto crc32{serialization::crc32_t{}
                         .update(
                             std::span(
                                 static_cast<const std::byte *>(buffer.data()),
                                 headerSize - sizeof(std::uint32_t)
                             )
                         )
                         .update(
                             std::span(
                                 static_cast<const std::byte *>(buffer.data() + bodyOffset),
                                 bufferSize - bodyOffset
                             )
                         )
                         .finalize()};

    return writer.write_endian_integer(serialization::le_uint32_t{crc32}).error();
}

[[nodiscard]] auto serialize_body(
    serialization::buffer_writer_t &writer, const structures::memtable::memtable_t &memtable
) noexcept -> std::optional<serialization::serialization_error_k>
{
    for (const auto &record : memtable)
    {
        if (writer.write_string(record.m_key.m_key)
                .write_endian_integer(serialization::le_uint64_t{record.m_sequenceNumber})
                .write_string(record.m_value.m_value)
                .write_endian_integer(
                    serialization::le_int64_t{static_cast<std::int64_t>(
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
                static_cast<std::int64_t>(record.m_timestamp.m_value.time_since_epoch().count())
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
        if (writer.write_string(record.m_key.m_key)
                .write_endian_integer(serialization::le_uint32_t{nextIndexEntryOffsetInBody})
                .has_error())
        {
            spdlog::error(
                "Failed to serialize index entry. Error={}, key={}, offset={}",
                magic_enum::enum_name(writer.error().value()),
                "",
                nextIndexEntryOffsetInBody
            );
            return writer.error();
        }

        nextIndexEntryOffsetInBody +=
            serialization::varint_size(record.m_key.m_key.length()) + record.m_key.m_key.length() +
            sizeof(std::uint64_t) /* seq_num */ +
            serialization::varint_size(record.m_value.m_value.length()) +
            record.m_value.m_value.length() + sizeof(std::int64_t) /* timestamp */;
    }
    return std::nullopt;
}

[[nodiscard]] auto serialize_segment(const structures::memtable::memtable_t &memtable) noexcept
    -> std::expected<std::vector<std::byte>, serialization::serialization_error_k>
{
    // Calculate number of bytes being serialized
    const auto headerSize{calculate_header_size_bytes()};
    const auto bodySize{calculate_body_size_bytes(memtable)};
    const auto indexSize{calculate_index_size_bytes(memtable)};
    const auto bufferSize{headerSize + bodySize + indexSize};

    // Calculate offsets into body and index
    const auto bodyOffset{headerSize};

    // Allocate the buffer
    std::vector<std::byte> buffer(bufferSize);

    // Serialize into the buffer
    serialization::buffer_writer_t writer{buffer};

    if (auto error = serialize_header(writer, headerSize, bodySize, bufferSize); error.has_value())
    {
        spdlog::error(
            "Failed to serialize segment header. Error={}",
            magic_enum::enum_name(writer.error().value())
        );
        return std::unexpected(writer.error().value());
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
        return std::unexpected(writer.error().value());
    }

    if (auto error = serialize_index(writer, memtable, bodyOffset); error.has_value())
    {
        spdlog::error(
            "Failed to serialize segment index. Error={}",
            magic_enum::enum_name(writer.error().value())
        );
        return std::unexpected(writer.error().value());
    }

    const auto previousCursor{writer.set_cursor(headerSize - sizeof(std::uint32_t))};
    if (auto error = serialize_crc32(writer, buffer, bufferSize, headerSize, bodyOffset);
        error.has_value())
    {
        spdlog::error(
            "Failed to serialize segment crc32. Error={}",
            magic_enum::enum_name(writer.error().value())
        );
        return std::unexpected(writer.error().value());
    }
    (void)writer.set_cursor(previousCursor);

    ASSERT(writer.bytes_written(), bufferSize);

    return buffer;
}

} // namespace

namespace structures::lsmtree::segments::regular_segment
{

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
        restore_index();
        ASSERT(!m_hashIndex.empty());
    }

    const auto offsets{m_hashIndex.offset(key)};
    if (offsets.empty())
    {
        spdlog::warn("No offset for key={} in index", key.m_key);
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

auto regular_segment_t::record(const hashindex::hashindex_t::offset_t &offset) const
    -> std::optional<record_t>
{
    const auto file{fs::random_access_file::random_access_file_builder_t{}.build(
        get_path(), posix_wrapper::open_flag_k::kReadOnly
    )};
    if (!file.has_value())
    {
        spdlog::error(
            "Failed to open segment file for reading record. Error={}, path={}",
            file.error().message,
            get_path().string()
        );
        return std::nullopt;
    }

    const auto fileSizeOpt{file->size()};
    if (!fileSizeOpt.has_value())
    {
        spdlog::error(
            "Failed to get segment file size. Error={}, path={}",
            file.error().message,
            get_path().string()
        );
        return std::nullopt;
    }
    const auto fileSize{fileSizeOpt.value()};
    if (fileSize == 0)
    {
        spdlog::error("Segment file size is zero. Path={}", get_path().string());
        return std::nullopt;
    }
    if (offset >= fileSize)
    {
        spdlog::error(
            "Invalid offset {} exceeds file size {}. Path={}", offset, fileSize, get_path().string()
        );
        return std::nullopt;
    }

    const auto fdOpt{file->fd()};
    if (!fdOpt.has_value())
    {
        spdlog::error(
            "Segment file has an invalid file descriptor. Error={}, path={}",
            file.error().message,
            get_path().string()
        );
        return std::nullopt;
    }
    const auto fd{fdOpt.value()};

    void *mmapPtr{mmap(nullptr, fileSize, PROT_READ, MAP_PRIVATE, fd, 0)};
    if (mmapPtr == MAP_FAILED)
    {
        // TODO(lnikon): Should fallback to read() + buffer?
        spdlog::error(
            "Failed to mmap segment file. Error={}, path={}", strerror(errno), get_path().string()
        );
        return std::nullopt;
    }

    madvise(mmapPtr, fileSize, MADV_RANDOM);

    std::span<std::byte> mmapSpan{static_cast<std::byte *>(mmapPtr) + offset, fileSize - offset};

    serialization::buffer_reader_t reader{mmapSpan};

    std::string                key;
    serialization::le_uint64_t sequenceNumber{0};
    std::string                value;
    serialization::le_int64_t  timestamp{0};

    if (auto error = reader.read_string(key)
                         .read_endian_integer(sequenceNumber)
                         .read_string(value)
                         .read_endian_integer(timestamp);
        error.has_error())
    {
        spdlog::error(
            "Unable to reader a record from a segment file. Error={}, path={}",
            magic_enum::enum_name(reader.error().value()),
            get_path().string()
        );
        return std::nullopt;
    }

    memtable_t::record_t record{
        memtable_t::record_t::key_t{std::move(key)},
        memtable_t::record_t::value_t{std::move(value)},
        sequenceNumber.get(),
        memtable_t::record_t::timestamp_t::from_representation(timestamp.get())
    };

    return std::make_optional(std::move(record));
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

    auto file{fs::random_access_file::random_access_file_builder_t{}.build(
        get_path(), posix_wrapper::open_flag_k::kReadWrite | posix_wrapper::open_flag_k::kCreate

    )};
    if (!file.has_value())
    {
        spdlog::critical(
            "Failed to open the file. Error={}, path={}", file.error().message, get_path().string()
        );
    }

    if (const auto result = file->write(
            std::string_view(
                reinterpret_cast<const char *>(bufferOpt.value().data()), bufferOpt.value().size()
            ),
            0
        );
        !result.has_value())
    {
        spdlog::critical(
            "Failed to write serialized binary data into the file. Error={}, path={}",
            result.error().message,
            get_path().string()
        );
        return;
    }
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
    ASSERT(m_memtable.has_value());
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

void regular_segment_t::restore_index()
{
    // Open file
    auto file{fs::random_access_file::random_access_file_builder_t{}.build(
        get_path(), posix_wrapper::open_flag_k::kReadOnly
    )};
    if (!file.has_value())
    {
        spdlog::critical(
            "Failed to open the segment file. Error={}, path={}",
            file.error().message,
            get_path().string()
        );
    }

    // Get file size
    const auto expectedFileSize = file->size();
    if (!expectedFileSize.has_value())
    {
        spdlog::critical("Failed to get segment file size. Path={}", get_path().string());
        return;
    }
    const auto fileSize{expectedFileSize.value()};

    // Allocate the buffer
    // TODO(lnikon): May be optimized using mmap
    // TODO(lnikon): Allocate enough bytes to read the header. Don't read entire file, as we need to
    //               restore the index only
    std::vector<std::byte> buffer(fileSize);

    // Read file into the buffer
    const auto readResult{file->read(0, reinterpret_cast<char *>(&buffer.front()), fileSize)};
    if (!readResult.has_value())
    {
        spdlog::error(
            "Failed to read segment file. Error={}, path={}",
            readResult.error().message,
            get_path().string()
        );
        return;
    }

    if (readResult.value() != fileSize)
    {
        spdlog::error(
            "Failed to read segment file. Expected size={}, read size={}, path={}",
            fileSize,
            readResult.value(),
            get_path().string()
        );
        return;
    }

    // Read the header
    struct header_t final
    {
        serialization::le_uint32_t magic{0};
        serialization::le_uint32_t version{0};
        serialization::le_uint32_t bodyOffset{0};
        serialization::le_uint32_t indexOffset{0};
        serialization::le_uint32_t size{0};
        serialization::le_uint32_t crc32{0};
    } header;

    serialization::buffer_reader_t reader{buffer};
    if (auto error = reader.read_endian_integer(header.magic)
                         .read_endian_integer(header.version)
                         .read_endian_integer(header.bodyOffset)
                         .read_endian_integer(header.indexOffset)
                         .read_endian_integer(header.size)
                         .read_endian_integer(header.crc32)
                         .error();
        error.has_value())
    {
        spdlog::error(
            "Failed to read segment header. Error={}, path={}",
            magic_enum::enum_name(error.value()),
            get_path().string()
        );
        return;
    }

    // Validate segment magic number
    if (header.magic.get() != SEGMENT_HEADER_MAGIC_NUMBER)
    {
        spdlog::error(
            "Segment header magic mismatch. Expected={}, got={}, path={}",
            SEGMENT_HEADER_MAGIC_NUMBER,
            header.magic.get(),
            get_path().string()
        );
        return;
    }

    // Validate segment version number
    if (header.version.get() != SEGMENT_VERSION_NUMBER)
    {
        spdlog::error(
            "Segment version mismatch. Expected={}, got={}, path={}",
            SEGMENT_VERSION_NUMBER,
            header.version.get(),
            get_path().string()
        );
        return;
    }

    // Validate segment size
    if (header.size.get() != fileSize)
    {
        spdlog::error(
            "Segment size mismatch. Expected={}, got={}, path={}",
            fileSize,
            header.size.get(),
            get_path().string()
        );
        return;
    }

    // Validate CRC32
    const auto calculatedCrc{
        serialization::crc32_t{}
            .update(
                std::span(buffer.data(), calculate_header_size_bytes() - sizeof(std::uint32_t))

            )
            .update(
                std::span(
                    buffer.data() + header.bodyOffset.get(), buffer.size() - header.bodyOffset.get()
                )
            )
            .finalize()
    };
    if (calculatedCrc != header.crc32.get())
    {
        spdlog::error(
            "Segment checksum mismatch. Expected={}, got={}, path={}",
            header.crc32.get(),
            calculatedCrc,
            get_path().string()
        );
        return;
    }

    // Read the index
    serialization::buffer_reader_t indexReader{std::span(
        buffer.data() + header.indexOffset.get(), buffer.size() - header.indexOffset.get()
    )};
    while (!indexReader.eof())
    {
        std::string                key;
        serialization::le_uint32_t entryOffset{0};

        if (auto error = indexReader.read_string(key).read_endian_integer(entryOffset).error();
            error.has_value())
        {
            spdlog::error(
                "Failed to read segment index. Error={}, path={}",
                magic_enum::enum_name(error.value()),
                get_path().string()
            );
            return;
        }
        m_hashIndex.emplace(std::move(key), entryOffset.get());
    }

    if (m_hashIndex.empty())
    {
        spdlog::warn("(restore_index): Hash index is empty for segment {}", m_path.c_str());
        ASSERT(!m_hashIndex.empty());
    }
}

} // namespace structures::lsmtree::segments::regular_segment

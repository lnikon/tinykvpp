#pragma once

#include <structures/memtable/memtable.h>

namespace wal
{

using kv_t = structures::memtable::memtable_t::record_t;

enum class operation_k : int8_t
{
    undefined_k = -1,
    add_k,
    delete_k,
};

enum class log_storage_type_k : int8_t
{
    undefined_k = -1,
    in_memory_k,
    persistent_k
};

auto to_string(log_storage_type_k type) -> std::string
{
    if (type == log_storage_type_k::in_memory_k)
    {
        return std::string{"inMemory"};
    }

    if (type == log_storage_type_k::persistent_k)
    {
        return std::string{"persistent"};
    }

    return std::string{"undefined"};
}

auto from_string(const std::string &type) -> log_storage_type_k
{
    if (type == "inMemory")
    {
        return log_storage_type_k::in_memory_k;
    }

    if (type == "persistent")
    {
        return log_storage_type_k::persistent_k;
    }

    return log_storage_type_k::undefined_k;
}

struct record_t
{
    operation_k op{operation_k::undefined_k};
    kv_t        kv;

    template <typename TStream> void write(TStream &stream) const
    {
        // Write operation opcode
        stream << static_cast<std::int32_t>(op) << ' ';

        // Write key-value pair
        kv.write(stream);
    }

    template <typename TStream> void read(TStream &stream)
    {
        // Read operation opcode
        int32_t opInt{0};
        stream >> opInt;
        op = static_cast<operation_k>(opInt);

        // Read key-value pair
        kv.read(stream);
    }
};

} // namespace wal

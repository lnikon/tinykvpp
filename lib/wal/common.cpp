//
// Created by nikon on 3/8/25.
//

#include "common.h"

namespace wal
{

auto to_string(log_storage_type_k type) -> std::string
{
    if (type == log_storage_type_k::in_memory_k)
    {
        return std::string{"inMemory"};
    }

    if (type == log_storage_type_k::file_based_persistent_k)
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
        return log_storage_type_k::file_based_persistent_k;
    }

    return log_storage_type_k::undefined_k;
}

}
#pragma once

#include <optional>
#include <string>

namespace wal::log
{

template <typename T>
concept TStorageBackendConcept =
    requires(T backend, const char *data, std::size_t offset, std::size_t size) {
        { backend.write(data, offset, size) } -> std::same_as<bool>;
        { backend.read(offset, size) } -> std::same_as<std::string>;
        { backend.size() } -> std::same_as<std::size_t>;
        { backend.reset() } -> std::same_as<bool>;
    };

template <typename T, typename Entry = std::string>
concept TLogStorageConcept = requires(const T     const_storage,
                                      T           storage,
                                      Entry       entry,
                                      std::string command,
                                      std::string key,
                                      std::string value,
                                      std::size_t index) {
    { storage.append(entry) } -> std::convertible_to<bool>;
    { storage.append(command, key, value) } -> std::convertible_to<bool>;
    { const_storage.read(index) } -> std::convertible_to<std::optional<std::string>>;
    { storage.reset() } -> std::convertible_to<bool>;
    { const_storage.size() } -> std::same_as<std::size_t>;
};

template <typename T, typename Entry = std::string>
concept TLogConcept = requires(const T     const_log,
                               T           log,
                               Entry       entry,
                               std::string command,
                               std::string key,
                               std::string value,
                               std::size_t index) {
    { log.append(entry) } -> std::convertible_to<bool>;
    { log.append(command, key, value) } -> std::convertible_to<bool>;
    { const_log.read(index) } -> std::same_as<std::optional<std::string>>;
    { log.reset() } -> std::convertible_to<bool>;
    { const_log.size() } -> std::same_as<std::size_t>;
};

} // namespace wal::log

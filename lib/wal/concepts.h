#pragma once

#include <optional>
#include <string>
#include <vector>

template <typename T>
concept TStorageBackendConcept = requires(T backend, const std::byte *data, std::size_t offset, std::size_t size) {
    { backend.write(data, offset, size) } -> std::same_as<bool>;
    { backend.read(offset, size) } -> std::same_as<std::vector<std::byte>>;
    { backend.size() } -> std::same_as<std::size_t>;
    { backend.reset() } -> std::same_as<bool>;
};

template <typename T, typename Entry = std::string>
concept TLogStorageConcept = requires(const T const_storage, T storage, Entry entry, std::size_t index) {
    { storage.append(entry) } -> std::same_as<void>;
    { const_storage.read(index) } -> std::convertible_to<std::optional<std::string>>;
    { storage.reset() } -> std::convertible_to<bool>;
};

template <typename T, typename Entry = std::string>
concept TLogConcept = requires(const T const_log, T log, Entry entry, std::size_t index) {
    { log.append(entry) } -> std::same_as<void>;
    { const_log.read(index) } -> std::convertible_to<std::optional<std::string>>;
    { log.reset() } -> std::convertible_to<bool>;
};

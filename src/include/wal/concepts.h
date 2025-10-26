#pragma once

#include <optional>
#include <string>

namespace wal
{

template <typename T>
concept TStorageBackendConcept =
    requires(T backend, const char *data, ssize_t offset, std::size_t size, std::size_t n) {
        { backend.write(data, offset, size) } -> std::same_as<bool>;
        { backend.read(offset, size) } -> std::same_as<std::string>;
        { backend.size() } -> std::same_as<std::size_t>;
        { backend.reset() } -> std::same_as<bool>;
        { backend.reset_last_n(n) } -> std::same_as<bool>;
    };

// A common interface shared between all log storages.
template <template <typename...> class TStorage, typename... Ts>
concept TLogStorageConcept = requires(
    const TStorage<Ts...>                  const_storage,
    TStorage<Ts...>                        storage,
    typename TStorage<Ts...>::entry_type_t entry,
    std::size_t                            index,
    std::size_t                            n
) {
    {
        const_storage.read(index)
    } -> std::convertible_to<std::optional<typename TStorage<Ts...>::entry_type_t>>;
    { storage.reset() } -> std::convertible_to<bool>;
    { const_storage.size() } -> std::same_as<std::size_t>;
    { storage.append(entry) } -> std::convertible_to<bool>;
    { storage.reset_last_n(n) } -> std::same_as<bool>;
};

// Log is a type-erased wrapper around different storage types,
// so it makes sense to force the log to have the same interface as its storage.
template <template <typename> class TStorage, typename TEntry = std::string>
concept TLogConcept = TLogStorageConcept<TStorage, TEntry>;

} // namespace wal

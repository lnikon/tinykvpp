#pragma once

#include "concepts.h"
#include "in_memory_log_storage.h"
#include "persistent_log_storage.h"

namespace wal::log
{

namespace storage_tags
{

struct in_memory_tag
{
};

struct file_backend_tag
{
};

struct object_backend_tag
{
};

} // namespace storage_tags

// It is possible to parametrize TLogStorageConcept with the Entity
// type e.g. TLogStorageConcept<std::string>. See the definitions of
// TLogStorageConcept.
template <TLogStorageConcept TStorage> class log_t
{
  public:
    log_t() = delete;

    explicit log_t(TStorage storage) noexcept

        : m_storage(std::move(storage))
    {
    }

    log_t(log_t &&other) noexcept
        : m_storage{std::move(other.m_storage)}
    {
    }

    auto operator=(log_t &&other) noexcept -> log_t &
    {
        using std::swap;
        swap(*this, other);
        return *this;
    }

    log_t(const log_t &) noexcept = delete;
    auto operator=(const log_t &) noexcept -> log_t & = delete;

    ~log_t() noexcept = default;

    [[nodiscard]] auto append(std::string entry) noexcept -> bool
    {
        return m_storage.append(std::move(entry));
    }

    [[nodiscard]] auto append(std::string command, std::string key, std::string value) noexcept
        -> bool
    {
        return m_storage.append(command, key, value);
    }

    [[nodiscard]] auto read(std::size_t index) const noexcept -> std::optional<std::string>
    {
        return m_storage.read(index);
    }

    [[nodiscard]] auto reset() noexcept -> bool
    {
        return m_storage.reset();
    }

    [[nodiscard]] auto size() const noexcept -> std::size_t
    {
        return m_storage.size();
    }

  private:
    TStorage m_storage;
};

using log_variant_t = std::variant<log_t<in_memory_log_storage_t>,
                                   log_t<persistent_log_storage_t<file_storage_backend_t>>>;

static_assert(TLogConcept<log_t<in_memory_log_storage_t>>,
              "log_t<in_memory_log_storage_t> should satisfy TLogConcept");
static_assert(TLogConcept<log_t<persistent_log_storage_t<file_storage_backend_t>>>,
              "log_t<file_storage_backend_t> should satisfy TLogConcept");

template <class... T> constexpr bool always_false = false;

// Primary template (intentionally undefined)
template <typename TagT> struct storage_type_mapping;

// Specializations for each tag type
template <> struct storage_type_mapping<storage_tags::in_memory_tag>
{
    using type = in_memory_log_storage_t;
};

template <> struct storage_type_mapping<storage_tags::file_backend_tag>
{
    using type = persistent_log_storage_t<file_storage_backend_t>;
};

// Helper alias template for cleaner usage
template <typename TagT> using storage_type_for = typename storage_type_mapping<TagT>::type;

template <typename TStorageTag> class log_builder_t
{
  public:
    log_builder_t() = default;

    template <typename T = TStorageTag>
        requires std::is_same_v<T, storage_tags::file_backend_tag>
    auto set_file_path(fs::path_t path) -> log_builder_t &
    {
        m_file_path = std::move(path);
        return *this;
    }

    template <typename T = TStorageTag>
        requires std::is_same_v<T, storage_tags::object_backend_tag>
    auto set_url(std::string url) -> log_builder_t &
    {
        m_url = std::move(url);
        return *this;
    }

    [[nodiscard]] auto build() const -> std::optional<log_t<storage_type_for<TStorageTag>>>
    {
        if constexpr (std::is_same_v<TStorageTag, storage_tags::in_memory_tag>)
        {
            return log_t{in_memory_log_storage_t{}};
        }
        else if constexpr (std::is_same_v<TStorageTag, storage_tags::file_backend_tag>)
        {
            auto &&storage =
                persistent_log_storage_builder_t<file_storage_backend_t>{{.file_path = m_file_path}}
                    .build();
            return storage.has_value() ? std::make_optional(log_t{std::move(storage.value())})
                                       : std::nullopt;
        }
        else
        {
            static_assert(always_false<TStorageTag>, "Unsupported storage tag type");
        }

        return std::nullopt;
    }

  private:
    fs::path_t  m_file_path;
    std::string m_url;
};

} // namespace wal::log

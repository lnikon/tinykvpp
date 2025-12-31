#pragma once

#include <cassert>
#include <optional>
#include <string>
#include <sys/types.h>
#include <chrono>

#include <structures/sorted_vector/sorted_vector.h>
#include <structures/skiplist/skiplist.h>

namespace structures::memtable
{

class memtable_t
{
  public:
    struct record_t
    {
        using sequence_number_t = std::uint64_t;

        struct key_t
        {
            using storage_type_t = std::string;

            explicit key_t(storage_type_t key) noexcept;
            key_t() noexcept = default;

            auto operator<(const key_t &other) const -> bool;
            auto operator>(const key_t &other) const -> bool;
            auto operator==(const key_t &other) const -> bool;

            [[nodiscard]] auto size() const -> std::size_t;

            storage_type_t m_key;
        };

        struct value_t
        {
            using storage_type_t = std::string;

            explicit value_t(storage_type_t value) noexcept;
            value_t() noexcept = default;

            auto operator==(const value_t &other) const -> bool;

            [[nodiscard]] auto size() const -> std::size_t;

            storage_type_t m_value;
        };

        struct timestamp_t
        {
            // Used to differentiate between keys with the same timestamp
            using clock_t = std::chrono::system_clock;
            using precision_t = std::chrono::nanoseconds;
            using time_point_t = std::chrono::time_point<clock_t, precision_t>;

            timestamp_t() noexcept;
            explicit timestamp_t(time_point_t timePoint) noexcept;

            static auto from_representation(precision_t::rep rep) noexcept -> timestamp_t;

            time_point_t m_value;
        };

        // TODO(lnikon): Get rid of default ctor
        record_t() = default;
        record_t(
            key_t             key,
            value_t           value,
            sequence_number_t sequenceNumber,
            timestamp_t       timestamp = timestamp_t{}
        ) noexcept;

        auto operator<(const record_t &record) const -> bool;
        auto operator>(const record_t &record) const -> bool;
        auto operator==(const record_t &record) const -> bool;

        [[nodiscard]] auto size() const -> std::size_t;

        key_t             m_key;
        value_t           m_value;
        sequence_number_t m_sequenceNumber{0};
        timestamp_t       m_timestamp;
    };

    struct record_comparator_by_key_t
    {
        auto operator()(const record_t &lhs, const record_t &rhs) -> bool
        {
            return lhs.m_key < rhs.m_key;
        }
    };

    // using storage_t = typename sorted_vector::sorted_vector_t<record_t,
    // record_comparator_by_key_t>;
    using storage_t = typename skiplist::skiplist_t<record_t, record_comparator_by_key_t>;
    using size_type = typename storage_t::size_type;
    using index_type = typename storage_t::index_type;
    using iterator = typename storage_t::iterator;
    using const_iterator = typename storage_t::const_iterator;
    using value_type = typename storage_t::value_type;

    void emplace(record_t record);

    [[nodiscard]] auto find(const record_t::key_t &key) const noexcept -> std::optional<record_t>;

    [[nodiscard]] auto size() const -> std::size_t;
    [[nodiscard]] auto num_of_bytes_used() const -> std::size_t;

    [[nodiscard]] auto count() const -> std::size_t;
    [[nodiscard]] auto empty() const -> bool;

    [[nodiscard]] auto begin() const -> typename storage_t::const_iterator;
    [[nodiscard]] auto end() const -> typename storage_t::const_iterator;

    [[nodiscard]] auto min() const noexcept -> std::optional<record_t::key_t>;
    [[nodiscard]] auto max() const noexcept -> std::optional<record_t::key_t>;

    [[nodiscard]] auto moved_records() -> std::vector<memtable_t::record_t>;

    auto operator<(const memtable_t &other) const -> bool;

  private:
    storage_t   m_data;
    std::size_t m_size{0};
    std::size_t m_count{0};
    std::size_t m_num_of_bytes{0};
};

} // namespace structures::memtable

template <> struct std::hash<structures::memtable::memtable_t::record_t::key_t>
{
    using key_t = structures::memtable::memtable_t::record_t::key_t;
    auto operator()(const key_t &key) const -> std::size_t
    {
        return std::hash<std::string>{}(key.m_key);
    }
};

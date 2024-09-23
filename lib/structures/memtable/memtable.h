//
// Created by nikon on 1/21/22.
//

#ifndef MEMTABLE_H
#define MEMTABLE_H

#include <cassert>
#include <structures/sorted_vector/sorted_vector.h>
#include <structures/skiplist/skiplist.h>

#include <optional>
#include <string>
#include <sys/types.h>
#include <chrono>
#include <iostream>
#include <csignal>

namespace structures::memtable
{

template <class> inline constexpr bool always_false_v = false;

// class memtable_t
class memtable_t
{
  public:
    struct record_t
    {
        struct key_t
        {
            using storage_type_t = std::string;

            explicit key_t(storage_type_t key);

            key_t() = default;
            key_t(const key_t &other) = default;
            auto operator=(const key_t &other) -> key_t & = default;
            key_t(key_t &&other) = default;
            auto operator=(key_t &&other) -> key_t & = default;
            ~key_t() = default;

            auto operator<(const key_t &other) const -> bool;
            auto operator>(const key_t &other) const -> bool;
            auto operator==(const key_t &other) const -> bool;

            template <typename stream_gt> void write(stream_gt &outStream) const;
            template <typename stream_gt> void read(stream_gt &outStream);

            [[nodiscard]] auto size() const -> std::size_t;

            static void swap(key_t &lhs, key_t &rhs) noexcept;

            storage_type_t m_key;
        };

        struct value_t
        {
            using storage_type_t = std::string;

            explicit value_t(storage_type_t value);

            value_t() = default;
            value_t(const value_t &other) = default;
            auto operator=(const value_t &other) -> value_t & = default;
            value_t(value_t &&other) = default;
            auto operator=(value_t &&other) -> value_t & = default;
            ~value_t() = default;

            auto operator==(const value_t &other) const -> bool;

            template <typename stream_gt> void write(stream_gt &outStream) const;
            template <typename stream_gt> void read(stream_gt &outStream);

            [[nodiscard]] auto size() const -> std::size_t;

            static void swap(value_t &lhs, value_t &rhs);

            storage_type_t m_value;
        };

        struct timestamp_t
        {
            // Used to differentiate between keys with the same timestamp
            using clock_t = std::chrono::high_resolution_clock;
            using underlying_value_type_t = std::chrono::time_point<clock_t>;

            timestamp_t();

            template <typename stream_gt> void write(stream_gt &outStream) const;
            template <typename stream_gt> void read(stream_gt &outStream);

            auto operator<(const timestamp_t &other) const -> bool;

            static void swap(timestamp_t &lhs, timestamp_t &rhs);

            underlying_value_type_t m_value;
        };

        record_t(key_t key, value_t value);

        record_t() = default;
        record_t(const record_t &other);
        auto operator=(const record_t &other) -> record_t &;
        record_t(record_t &&other) = default;
        auto operator=(record_t &&other) -> record_t & = default;
        ~record_t() = default;

        auto operator<(const record_t &record) const -> bool;
        auto operator>(const record_t &record) const -> bool;
        auto operator==(const record_t &record) const -> bool;

        [[nodiscard]] auto size() const -> std::size_t;

        template <typename stream_gt> void write(stream_gt &outStream) const;
        template <typename stream_gt> void read(stream_gt &outStream);

        key_t m_key;
        value_t m_value;
        timestamp_t m_timestamp;
    };

    struct record_comparator_by_key_t
    {
        auto operator()(const record_t &lhs, const record_t &rhs) -> bool
        {
            return lhs.m_key < rhs.m_key;
        }
    };

    // using storage_t = typename sorted_vector::sorted_vector_t<record_t, record_comparator_by_key_t>;
    using storage_t = typename skiplist::skiplist_t<record_t, record_comparator_by_key_t>;
    using size_type = typename storage_t::size_type;
    using index_type = typename storage_t::index_type;
    using iterator = typename storage_t::iterator;
    using const_iterator = typename storage_t::const_iterator;
    using value_type = typename storage_t::value_type;

    memtable_t() = default;
    memtable_t(const memtable_t &) = default;
    auto operator=(const memtable_t &) -> memtable_t & = default;
    memtable_t(memtable_t &&) = default;
    auto operator=(memtable_t &&) -> memtable_t & = default;
    ~memtable_t() = default;

    /**
     * @brief
     *
     * @param record
     */
    void emplace(record_t record);

    /**
     * @brief
     *
     * @param key
     */
    auto find(const record_t::key_t &key) -> std::optional<record_t>;

    /**
     * @brief
     */
    [[nodiscard]] auto size() const -> std::size_t;

    /**
     * @brief
     */
    [[nodiscard]] auto num_of_bytes_used() const -> std::size_t;

    /**
     * @brief
     */
    [[nodiscard]] auto count() const -> std::size_t;

    /**
     * @brief
     */
    [[nodiscard]] auto empty() const -> bool;

    /**
     * @brief
     */
    [[nodiscard]] auto begin() const -> typename storage_t::const_iterator;

    /**
     * @brief
     */
    [[nodiscard]] auto end() const -> typename storage_t::const_iterator;

    /**
     * @brief
     */
    [[nodiscard]] auto min() const noexcept -> std::optional<record_t::key_t>;

    /**
     * @brief
     */
    [[nodiscard]] auto max() const noexcept -> std::optional<record_t::key_t>;

    /**
     * @brief
     */
    [[nodiscard]] auto moved_records() -> std::vector<memtable_t::record_t>;

    /**
     * @brief
     *
     * @param other
     * @return
     */
    auto operator<(const memtable_t &other) const -> bool;

    template <typename stream_gt> void write(stream_gt &outStream) const;
    template <typename stream_gt> void read(stream_gt &outStream);

  private:
    storage_t m_data;
    std::size_t m_size{0};
    std::size_t m_count{0};
    std::size_t m_num_of_bytes{0};
};

// ------------------------------------------------
// memtable_t::record_t::key_t
// ------------------------------------------------
template <typename stream_gt> void memtable_t::record_t::key_t::write(stream_gt &outStream) const
{
    auto size = m_key.size();
    outStream << m_key.size() << ' ' << m_key;
}

template <typename stream_gt> void memtable_t::record_t::key_t::read(stream_gt &outStream)
{
    std::size_t size{0};
    outStream >> size;
    m_key.resize(size);
    outStream >> m_key;
}

// ------------------------------------------------
// memtable_t::record_t::value_t
// ------------------------------------------------
template <typename stream_gt> void memtable_t::record_t::value_t::write(stream_gt &outStream) const
{
    outStream << size() << ' ' << m_value;
}

template <typename stream_gt> void memtable_t::record_t::value_t::read(stream_gt &outStream)
{
    std::size_t size{0};
    outStream >> size;
    m_value.reserve(size);
    outStream >> m_value;
}

// ------------------------------------------------
// memtable_t::record_t::timestamp_t
// ------------------------------------------------
template <typename stream_gt> void memtable_t::record_t::timestamp_t::write(stream_gt &outStream) const
{
    outStream << m_value.time_since_epoch().count();
}

template <typename stream_gt> void memtable_t::record_t::timestamp_t::read(stream_gt &outStream)
{
    clock_t::rep count = 0;
    outStream >> count;
    m_value = clock_t::time_point{clock_t::duration{count}};
}

// ------------------------------------------------
// memtable_t::record_t
// ------------------------------------------------
template <typename stream_gt> void memtable_t::record_t::write(stream_gt &outStream) const
{
    m_key.write(outStream);
    outStream << ' ';
    m_value.write(outStream);
    outStream << ' ';
    m_timestamp.write(outStream);
}

template <typename stream_gt> void memtable_t::record_t::read(stream_gt &outStream)
{
    m_key.read(outStream);
    m_value.read(outStream);
    m_timestamp.read(outStream);
}

template <typename stream_gt> void memtable_t::write(stream_gt &outStream) const
{
    for (auto rec : *this)
    {
        rec.write(outStream);
        outStream << std::endl;
    }
}

template <typename stream_gt> void memtable_t::read(stream_gt &outStream)
{
    // TODO(lnikon): Implement deserialization
}

} // namespace structures::memtable

template <> struct std::hash<structures::memtable::memtable_t::record_t::key_t>
{
    using key_t = structures::memtable::memtable_t::record_t::key_t;
    auto operator()(const key_t &key) const -> std::size_t
    {
        return std::hash<std::string>{}(key.m_key);
    }
};

#endif // MEMTABLE_H

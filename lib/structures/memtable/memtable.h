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
#include <utility>
#include <variant>
#include <chrono>
#include <iostream>

namespace structures::memtable
{

template <class> inline constexpr bool always_false_v = false;

std::size_t string_size_in_bytes(const std::string &str);

// class memtable_t
class memtable_t
{
  public:
    struct record_t
    {
        // TODO(vahag): Introduce 'buffer' type, an uninterpreted array of bytes
        enum class record_value_type_t
        {
            integer_k = 0,
            double_k,
            string_k
        };

        struct key_t
        {
            using storage_type_t = std::string;

            explicit key_t(std::string key);

            key_t() = default;
            key_t(const key_t &other) = default;
            key_t &operator=(const key_t &other) = default;

            bool operator<(const key_t &other) const;
            bool operator>(const key_t &other) const;
            bool operator==(const key_t &other) const;

            template <typename stream_gt> void write(stream_gt &os) const;
            template <typename stream_gt> void read(stream_gt &os);

            [[nodiscard]] std::size_t size() const;

            static void swap(key_t &lhs, key_t &rhs);

            storage_type_t m_key;
        };

        struct value_t
        {
            using underlying_value_type_t = std::variant<int64_t, double, std::string>;

            explicit value_t(underlying_value_type_t value);

            value_t() = default;
            value_t(const value_t &other) = default;
            value_t &operator=(const value_t &other) = default;

            bool operator==(const value_t &other) const;

            template <typename stream_gt> void write(stream_gt &os) const;
            template <typename stream_gt> void read(stream_gt &os);

            [[nodiscard]] std::size_t size() const;

            static void swap(value_t &lhs, value_t &rhs);

            underlying_value_type_t m_value;
        };

        struct timestamp_t
        {
            // Used to differentiate between keys with the same timestamp
            using clock_t = std::chrono::high_resolution_clock;
            using underlying_value_type_t = std::chrono::time_point<clock_t>;

            timestamp_t();

            template <typename stream_gt> void write(stream_gt &os) const;
            template <typename stream_gt> void read(stream_gt &os);

            bool operator<(const timestamp_t &other) const;

            static void swap(timestamp_t &lhs, timestamp_t &rhs);

            underlying_value_type_t m_value;
        };

        record_t() = default;
        record_t(const record_t &other);
        record_t(const key_t &key, const value_t &value);
        record_t &operator=(const record_t &other);

        bool operator<(const record_t &record) const;
        bool operator>(const record_t &record) const;
        bool operator==(const record_t &record) const;

        [[nodiscard]] std::size_t size() const;

        template <typename stream_gt> void write(stream_gt &os) const;
        template <typename stream_gt> void read(stream_gt &os);

        key_t m_key;
        value_t m_value;
        timestamp_t m_timestamp;
    };

    struct record_comparator_by_key_t
    {
        bool operator()(const record_t &lhs, const record_t &rhs)
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
    // using reverse_iterator = typename storage_t::reverse_iterator;
    using value_type = typename storage_t::value_type;

    memtable_t() = default;
    memtable_t(const memtable_t &) = delete;
    memtable_t &operator=(const memtable_t &) = delete;
    memtable_t(memtable_t &&) = default;
    memtable_t &operator=(memtable_t &&) = default;

    /**
     * @brief
     *
     * @param record
     */
    void emplace(const record_t &record);

    /**
     * @brief
     *
     * @param key
     */
    std::optional<record_t> find(const record_t::key_t &key);

    /**
     * @brief
     */
    [[nodiscard]] std::size_t size() const;

    /**
     * @brief
     */
    [[nodiscard]] std::size_t count() const;

    /**
     * @brief
     */
    [[nodiscard]] bool empty() const;

    /**
     * @brief
     *
     * @param pMemtable
     */
    void merge(memtable_t pMemtable) noexcept;

    /**
     * @brief
     */
    typename storage_t::const_iterator begin() const;

    /**
     * @brief
     */
    typename storage_t::const_iterator end() const;

    /**
     * @brief
     */
    [[nodiscard]] std::optional<record_t::key_t> min() const noexcept;

    /**
     * @brief
     */
    [[nodiscard]] std::optional<record_t::key_t> max() const noexcept;

    /**
     * @brief
     *
     * @param other
     * @return
     */
    bool operator<(const memtable_t &other);

    template <typename stream_gt> void write(stream_gt &os) const;
    template <typename stream_gt> void read(stream_gt &os);

  private:
    /**
     * @brief
     *
     * @param record
     */
    void update_size(const record_t &record);

  private:
    storage_t m_data;
    std::size_t m_size{0};
    std::size_t m_count{0};
};

// static_assert(std::ranges::random_access_range<memtable_t>);

template <typename stream_gt> void memtable_t::record_t::key_t::write(stream_gt &os) const
{
    os << m_key.size() << ' ' << m_key << ' ';
}

template <typename stream_gt> void memtable_t::record_t::key_t::read(stream_gt &os)
{
    std::size_t size{0};
    os >> size;
    m_key.resize(size);
    os >> m_key;
}

template <typename stream_gt> void memtable_t::record_t::value_t::write(stream_gt &os) const
{
    std::visit(
        [&os, this](auto &&value)
        {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, int64_t>)
            {
                os << std::to_underlying(record_t::record_value_type_t::integer_k) << ' ' << value << ' ';
            }
            else if constexpr (std::is_same_v<T, double>)
            {
                os << std::to_underlying(record_t::record_value_type_t::double_k) << ' ' << value << ' ';
            }
            else if constexpr (std::is_same_v<T, std::string>)
            {
                os << std::to_underlying(record_t::record_value_type_t::string_k) << ' ' << size() << ' ' << value
                   << ' ';
            }
            else
            {
                static_assert(always_false_v<T>, "non-exhaustive visitor");
            }
        },
        m_value);
}

template <typename stream_gt> void memtable_t::record_t::value_t::read(stream_gt &os)
{
    std::size_t tag;
    os >> tag;

    if (tag == std::to_underlying(record_t::record_value_type_t::integer_k))
    {
        std::int64_t value;
        os >> value;
        m_value.emplace<std::int64_t>(value);
    }
    else if (tag == std::to_underlying(record_t::record_value_type_t::double_k))
    {
        double value;
        os >> value;
        m_value.emplace<double>(value);
    }
    else if (tag == std::to_underlying(record_t::record_value_type_t::string_k))
    {
        std::size_t size{0};
        os >> size;
        std::string value;
        value.resize(size);
        os >> value;
        m_value.emplace<std::string>(std::move(value));
    }
    else
    {
        assert("non-exhaustive if");
    }
}

template <typename stream_gt> void memtable_t::record_t::timestamp_t::write(stream_gt &os) const
{
    os << m_value.time_since_epoch().count() << ' ';
}

template <typename stream_gt> void memtable_t::record_t::timestamp_t::read(stream_gt &os)
{
    clock_t::rep count;
    os >> count;
    m_value = clock_t::time_point{clock_t::duration{count}};
}

template <typename stream_gt> void memtable_t::record_t::write(stream_gt &os) const
{
    m_key.write(os);
    m_value.write(os);
    m_timestamp.write(os);
    os << std::endl;
}

template <typename stream_gt> void memtable_t::record_t::read(stream_gt &os)
{
    m_key.read(os);
    m_value.read(os);
    m_timestamp.read(os);
}

template <typename stream_gt> void memtable_t::write(stream_gt &os) const
{
    for (auto rec : *this)
    {
        rec.write(os);
        os << std::endl;
    }
}

template <typename stream_gt> void memtable_t::read(stream_gt &os)
{
}

} // namespace structures::memtable

template <> struct std::hash<structures::memtable::memtable_t::record_t::key_t>
{
    using S = structures::memtable::memtable_t::record_t::key_t;
    std::size_t operator()(const S &s) const
    {
        return std::hash<std::string>{}(s.m_key);
    }
};

#endif // MEMTABLE_H

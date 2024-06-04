//
// Created by nikon on 1/21/22.
//

#ifndef MEMTABLE_H
#define MEMTABLE_H

#include <structures/sorted_vector/sorted_vector.h>

#include <boost/date_time.hpp>

#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <variant>

namespace structures::memtable
{
using namespace sorted_vector;
// TODO: Maybe instead of supporting any K/V type, we can support fixed set of
// types?
// TODO: Use `concepts` to describe interface of template parameters.
// TODO: MemTable should be safe for concurrent access and *scalable*. Consider
// to use SkipList! template <template<typename> class StorageType>

std::size_t string_size_in_bytes(const std::string &str);

// class memtable_t;
// using unique_ptr_t = std::unique_ptr<memtable_t>;
//
// template <typename... Args> auto make_unique(Args... args)
// {
//     return std::make_unique<memtable_t>(std::forward<args>...);
// }

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

        // TODO(lnikon): Consider to make this move-onl
        struct key_t
        {
            using storage_type_t = std::string;

            explicit key_t(std::string key);

            key_t(const key_t &other) = default;
            key_t &operator=(const key_t &other) = default;

            bool operator<(const key_t &other) const;
            bool operator>(const key_t &other) const;
            bool operator==(const key_t &other) const;

            void write(std::stringstream &os) const;
            [[nodiscard]] std::size_t size() const;

            static void swap(key_t &lhs, key_t &rhs);

            storage_type_t m_key;
        };

        struct value_t
        {
            using underlying_value_type_t = std::variant<int64_t, double, std::string>;

            explicit value_t(underlying_value_type_t value);

            value_t(const value_t &other) = default;
            value_t &operator=(const value_t &other) = default;

            bool operator==(const value_t &other) const;

            void write(std::stringstream &os) const;
            [[nodiscard]] std::size_t size() const;

            static void swap(value_t &lhs, value_t &rhs);

            underlying_value_type_t m_value;
        };

        record_t(const record_t &other);
        record_t(const key_t &key, const value_t &value);
        record_t &operator=(const record_t &other);

        bool operator<(const record_t &record) const;
        bool operator>(const record_t &record) const;
        bool operator==(const record_t &record) const;

        friend auto operator<=>(const record_t &, const record_t &);

        [[nodiscard]] std::size_t size() const;

        friend std::ostream &operator<<(std::ostream &out, const record_t &r);

        key_t m_key;
        value_t m_value;
    };

    using storage_t = sorted_vector_t<record_t>;
    using size_type = typename storage_t::size_type;
    using index_type = typename storage_t::index_type;
    using iterator = typename storage_t::iterator;
    using const_iterator = typename storage_t::const_iterator;
    using reverse_iterator = typename storage_t::reverse_iterator;
    using value_type = typename storage_t::value_type;

    memtable_t() = default;
    memtable_t(const memtable_t &) = delete;
    memtable_t &operator=(const memtable_t &) = delete;
    memtable_t(memtable_t &&) = default;
    memtable_t &operator=(memtable_t &&) = default;

    void emplace(const record_t &record);
    std::optional<record_t> find(const record_t::key_t &key);
    [[nodiscard]] std::size_t size() const;
    [[nodiscard]] std::size_t count() const;

    void merge(memtable_t pMemtable) noexcept;

    typename storage_t::iterator begin();
    typename storage_t::iterator end();

    void write(std::stringstream &ss);

    [[nodiscard]] std::optional<record_t::key_t> min() const noexcept;
    [[nodiscard]] std::optional<record_t::key_t> max() const noexcept;

    bool operator<(const memtable_t& other);

  private:
    void update_size(const record_t &record);

  private:
    storage_t m_data;
    std::size_t m_size{0};
    std::size_t m_count{0};
};

static_assert(std::ranges::random_access_range<memtable_t>);

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

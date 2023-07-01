//
// Created by nikon on 1/21/22.
//

#ifndef CPP_PROJECT_TEMPLATE_MEMTABLE_H
#define CPP_PROJECT_TEMPLATE_MEMTABLE_H

#include <utility>
#include <vector>
#include <string>
#include <variant>
#include <optional>
#include <mutex>
#include <algorithm>
#include <sstream>

#include <boost/date_time.hpp>

#define FMT_HEADER_ONLY
#include <spdlog/spdlog.h>

namespace structures::memtable {
    // TODO: Maybe instead of supporting any K/V type, we can support fixed set of types?
    // TODO: Use `concepts` to describe interface of template parameters.
    // TODO: MemTable should be safe for concurrent access and *scalable*. Consider to use SkipList!
    //       template <template<typename> class StorageType>

    std::size_t StringSizeInBytes(const std::string &str);

    class MemTable {
    public:
        struct Record {
            enum class ValueType {
                Integer = 0,
                Double,
                String
            };

            struct Key {
                explicit Key(std::string key);

                Key(const Key &other);
                Key &operator=(const Key &other);
                // TODO: Impl move assign operator

                bool operator<(const Key &other) const;
                bool operator>(const Key &other) const;
                bool operator==(const Key &other) const;

                void Write(std::stringstream& os) const;
                [[nodiscard]] std::size_t Size() const;

                static void swap(Key &lhs, Key &rhs);

                std::string m_key;
            };

            struct Value {
                using UnderlyingValueType = std::variant<int64_t, double, std::string>;

                explicit Value(UnderlyingValueType value);

                Value(const Value &other);
                Value &operator=(const Value &other);
                // TODO: Impl move assign operator

                bool operator==(const Value &other) const;

                void Write(std::stringstream& os);
                [[nodiscard]] std::optional<std::size_t> Size() const;

                static void swap(Value &lhs, Value &rhs);

                UnderlyingValueType m_value;
            };

            Record(const Record &other);
            Record(const Key &key, const Value &value);

            bool operator<(const Record &record) const;
            bool operator>(const Record &record) const;
            Record &operator=(const Record &other);

            [[nodiscard]] Key GetKey() const;
            [[nodiscard]] Value GetValue() const;
            [[nodiscard]] std::size_t Size() const;

        private:
            Key m_key;
            Value m_value;
        };

        MemTable() = default;
        MemTable(const MemTable &) = delete;
        MemTable &operator=(const MemTable &) = delete;
        MemTable(MemTable &&) = delete;
        MemTable &operator=(MemTable &&) = delete;

        void Emplace(const Record &record);
        std::optional<Record> Find(const Record::Key &key);
        [[nodiscard]] std::size_t Size() const;
        [[nodiscard]] std::size_t Count() const;

        // TODO: Implement iterators to use for dumping
        auto Begin();
        auto End();

        void Write(std::stringstream& ss);

    private:
        void updateSize(const Record &record);

    private:
        std::mutex m_mutex;
        // TODO: Should abstract out this part to some generic storage with good O(n) times.
        std::vector<Record> m_data;
        std::size_t m_size{0};
        std::size_t m_count{0};
    };

    using MemTableUniquePtr = std::unique_ptr<MemTable>;

    template <typename... Args>
    auto make_unique(Args... args)
    {
        return std::make_unique<MemTable>(std::forward<args>...);
    }
}

#endif //CPP_PROJECT_TEMPLATE_MEMTABLE_H

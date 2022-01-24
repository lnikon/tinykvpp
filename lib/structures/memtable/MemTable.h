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
#include <iostream>

#include <boost/date_time.hpp>

namespace structure::memtable {
        // TODO: Maybe instead of supporting any K/V type, we can support fixed set of types?
        // TODO: Use `concepts` to describe interface of template parameters.
        // TODO: MemTable should be safe for concurrent access and *scalable*. Consider to use SkipList!
//        template <template<typename> class StorageType>

        std::size_t StringSizeInBytes(const std::string& str)
        {
            return sizeof(std::string::value_type) * str.size();
        }

        class MemTable {
        public:
            struct Record
            {
                enum class ValueType
                {
                    Integer = 0,
                    Double,
                    String
                };

                struct Key
                {
                    explicit Key(std::string key)
                        : m_key(std::move(key))
                    { }

                    Key(const Key& other)
                        : m_key(other.m_key)
                    { }

                    Key& operator=(const Key& other)
                    {
                        if (this == &other)
                        {
                            return *this;
                        }

                        Key tmp(other);
                        swap(*this, tmp);

                        return *this;
                    }

                    static void swap(Key& lhs, Key& rhs)
                    {
                        using std::swap;
                        std::swap(lhs.m_key, rhs.m_key);
                    }

                    // TODO: Impl move assign operator

                    [[nodiscard]] std::size_t Size() const
                    {
                        return StringSizeInBytes(m_key);
                    }

                    bool operator<(const Key& other) const
                    {
                        return m_key < other.m_key;
                    }

                    bool operator>(const Key& other) const
                    {
                        return !(*this < other);
                    }

                    bool operator==(const Key& other) const
                    {
                        return m_key == other.m_key;
                    }

                    std::string m_key;
                };

                struct Value
                {
                    using UnderlyingValueType = std::variant<int64_t, double, std::string>;

                    explicit Value(UnderlyingValueType  value)
                        : m_value(std::move(value))
                    { }

                    Value(const Value& other)
                        : m_value(other.m_value)
                    { }

                    Value& operator=(const Value& other)
                    {
                        if (this == &other)
                        {
                            return *this;
                        }

                        Value tmp(other);
                        std::swap(m_value, tmp.m_value);

                        return *this;
                    }

                    // TODO: Impl move assign operator

                    [[nodiscard]] std::size_t Size() const
                    {
                        return std::visit([](const UnderlyingValueType& value) {
                            if (value.index() == static_cast<std::size_t>(ValueType::Integer))
                            {
                                return sizeof(int64_t);
                            }
                            else if (value.index() == static_cast<std::size_t>(ValueType::Double))
                            {
                                return sizeof(double);
                            }
                            else if (value.index() == static_cast<std::size_t>(ValueType::String))
                            {
                                return StringSizeInBytes(std::get<std::string>(value));
                            }
                            else
                            {
                                // TODO: Use spdlog here!
                                assert(false);
                            }
                        }, m_value);
                    }

                    bool operator==(const Value& other) const
                    {
                        return m_value == other.m_value;
                    }

                    static void swap(Value& lhs, Value& rhs)
                    {
                        using std::swap;
                        swap(lhs.m_value, rhs.m_value);
                    }

                    UnderlyingValueType m_value;
                };

                Record(const Key& key, const Value& value)
                    : m_key(key)
                    , m_value(value)
                { }

                Record(const Record& other)
                    : m_key(other.m_key)
                    , m_value(other.m_value)
                { }

                Record& operator=(const Record& other)
                {
                    if (this == &other)
                    {
                        return *this;
                    }

                    Record tmp(other);
                    Record::Key::swap(m_key, tmp.m_key);
                    Record::Value::swap(m_value, tmp.m_value);

                    return *this;
                }

                bool operator<(const Record& record) const
                {
                    return m_key < record.m_key;
                }

                bool operator>(const Record& record) const
                {
                    return !(m_key < record.m_key);
                }

                [[nodiscard]] Key GetKey() const
                {
                    return m_key;
                }

                [[nodiscard]] Value GetValue() const
                {
                    return m_value;
                }

                [[nodiscard]] std::size_t Size() const
                {
                    return m_key.Size() + m_value.Size();
                }

            private:
                Key m_key;
                Value m_value;
            };

            MemTable() = default;
            MemTable(const MemTable&) = delete;
            MemTable& operator=(const MemTable&) = delete;
            MemTable(MemTable&&) = delete;
            MemTable& operator=(MemTable&&) = delete;

            void emplace(const Record& record)
            {
                std::lock_guard lg(m_mutex);
                m_data.insert(std::lower_bound(m_data.begin(), m_data.end(), record), record);

                updateSize(record);
                m_count++;
            }

            std::optional<Record> find(const Record::Key& key)
            {
                Record record{key, Record::Value{""}};

                decltype(m_data)::iterator it;
                {
                    std::lock_guard lg(m_mutex);
                    it = std::lower_bound(m_data.begin(), m_data.end(), record);
                }

                return (it->GetKey() == key ? std::make_optional(*it) : std::nullopt);
            }

            [[nodiscard]] std::size_t Size() const
            {
                return m_size;
            }

            [[nodiscard]] std::size_t Count() const
            {
                return m_count;
            }

        private:
            void updateSize(const Record& record)
            {
                m_size += record.Size();
            }

        private:
            std::mutex m_mutex;
            std::vector<Record> m_data;
            std::size_t m_size{0};
            std::size_t m_count{0};
        };
    }

#endif //CPP_PROJECT_TEMPLATE_MEMTABLE_H

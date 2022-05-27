//
// Created by nikon on 1/21/22.
//

#include "MemTable.h"

namespace structures::memtable {
    std::size_t StringSizeInBytes(const std::string &str) {
        return sizeof(std::string::value_type) * str.size();
    }

    MemTable::Record::Key::Key(std::string key)
            : m_key(std::move(key)) {}

    MemTable::Record::Key::Key(const MemTable::Record::Key &other)
            : m_key(other.m_key) {}

    MemTable::Record::Key &MemTable::Record::Key::operator=(const MemTable::Record::Key &other) {
        if (this == &other) {
            return *this;
        }

        Key tmp(other);
        swap(*this, tmp);

        return *this;
    }

    void MemTable::Record::Key::swap(MemTable::Record::Key &lhs, MemTable::Record::Key &rhs) {
        using std::swap;
        std::swap(lhs.m_key, rhs.m_key);
    }

    std::size_t MemTable::Record::Key::Size() const {
        return StringSizeInBytes(m_key);
    }

    bool MemTable::Record::Key::operator<(const MemTable::Record::Key &other) const {
        return m_key < other.m_key;
    }

    bool MemTable::Record::Key::operator>(const MemTable::Record::Key &other) const {
        return !(*this < other);
    }

    bool MemTable::Record::Key::operator==(const MemTable::Record::Key &other) const {
        return m_key == other.m_key;
    }

    void MemTable::Record::Key::Write(std::ostream &os) const {
        os << m_key;
    }

    MemTable::Record::Value::Value(MemTable::Record::Value::UnderlyingValueType value)
            : m_value(std::move(value)) {}

    MemTable::Record::Value::Value(const MemTable::Record::Value &other)
            : m_value(other.m_value) {}

    MemTable::Record::Value &MemTable::Record::Value::operator=(const MemTable::Record::Value &other) {
        if (this == &other) {
            return *this;
        }

        Value tmp(other);
        std::swap(m_value, tmp.m_value);

        return *this;
    }

    std::optional<std::size_t> MemTable::Record::Value::Size() const {
        return std::visit([](const UnderlyingValueType &value) {
            if (value.index() == static_cast<std::size_t>(ValueType::Integer)) {
                return std::make_optional<std::size_t>(sizeof(int64_t));
            } else if (value.index() == static_cast<std::size_t>(ValueType::Double)) {
                return std::make_optional<std::size_t>(sizeof(double));
            } else if (value.index() == static_cast<std::size_t>(ValueType::String)) {
                return std::make_optional<std::size_t>(StringSizeInBytes(std::get<std::string>(value)));
            } else {
                spdlog::warn("Unsupported value type with value index=" + std::to_string(value.index()));
                return std::optional<std::size_t>(std::nullopt);
            }

        }, m_value);
    }

    bool MemTable::Record::Value::operator==(const MemTable::Record::Value &other) const {
        return m_value == other.m_value;
    }

    void MemTable::Record::Value::swap(MemTable::Record::Value &lhs, MemTable::Record::Value &rhs) {
        using std::swap;
        swap(lhs.m_value, rhs.m_value);
    }

    void MemTable::Record::Value::Write(std::ostream &os) {
        std::visit([&os](const UnderlyingValueType &value) {
            if (value.index() == static_cast<std::size_t>(ValueType::Integer)) {
                os << std::get<static_cast<std::size_t>(ValueType::Integer)>(value);
            } else if (value.index() == static_cast<std::size_t>(ValueType::Double)) {
                os << std::get<static_cast<std::size_t>(ValueType::Double)>(value);
            } else if (value.index() == static_cast<std::size_t>(ValueType::String)) {
                os << std::get<static_cast<std::size_t>(ValueType::String)>(value);
            } else {
                spdlog::warn("Unsupported value type with value index=" + std::to_string(value.index()));
            }
        }, m_value);
    }

    MemTable::Record::Record(const MemTable::Record::Key &key, const MemTable::Record::Value &value)
            : m_key(key), m_value(value) {}

    MemTable::Record::Record(const MemTable::Record &other)
            : m_key(other.m_key), m_value(other.m_value) {}

    MemTable::Record &MemTable::Record::operator=(const MemTable::Record &other) {
        if (this == &other) {
            return *this;
        }

        Record tmp(other);
        Record::Key::swap(m_key, tmp.m_key);
        Record::Value::swap(m_value, tmp.m_value);

        return *this;
    }

    bool MemTable::Record::operator<(const MemTable::Record &record) const {
        return m_key < record.m_key;
    }

    bool MemTable::Record::operator>(const MemTable::Record &record) const {
        return !(m_key < record.m_key);
    }

    MemTable::Record::Key MemTable::Record::GetKey() const {
        return m_key;
    }

    MemTable::Record::Value MemTable::Record::GetValue() const {
        return m_value;
    }

    std::size_t MemTable::Record::Size() const {
        const auto valueSizeOpt = m_value.Size();
        if (!valueSizeOpt.has_value()) {
            spdlog::warn("Value has null size!");
        }

        return m_key.Size() + valueSizeOpt.value_or(0);
    }

    void MemTable::Emplace(const MemTable::Record &record) {
        std::lock_guard lg(m_mutex);
        m_data.insert(std::lower_bound(m_data.begin(), m_data.end(), record), record);

        updateSize(record);
        m_count++;
    }

    std::optional<MemTable::Record> MemTable::Find(const MemTable::Record::Key &key) {
        Record record{key, Record::Value{""}};

        std::lock_guard lg(m_mutex);
        decltype(m_data)::iterator it = std::lower_bound(m_data.begin(), m_data.end(), record);

        return (it->GetKey() == key ? std::make_optional(*it) : std::nullopt);
    }

    std::size_t MemTable::Size() const {
        return m_size;
    }

    std::size_t MemTable::Count() const {
        return m_count;
    }

    auto MemTable::Begin() {
        return m_data.begin();
    }

    auto MemTable::End() {
        return m_data.end();
    }

    void MemTable::Write(std::ostream &os) {
        std::lock_guard lg(m_mutex);
        if (m_count == 0)
        {
            return;
        }

        os << m_count << '\n';
        for (const auto& record: m_data)
        {
            record.GetKey().Write(os);
            os << ' ';
            record.GetValue().Write(os);
            os << '\n';
        }
    }

    void MemTable::updateSize(const MemTable::Record &record) {
        m_size += record.Size();
    }
}

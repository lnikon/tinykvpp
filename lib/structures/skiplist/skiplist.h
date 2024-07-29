#pragma once

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <vector>
#include <memory>

namespace structures::skiplist
{

const std::int64_t max_height = 12;

struct record_t
{
    using key_t = std::string;
    using value_t = std::string;

    key_t key;
    value_t value;
};

struct node_t
{
    node_t(record_t rec, std::int64_t level)
        : record(rec),
          forward(level + 1, nullptr)
    {
    }

    record_t record;
    // std::vector<std::shared_ptr<node_t>> forward;
    std::vector<node_t *> forward;
};

class skiplist_t
{
  public:
    std::optional<record_t> find(const record_t::key_t &key)
    {
        node_t *current{m_head};
        for (std::int64_t i = m_level; i >= 0; i--)
        {
            while (current->forward[i] && current->forward[i]->record.key < key)
            {
                current = current->forward[i];
            }
        }

        current = current->forward[0];
        if (current != nullptr && current->record.key == key)
        {
            return current->record;
        }

        return std::nullopt;
    }

    void insert(record_t record)
    {
        const auto newLevel{random_level()};
        if (newLevel > m_level)
        {
            m_head->forward.resize(newLevel + 1);
            m_level = newLevel;
        }

        std::vector<node_t *> to_be_updated(m_level + 1, nullptr);

        auto current{m_head};
        for (std::int64_t i{m_level}; i >= 0; i--)
        {
            while (current->forward[i] && current->forward[i]->record.key < record.key)
            {
                current = current->forward[i];
            }
            to_be_updated[i] = current;
        }

        current = current->forward[0];
        if (current == nullptr || current->record.key != record.key)
        {
            node_t *newNode = new node_t(record, m_level);
            for (std::int64_t i{0}; i <= newLevel; i++)
            {
                newNode->forward[i] = to_be_updated[i]->forward[i];
                to_be_updated[i]->forward[i] = newNode;
            }

            m_size++;
        }
        else
        {
            std::cout << record.key << " already exists\n";
        }
    }

    std::size_t size() const noexcept
    {
        return m_size;
    }

  private:
    std::int64_t random_level() const
    {
        std::int64_t level{0};
        std::mt19937 rg{std::random_device{}()};
        while (std::uniform_int_distribution<std::int64_t>(0, 1024)(rg) % 2 == 0)
        {
            level += 1;
        }
        return level;
    }

  private:
    node_t *m_head{new node_t(record_t{}, max_height)};
    std::int64_t m_level{0};
    std::size_t m_size{0};
};

} // namespace structures::skiplist
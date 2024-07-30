#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <vector>
#include <ranges>

namespace structures::skiplist
{

const std::int64_t max_height = 12;

template <typename record_gt, typename comparator_gt>
class skiplist_t : public std::ranges::view_interface<skiplist_t<record_gt, comparator_gt>>
{
  public:
    template <typename T, typename Pointer, typename Reference> struct iterator_base;

    struct node_t
    {
        node_t(record_gt rec, std::int64_t level)
            : record(rec),
              forward(level + 1, nullptr)
        {
        }

        record_gt record;
        std::vector<node_t *> forward;
    };

    using value_type = record_gt;
    using reference = record_gt &;
    using const_reference = const record_gt &;
    using difference_type = std::ptrdiff_t;
    using size_type = std::size_t;
    using index_type = std::size_t;
    using iterator = iterator_base<value_type, value_type *, value_type &>;
    using const_iterator = iterator_base<value_type, const value_type *, const value_type &>;

    template <typename T, typename Pointer, typename Reference> struct iterator_base
    {
        using iterator_category = std::forward_iterator_tag;
        using difference_type = typename skiplist_t::difference_type;
        using value_type = T;
        using pointer = Pointer;
        using reference = Reference;

        iterator_base(node_t *node)
            : m_node{node}
        {
        }

        reference operator*() const
        {
            return m_node->record;
        }

        pointer operator->()
        {
            return &m_node->record;
        }

        iterator_base &operator++()
        {
            m_node = m_node->forward[0];
            return *this;
        }

        iterator_base operator++(int)
        {
            iterator_base tmp = *this;
            ++(*this);
            return tmp;
        }

        friend bool operator==(const iterator_base &a, const iterator_base &b)
        {
            return a.m_node == b.m_node;
        };

        friend bool operator!=(const iterator_base &a, const iterator_base &b)
        {
            return a.m_node != b.m_node;
        };

      private:
        node_t *m_node;
    };

    iterator begin()
    {
        return iterator(m_head->forward[0]);
    }

    iterator end()
    {
        return iterator(nullptr);
    }

    const_iterator cbegin()
    {
        return const_iterator(m_head->forward[0]);
    }

    const_iterator cend()
    {
        return const_iterator(nullptr);
    }

    // iterator begin() const
    // {
    //     return iterator(m_head->forward[0]);
    // }

    // iterator end() const
    // {
    //     return iterator(nullptr);
    // }

    const_iterator cbegin() const
    {
        return const_iterator(m_head->forward[0]);
    }

    const_iterator cend() const
    {
        return const_iterator(nullptr);
    }

    std::optional<record_gt> find(const record_gt::key_t &key)
    {
        node_t *current{m_head};
        for (std::int64_t i = m_level; i >= 0; i--)
        {
            while (current->forward[i] && current->forward[i]->record.m_key < key)
            {
                current = current->forward[i];
            }
        }

        current = current->forward[0];
        if (current != nullptr && current->record.m_key == key)
        {
            return current->record;
        }

        return std::nullopt;
    }

    void emplace(record_gt record)
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
            while (current->forward[i] && current->forward[i]->record.m_key < record.m_key)
            {
                current = current->forward[i];
            }
            to_be_updated[i] = current;
        }

        current = current->forward[0];
        if (current == nullptr || current->record.m_key != record.m_key)
        {
            node_t *newNode = new node_t(record, m_level);
            for (std::int64_t i{0}; i <= newLevel; i++)
            {
                newNode->forward[i] = to_be_updated[i]->forward[i];
                to_be_updated[i]->forward[i] = newNode;
            }

            m_size++;
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
    node_t *m_head{new node_t(record_gt{}, max_height)};
    std::int64_t m_level{0};
    std::size_t m_size{0};
};

} // namespace structures::skiplist
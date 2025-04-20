#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <utility>
#include <vector>
#include <memory>

namespace structures::skiplist
{

const std::int64_t max_height = 12;

template <typename record_gt, typename comparator_gt> class skiplist_t
{
  public:
    template <typename T, typename Pointer, typename Reference> struct iterator_base;

    struct node_t;
    using node_shared_ptr_t = std::shared_ptr<node_t>;

    struct node_t
    {
        node_t(record_gt rec, std::int64_t level)
            : record(std::move(rec)),
              forward(level + 1, nullptr)
        {
        }

        record_gt                      record;
        std::vector<node_shared_ptr_t> forward;
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

        explicit iterator_base(node_shared_ptr_t node)
            : m_node{node}
        {
        }

        auto operator*() const -> reference
        {
            return m_node->record;
        }

        auto operator->() -> pointer
        {
            return &m_node->record;
        }

        auto operator++() -> iterator_base &
        {
            m_node = m_node->forward[0];
            return *this;
        }

        auto operator++(int) -> iterator_base
        {
            iterator_base tmp = *this;
            ++(*this);
            return tmp;
        }

        friend auto operator==(const iterator_base &lhs, const iterator_base &rhs) -> bool
        {
            return lhs.m_node == rhs.m_node;
        };

        friend auto operator!=(const iterator_base &lhs, const iterator_base &rhs) -> bool
        {
            return lhs.m_node != rhs.m_node;
        };

      private:
        node_shared_ptr_t m_node;
    };

    auto begin() -> iterator
    {
        return iterator(m_head->forward[0]);
    }

    auto end() -> iterator
    {
        return iterator(nullptr);
    }

    auto cbegin() -> const_iterator
    {
        return const_iterator(m_head->forward[0]);
    }

    auto cend() -> const_iterator
    {
        return const_iterator(nullptr);
    }

    [[nodiscard]] auto cbegin() const -> const_iterator
    {
        return const_iterator(m_head->forward[0]);
    }

    [[nodiscard]] auto cend() const -> const_iterator
    {
        return const_iterator(nullptr);
    }

    auto find(const typename record_gt::key_t &key) const noexcept -> std::optional<record_gt>
    {
        node_shared_ptr_t current{m_head};
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

    void emplace(record_gt &&record)
    {
        const auto newLevel{random_level()};
        if (newLevel > m_level)
        {
            m_head->forward.resize(newLevel + 1);
            m_level = newLevel;
        }

        std::vector<node_shared_ptr_t> to_be_updated(m_level + 1, nullptr);

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
            // Insert new node
            node_shared_ptr_t newNode =
                std::make_shared<node_t>(std::forward<record_gt>(record), m_level);
            for (std::int64_t i{0}; i <= newLevel; i++)
            {
                newNode->forward[i] = to_be_updated[i]->forward[i];
                to_be_updated[i]->forward[i] = newNode;
            }

            m_size++;
        }
        else if (current->record.m_key == record.m_key)
        {
            // Update value of the existing node
            current->record = std::forward<record_gt>(record);
        }
    }

    [[nodiscard]] auto size() const noexcept -> std::size_t
    {
        return m_size;
    }

  private:
    [[nodiscard]] auto random_level() const -> std::int64_t
    {
        constexpr const int min = 0;
        constexpr const int max = 1024;

        std::int64_t level{0};
        std::mt19937 rng{std::random_device{}()};
        while (std::uniform_int_distribution<std::int64_t>(min, max)(rng) % 2 == 0)
        {
            level += 1;
        }
        return level;
    }

    node_shared_ptr_t m_head = std::make_shared<node_t>(record_gt{}, max_height);
    std::int64_t      m_level{0};
    std::size_t       m_size{0};
};

} // namespace structures::skiplist

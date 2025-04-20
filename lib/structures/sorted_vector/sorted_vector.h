#ifndef SORTED_VECTOR_H
#define SORTED_VECTOR_H

#include <algorithm>
#include <optional>
#include <vector>

namespace structures::sorted_vector
{

// TODO: Support custom allocator
template <typename Data, typename Comparator = std::less<Data>> class sorted_vector_t
{
  public:
    using size_type = typename std::vector<Data>::size_type;
    using index_type = size_type;
    using iterator = typename std::vector<Data>::iterator;
    using const_iterator = typename std::vector<Data>::const_iterator;
    using reverse_iterator = typename std::vector<Data>::reverse_iterator;
    using value_type = typename iterator::value_type;

    sorted_vector_t() = default;
    sorted_vector_t(sorted_vector_t &) = default;
    sorted_vector_t(sorted_vector_t &&) = default;
    auto operator=(const sorted_vector_t &) -> sorted_vector_t & = default;
    auto operator=(sorted_vector_t &&) -> sorted_vector_t & = default;
    ~sorted_vector_t() = default;

    void emplace(Data data, Comparator comparator = Comparator());
    auto find(Data data, Comparator comparator = Comparator()) const -> std::optional<Data>;

    [[nodiscard]] auto size() const -> size_type;
    auto               at(index_type index) -> Data &;

    auto begin() -> iterator;
    auto end() -> iterator;

    [[nodiscard]] auto begin() const -> const_iterator;
    [[nodiscard]] auto end() const -> const_iterator;

    auto rbegin() -> reverse_iterator;
    auto rend() -> reverse_iterator;

    [[nodiscard]] auto cbegin() const -> const_iterator;
    [[nodiscard]] auto cend() const -> const_iterator;

    auto erase(iterator begin, iterator end) -> iterator;

    [[nodiscard]] auto back() const noexcept -> Data;

    void clear() noexcept;

  private:
    std::vector<Data> m_data;
};

template <typename Data, typename Comparator>
auto sorted_vector_t<Data, Comparator>::back() const noexcept -> Data
{
    return m_data.back();
}

template <typename Data, typename Comparator>
void sorted_vector_t<Data, Comparator>::clear() noexcept
{
    m_data.clear();
}

template <typename Data, typename Comparator>
void sorted_vector_t<Data, Comparator>::emplace(Data data, Comparator comparator)
{
    m_data.insert(std::lower_bound(m_data.begin(), m_data.end(), data, comparator), data);
}

template <typename Data, typename Comparator>
auto sorted_vector_t<Data, Comparator>::find(Data data, Comparator comparator) const
    -> std::optional<Data>
{
    auto       dataIt = std::lower_bound(m_data.begin(), m_data.end(), data, comparator);
    const bool found = (dataIt != m_data.end()) && !comparator(data, *dataIt);
    return found ? std::make_optional(*dataIt) : std::nullopt;
    ;
}

template <typename Data, typename Comparator>
auto sorted_vector_t<Data, Comparator>::size() const ->
    typename sorted_vector_t<Data, Comparator>::size_type
{
    return m_data.size();
}

template <typename Data, typename Comparator>
auto sorted_vector_t<Data, Comparator>::at(const index_type index) -> Data &
{
    return m_data[index];
}

template <typename Data, typename Comparator>
auto sorted_vector_t<Data, Comparator>::begin() ->
    typename sorted_vector_t<Data, Comparator>::iterator
{
    return m_data.begin();
}

template <typename Data, typename Comparator>
auto sorted_vector_t<Data, Comparator>::end() ->
    typename sorted_vector_t<Data, Comparator>::iterator
{
    return m_data.end();
}

template <typename Data, typename Comparator>
auto sorted_vector_t<Data, Comparator>::begin() const ->
    typename sorted_vector_t<Data, Comparator>::const_iterator
{
    return m_data.cbegin();
}

template <typename Data, typename Comparator>
auto sorted_vector_t<Data, Comparator>::end() const ->
    typename sorted_vector_t<Data, Comparator>::const_iterator
{
    return m_data.cend();
}

template <typename Data, typename Comparator>
auto sorted_vector_t<Data, Comparator>::rbegin() ->
    typename sorted_vector_t<Data, Comparator>::reverse_iterator
{
    return m_data.rbegin();
}

template <typename Data, typename Comparator>
auto sorted_vector_t<Data, Comparator>::rend() ->
    typename sorted_vector_t<Data, Comparator>::reverse_iterator
{
    return m_data.rend();
}

template <typename Data, typename Comparator>
auto sorted_vector_t<Data, Comparator>::cbegin() const ->
    typename sorted_vector_t<Data, Comparator>::const_iterator
{
    return m_data.cbegin();
}

template <typename Data, typename Comparator>
auto sorted_vector_t<Data, Comparator>::cend() const ->
    typename sorted_vector_t<Data, Comparator>::const_iterator
{
    return m_data.cend();
}

template <typename Data, typename Comparator>
auto sorted_vector_t<Data, Comparator>::erase(iterator begin, iterator end) ->
    typename sorted_vector_t<Data, Comparator>::iterator
{
    return m_data.erase(begin, end);
}

} // namespace structures::sorted_vector

#endif // SORTED_VECTOR_H

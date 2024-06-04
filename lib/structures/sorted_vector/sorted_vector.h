#ifndef SORTED_VECTOR_H
#define SORTED_VECTOR_H

#include <algorithm>
#include <vector>

namespace structures::sorted_vector
{

// TODO: Support custom allocator
template <typename Data, typename Comparator = std::less<Data>>
class sorted_vector_t
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
    sorted_vector_t &operator=(sorted_vector_t &) = default;
    sorted_vector_t &operator=(sorted_vector_t &&) = default;
    ~sorted_vector_t() = default;

    void emplace(Data data, Comparator comparator = Comparator());
    std::pair<bool, index_type> find(
        Data data,
        Comparator comparator = Comparator()) const;

    size_type size() const;
    Data &at(const index_type index);

    iterator begin();
    iterator end();

    reverse_iterator rbegin();
    reverse_iterator rend();

    const_iterator cbegin() const;
    const_iterator cend() const;

    iterator erase(iterator begin, iterator end);

    void clear() noexcept;

   private:
    std::vector<Data> m_data;
};

template <typename Data, typename Comparator>
void sorted_vector_t<Data, Comparator>::clear() noexcept
{
    m_data.clear();
}

template <typename Data, typename Comparator>
void sorted_vector_t<Data, Comparator>::emplace(Data data,
                                                Comparator comparator)
{
    m_data.insert(
        std::lower_bound(m_data.begin(), m_data.end(), data, comparator), data);
}

template <typename Data, typename Comparator>
std::pair<bool, typename sorted_vector_t<Data, Comparator>::index_type>
sorted_vector_t<Data, Comparator>::find(Data data, Comparator comparator) const
{
    auto it = std::lower_bound(
        std::begin(m_data), std::end(m_data), data, comparator);
    return std::make_pair(
        (!(std::begin(m_data) == std::end(m_data)) && !(comparator(data, *it))),
        std::distance(std::begin(m_data), it));
}

template <typename Data, typename Comparator>
typename sorted_vector_t<Data, Comparator>::size_type
sorted_vector_t<Data, Comparator>::size() const
{
    return m_data.size();
}

template <typename Data, typename Comparator>
Data &sorted_vector_t<Data, Comparator>::at(const index_type index)
{
    // TODO: debug assert
    return m_data[index];
}

template <typename Data, typename Comparator>
typename sorted_vector_t<Data, Comparator>::iterator
sorted_vector_t<Data, Comparator>::begin()
{
    return m_data.begin();
}

template <typename Data, typename Comparator>
typename sorted_vector_t<Data, Comparator>::iterator
sorted_vector_t<Data, Comparator>::end()
{
    return m_data.end();
}

template <typename Data, typename Comparator>
typename sorted_vector_t<Data, Comparator>::reverse_iterator
sorted_vector_t<Data, Comparator>::rbegin()
{
    return m_data.rbegin();
}

template <typename Data, typename Comparator>
typename sorted_vector_t<Data, Comparator>::reverse_iterator
sorted_vector_t<Data, Comparator>::rend()
{
    return m_data.rend();
}

template <typename Data, typename Comparator>
typename sorted_vector_t<Data, Comparator>::const_iterator
sorted_vector_t<Data, Comparator>::cbegin() const
{
    return m_data.cbegin();
}

template <typename Data, typename Comparator>
typename sorted_vector_t<Data, Comparator>::const_iterator
sorted_vector_t<Data, Comparator>::cend() const
{
    return m_data.cend();
}

template <typename Data, typename Comparator>
typename sorted_vector_t<Data, Comparator>::iterator
sorted_vector_t<Data, Comparator>::erase(iterator begin, iterator end)
{
    return m_data.erase(begin, end);
}

}  // namespace structures::sorted_vector

#endif  // SORTED_VECTOR_H

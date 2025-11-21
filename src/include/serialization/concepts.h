#pragma once

#include <bit>
#include <type_traits>

namespace serialization
{

// Forward declaration
template <typename T, std::endian> class endian_integer;

// Nothing is an endian_integer
template <typename T> struct is_endian_integer : std::false_type
{
};

// Except the endian_integer itself
template <typename T, std::endian E> struct is_endian_integer<endian_integer<T, E>> : std::true_type
{
};

template <typename T>
concept EndianInteger = is_endian_integer<T>::value;

} // namespace serialization

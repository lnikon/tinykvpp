#pragma once

#include <string>
#include <optional>

namespace wal::log::storage
{

template <typename Derived> class log_storage_crtp
{
  public:
    [[nodiscard]] auto append(std::string entry) noexcept -> bool
    {
        return static_cast<Derived &>(*this).append(std::move(entry));
    }

    [[nodiscard]] auto append(std::string command, std::string key, std::string value) noexcept
        -> bool
    {
        return static_cast<Derived &>(*this).append(
            std::move(command), std::move(key), std::move(value));
    }

    [[nodiscard]] auto read(std::size_t index) const noexcept -> std::optional<std::string>
    {
        return static_cast<const Derived &>(*this).read(index);
    }

    [[nodiscard]] auto reset() noexcept -> bool
    {
        return static_cast<Derived &>(*this).reset();
    }

    [[nodiscard]] auto size() const noexcept -> std::size_t
    {
        return static_cast<const Derived &>(*this).size();
    }
};

} // namespace wal::log::storage

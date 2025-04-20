#pragma once

#include <cstdint>
#include <optional>
namespace raft
{

class persistence_t
{
public:
  persistence_t() = delete;

  persistence_t(const persistence_t &) = delete;
  auto operator=(const persistence_t &) -> persistence_t & = delete;

  persistence_t(persistence_t &&) = delete;
  auto operator=(const persistence_t &&) -> persistence_t & = delete;

  ~persistence_t() = default;

  [[nodiscard]] auto initialize() -> bool;
  [[nodiscard]] auto update(std::optional<std::uint32_t> commitIndex,
                            std::optional<std::uint32_t> votedFor) -> bool;
  [[nodiscard]] auto flush() -> bool;
  [[nodiscard]] auto restore() -> bool;
};

} // namespace raft

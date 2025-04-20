#pragma once

#include <cstdint>
#include <string>
#include <optional>

namespace server
{

static constexpr const std::string_view GRPC_STR_VIEW = "grpc";
static constexpr const std::string_view TCP_STR_VIEW = "tcp";
static constexpr const std::string_view UNDEFINED_STR_VIEW = "undefined";

enum class communication_strategy_kind_k : uint8_t
{
    tcp_k,
    grpc_k
};

[[nodiscard]] auto to_string(const communication_strategy_kind_k kind) noexcept
    -> std::optional<std::string_view>;

[[nodiscard]] auto from_string(const std::string_view kind) noexcept
    -> std::optional<communication_strategy_kind_k>;

} // namespace server

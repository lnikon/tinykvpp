#pragma once

#include <string>

namespace server
{

static constexpr const std::string_view GRPC_STR_VIEW = "grpc";
static constexpr const std::string_view TCP_STR_VIEW = "tcp";
static constexpr const std::string_view UNDEFINED_STR_VIEW = "undefined";

enum class communication_strategy_kind_k
{
    tcp_k,
    grpc_k,
    undefined_k
};

auto to_string(const communication_strategy_kind_k kind) noexcept -> std::string;
auto from_string(const std::string_view kind) noexcept -> communication_strategy_kind_k;

} // namespace server

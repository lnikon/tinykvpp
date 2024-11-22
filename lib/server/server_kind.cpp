#include "server/server_kind.h"
#include <optional>

namespace server
{

auto to_string(const communication_strategy_kind_k kind) noexcept -> std::optional<std::string_view>
{
    switch (kind)
    {
    case communication_strategy_kind_k::grpc_k:
        return GRPC_STR_VIEW;
    case communication_strategy_kind_k::tcp_k:
        return TCP_STR_VIEW;
    default:
        return std::nullopt;
    }
}

auto from_string(const std::string_view kind) noexcept -> std::optional<communication_strategy_kind_k>
{
    if (kind == GRPC_STR_VIEW)
    {
        return communication_strategy_kind_k::grpc_k;
    }

    if (kind == TCP_STR_VIEW)
    {
        return communication_strategy_kind_k::tcp_k;
    }

    return std::nullopt;
}

} // namespace server

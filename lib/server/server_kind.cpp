#include "server/server_kind.h"
#include <optional>

namespace server
{

auto to_string(const communication_strategy_kind_k kind) noexcept -> std::optional<std::string>
{
    switch (kind)
    {
    case communication_strategy_kind_k::grpc_k:
        return std::string{GRPC_STR_VIEW.data(), GRPC_STR_VIEW.size()};
    case communication_strategy_kind_k::tcp_k:
        return std::string{TCP_STR_VIEW.data(), TCP_STR_VIEW.size()};
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

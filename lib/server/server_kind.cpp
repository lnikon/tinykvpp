#include "server/server_kind.h"

namespace server
{

auto to_string(const communication_strategy_kind_k kind) noexcept -> std::string
{
    switch (kind)
    {
    case communication_strategy_kind_k::grpc_k:
        return {GRPC_STR_VIEW.data(), GRPC_STR_VIEW.size()};
    case communication_strategy_kind_k::tcp_k:
        return {TCP_STR_VIEW.data(), TCP_STR_VIEW.size()};
    case communication_strategy_kind_k::undefined_k:
        return {UNDEFINED_STR_VIEW.data(), UNDEFINED_STR_VIEW.size()};
    }
}

auto from_string(const std::string_view kind) noexcept -> communication_strategy_kind_k
{
    if (kind == GRPC_STR_VIEW)
    {
        return communication_strategy_kind_k::grpc_k;
    }

    if (kind == TCP_STR_VIEW)
    {
        return communication_strategy_kind_k::tcp_k;
    }

    return communication_strategy_kind_k::undefined_k;
}

} // namespace server

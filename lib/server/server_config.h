#pragma once

#include <cstdint>
#include <string>

namespace server::config
{

struct server_config_t
{
    std::string host;
    uint16_t    port;
    std::string transport;
};

} // namespace server::config

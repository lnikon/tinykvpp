#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace server
{

struct server_config_t
{
    std::string              host;
    uint16_t                 port;
    std::string              transport;
    uint32_t                 id;
    std::vector<std::string> peers;
};

} // namespace server

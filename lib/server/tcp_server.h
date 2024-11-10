#pragma once

#include "server_kind.h"

#include <iostream>

class TCPServerCommunication
{
  public:
    static constexpr const auto kind = CommunicationStrategyKind::TCP;

    void start(db::db_t &db) const
    {
        std::cout << "Starting TCP Server communication..." << std::endl;
    }
};

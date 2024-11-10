#pragma once

#include "server_factory.h"
#include <utility>

template <CommunicationStrategy Strategy> class Server
{
  public:
    Server(Strategy server)
        : m_impl{std::move(server)}
    {
    }

    void start(db::db_t &db)
    {
        m_impl.start(db);
    }

  private:
    Strategy m_impl;
};

template <CommunicationStrategyKind Type> void mainServer(db::db_t &db)
{
    auto   communicationStrategy = factory<Type>();
    Server server(std::move(communicationStrategy));
    server.start(db);
}

#pragma once

#include "server_factory.h"

#include <utility>

namespace server
{

template <communication_strategy_t Strategy> class server_t
{
  public:
    server_t(Strategy server)
        : m_impl{std::move(server)}
    {
    }

    void start(db::db_t &db)
    {
        m_impl.start(db);
    }

    void shutdown()
    {
        m_impl.shutdown();
    }

  private:
    Strategy m_impl;
};

template <communication_strategy_kind_k Type> auto main_server(db::db_t &db)
{
    auto     communicationStrategy = factory<Type>();
    server_t server(std::move(communicationStrategy));
    server.start(db);
    return server;
}

} // namespace server
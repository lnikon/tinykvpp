#pragma once

#include <utility>

#include "server/server_factory.h"

namespace server
{

template <communication_strategy_t Strategy> class server_t
{
  public:
    server_t(Strategy server)
        : m_impl{std::move(server)}
    {
    }

    void start(db::shared_ptr_t &db)
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

template <communication_strategy_kind_k Type> auto main_server(db::shared_ptr_t &db)
{
    auto     communicationStrategy = factory<Type>();
    server_t server(std::move(communicationStrategy));
    server.start(db);
    return server;
}

} // namespace server

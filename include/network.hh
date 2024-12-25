#pragma once

#include <thread>
#include <vector>

#include "messages.hh"

namespace dfs
{
    class GameData;

    class Proxy {
      public:
        Proxy(int port, const GameData &game_data);
        ~Proxy();

        Proxy(const Proxy &) = delete;
        Proxy operator=(const Proxy &) = delete;

        void Run();

      private:
        void HandleConnection(int client_sock) const;

      private:
        int m_Port;
        const GameData &m_GameData;
        std::vector<std::thread> m_Clients;
        Messages m_MessageHandler;
    };
} // namespace dfs

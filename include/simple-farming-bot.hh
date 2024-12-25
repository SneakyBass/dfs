#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

#include "bot-state.hh"
#include "bot.hh"

namespace dfs
{
    class GameData;

    class SimpleFarmingBot {
      public:
        SimpleFarmingBot(std::vector<int> &&tour, const GameData &game_data, int server_sock);
        ~SimpleFarmingBot();

        SimpleFarmingBot(const SimpleFarmingBot &) = delete;
        SimpleFarmingBot operator=(const SimpleFarmingBot &) = delete;

        void Run();
        void Stop();

        BotDescriptor *GetDescriptor() const;

      private:
        void RunBot();

      private:
        std::atomic<bool> m_Running;
        std::vector<int> m_Tour;
        BotState m_BotState;
        std::unique_ptr<BotDescriptor> m_BotDescriptor;

        int32_t m_CurrentMapId;
        std::thread m_BotThread;
    };
} // namespace dfs

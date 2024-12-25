#include <bits/chrono.h>
#include <chrono>
#include <fmt/base.h>
#include <fmt/color.h>
#include <mutex>
#include <sys/socket.h>

#include "bot-state.hh"
#include "bot.hh"
#include "game.hh"
#include "map.hh"
#include "messages.hh"

namespace dfs
{
    static auto MAX_TIME = std::chrono::high_resolution_clock::now() + std::chrono::years(99);

    BotState::BotState(const GameData &game_data)
        : Data(game_data) {
    }

    BotDescriptor::BotDescriptor(BotState &state, int server_sock)
        : m_State(state)
        , m_ServerSock(server_sock) {
    }

    void BotDescriptor::MarkUpdated(const now_t &wake_at) {
        // Update entities, positions and everything
        const auto now = std::chrono::high_resolution_clock::now();

        for (auto &p : m_State.OtherPlayers)
            p.second.UpdateState(now);

        for (auto &m : m_State.Monsters)
            m.second.UpdateState(now);

        if (m_State.CurrentMap != nullptr) {
            m_State.CurrentMap->ClearOccupiedCells();

            for (auto &a : m_State.Actors) {
                if (a.second.Id == m_State.CurrentPlayer.Id)
                    continue;

                a.second.UpdateState(now);

                if (!a.second.Moving)
                    m_State.CurrentMap->MarkCellOccupied(a.second.CurrentCell);
            }
        }

        if (wake_at > now) {
            // Add the timer
            m_Timers.push_back(wake_at);
            m_Timers.sort();

            fmt::println("Next update is in {}ms", (m_Timers.front() - now) / std::chrono::milliseconds(1));
        }

        m_Condvar.notify_one();
    }

    void BotDescriptor::MoveTo(int cell_id) {
        if (m_State.CurrentMap == nullptr)
            return;

        fmt::println("Moving to {}", cell_id);

        // Get the current cell of the player
        auto current_cell = m_State.CurrentPlayer.CurrentCell;

        // Get the shortest path
        auto path = m_State.CurrentMap->GetShortestPath(current_cell, cell_id, !m_State.InCombat);

        if (path.size() == 0) {
            fmt::println("Invalid path: length is 0. Skipping.");
            return;
        }

        // Set the state of the bot
        m_State.CurrentPlayer.Moving = true;

        // The arrival time will be set by the server response. We set it to MAX_TIME to avoid sending a
        // MovementConfirmRequest directly.
        m_State.CurrentPlayer.ArrivalTime = MAX_TIME;

        // Forge the map movement request message
        auto message = Messages::ForgeMapMovementRequest(path, m_State.CurrentMap->GetId(), false);

        // Send the forged movement request to the server
        send(m_ServerSock, message.data(), message.size(), 0);

        fmt::print(fmt::fg(fmt::color::purple), " === SENT FORGED: Move Request ===\n");
    }

    void BotDescriptor::Interact(int element_id, int skill_instance_uid) {
        // Set the state of the bot
        m_State.CurrentPlayer.Collecting = true;
        m_State.CurrentPlayer.CollectingId = element_id;

        // Forge the interact request message
        auto message = Messages::ForgeInteractiveUseRequest(element_id, skill_instance_uid);

        // Send the forged request to the server
        send(m_ServerSock, message.data(), message.size(), 0);

        fmt::print(fmt::fg(fmt::color::purple), " === SENT FORGED: Interact Request ===\n");
    }

    void BotDescriptor::ChangeMap(int map_id) {
        if (m_State.CurrentMap == nullptr)
            return;

        fmt::println("Changing maps to {}", map_id);

        // Set the state of the bot
        m_State.ChangingMaps = true;

        // Forge the map change request message
        auto message = Messages::ForgeMapChangeRequest(map_id, false);

        // Send the forged request to the server
        send(m_ServerSock, message.data(), message.size(), 0);

        fmt::print(fmt::fg(fmt::color::purple), " === SENT FORGED: Map Change Request ===\n");
    }

    std::unique_lock<std::mutex> BotDescriptor::WaitForStateUpdate() {
        // Wait until we receive a message from the server or for the end of the timer we set.
        std::unique_lock<std::mutex> lock(m_Mutex);

        const auto now = std::chrono::high_resolution_clock::now();
        auto max_time = MAX_TIME;
        for (auto it = m_Timers.begin(); it != m_Timers.end(); it++) {
            // Remove timer if it is already passed
            if (*it < now)
                m_Timers.erase(it);

            max_time = *it;
            break;
        }

        m_Condvar.wait_until(lock, max_time);

        // If we are in socket mode, we need to check if the current player
        // has finished his move.
        if (m_State.Active && m_State.CurrentPlayer.Moving && m_State.CurrentPlayer.ArrivalTime <= now) {
            // Forge the map change request message
            auto message = Messages::ForgeMapMovementConfirmRequest();

            // Send the forged request to the server
            send(m_ServerSock, message.data(), message.size(), 0);

            fmt::print(fmt::fg(fmt::color::purple), " === SENT FORGED: Map Movement Confirm Request ===\n");
        }

        return lock;
    }

    void BotDescriptor::ClearState() {
        // Clear the state of the bot
        m_State.InCombat = false;

        m_State.OtherPlayers.clear();
        m_State.Monsters.clear();
        m_State.NPCs.clear();
        m_State.Actors.clear();
        m_State.Collectibles.clear();

        m_Timers.clear();
    }

    std::unique_lock<std::mutex> BotDescriptor::Lock() {
        std::unique_lock<std::mutex> lock(m_Mutex);
        return lock;
    }

    void GenericActor::UpdateState(const now_t &now) {
        if (Moving && ArrivalTime <= now) {
            CurrentCell = TargetCell;
            Moving = false;
        }
    }

    void Player::UpdateState(const now_t &now) {
        if (Collecting && ArrivalTime <= now)
            Collecting = false;

        GenericActor::UpdateState(now);
    }
} // namespace dfs

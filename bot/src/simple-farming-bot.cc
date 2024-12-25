#include <bits/chrono.h>
#include <cmath>
#include <cstdint>
#include <fmt/base.h>
#include <limits>
#include <memory>
#include <thread>
#include <unordered_set>
#include <vector>

#include "bot-state.hh"
#include "bot.hh"
#include "game.hh"
#include "map.hh"
#include "simple-farming-bot.hh"
#include "utils.hh"

namespace dfs
{
    SimpleFarmingBot::SimpleFarmingBot(std::vector<int> &&tour, const GameData &game_data, int server_sock)
        : m_Running(false)
        , m_Tour(tour)
        , m_BotState(game_data)
        , m_BotDescriptor(std::make_unique<BotDescriptor>(m_BotState, server_sock)) {
    }

    void SimpleFarmingBot::RunBot() {
        std::unordered_set<int32_t> wanted_resources;
        wanted_resources.emplace(ElementType::Nettle);
        wanted_resources.emplace(ElementType::Frene);

        while (m_Running) {
            // Wait for event
            auto lock = m_BotDescriptor->WaitForStateUpdate();

            // Check if we are still running
            if (!m_Running)
                break;

            // Ignore this event if we are not active
            if (!m_BotState.Active)
                continue;

            // We are not yet on a map
            if (m_BotState.CurrentMap == nullptr)
                continue;

            // We are currently moving. Ignore this message.
            if (m_BotState.CurrentPlayer.Moving)
                continue;

            // We are chilling
            if (m_BotState.CurrentPlayer.Collecting)
                continue;

            // We don't want to do any action while we are in the process of changing
            // maps (I don't believe that could happen tho, but better be safe than sorry)
            if (m_BotState.ChangingMaps)
                continue;

            // Get the closest (and best) collectible among the ones available
            const Collectible *best_collectible = nullptr;
            double closest_distance = std::numeric_limits<double>::max();
            int destination_cell = -1;

            // Get the list of collectibles on the map
            for (const auto &c : m_BotState.Collectibles) {
                auto &collectible = c.second;

                if (collectible.State != CollectibleState::Available)
                    continue;

                // Check if we can harverst this collectible
                if (!wanted_resources.contains(collectible.ElementTypeId))
                    continue;

                // The cell we need to get to to get this resource
                auto harvest_cell =
                    m_BotState.CurrentMap->GetCellForResource(m_BotState.CurrentPlayer.CurrentCell, collectible.CellId);

                if (harvest_cell == m_BotState.CurrentPlayer.CurrentCell) {
                    // This is the resource we want to get. We are already on the correct cell, just send the collect
                    // message
                    best_collectible = &collectible;
                    destination_cell = harvest_cell;
                    break;
                }

                // We need to get to the harevest cell to collect this
                auto harvest_cell_pos = map_tools::GetCellCordById(harvest_cell);
                auto current_pos = map_tools::GetCellCordById(m_BotState.CurrentPlayer.CurrentCell);

                // Sort collectibles by distance from us
                auto distance = std::sqrt(std::pow(current_pos.Y - harvest_cell_pos.Y, 2)
                                          + std::pow(current_pos.X - harvest_cell_pos.X, 2));

                if (distance < closest_distance) {
                    best_collectible = &collectible;
                    closest_distance = distance;
                    destination_cell = harvest_cell;
                }
            }

            if (best_collectible != nullptr) {
                if (destination_cell == m_BotState.CurrentPlayer.CurrentCell) {
                    // Harvest
                    fmt::println("We want to harvest collectible {}", best_collectible->Id);
                    if (best_collectible->EnabledSkills.size() != 1) {
                        fmt::println("Missing skill for this collectible");
                        continue;
                    }

                    m_BotDescriptor->Interact(best_collectible->Id,
                                              best_collectible->EnabledSkills[0].SkillInstanceUid);
                } else {
                    // Let's get that sweetness
                    fmt::println("Let's get the collectible {} of type {} at cell {} by moving to {}.",
                                 best_collectible->Id, best_collectible->ElementTypeId, best_collectible->CellId,
                                 destination_cell);
                    m_BotDescriptor->MoveTo(destination_cell);
                }

                continue;
            }

            // We didn't find anything to collect. Let's just bounce.
            // Get current position (in map coordinates)

            // TODO: Use the world graph to get a decent path instead of guessing using the 4 directions
            auto [x, y] = m_BotState.CurrentMap->GetCoordinates();
            auto target_map = m_Tour[(m_CurrentMapId + 1) % m_Tour.size()];

            // Get the next map
            if (m_BotState.CurrentMap->GetId() == target_map) {
                // We changed maps
                m_CurrentMapId = (m_CurrentMapId + 1) % m_Tour.size();
                target_map = m_Tour[(m_CurrentMapId + 1) % m_Tour.size()];
            }

            auto [tx, ty] = m_BotState.Data.GetMap(target_map)->GetCoordinates();

            auto current_cell = m_BotState.CurrentPlayer.CurrentCell;
            auto change_map_cell = m_BotState.CurrentMap->GetCellToMap(target_map);

            if (change_map_cell == nullptr) {
                fmt::println("There is no path to go to the next map. Stopping bot.");
                m_BotState.Active = false;
                continue;
            }

            if (current_cell == change_map_cell->Transitions[0].CellId) {
                // Send the map change command
                m_BotDescriptor->ChangeMap(change_map_cell->Transitions[0].TransitionMapId);
                continue;
            }

            fmt::println("We are on [{}, {}]. We want to go to [{}, {}]", x, y, tx, ty);

            // Let's move to this cell.
            m_BotDescriptor->MoveTo(change_map_cell->Transitions[0].CellId);
        }

        fmt::println("Bot stopped");
    }

    BotDescriptor *SimpleFarmingBot::GetDescriptor() const {
        return m_BotDescriptor.get();
    }

    void SimpleFarmingBot::Stop() {
        if (!m_Running)
            return;

        fmt::println("Stopping bot...");
        m_Running = false;
        m_BotDescriptor->MarkUpdated();
    }

    void SimpleFarmingBot::Run() {
        fmt::println("Starting simple-farming-bot");
        m_Running = true;
        m_BotState.Active = false;
        m_BotState.InCombat = false;
        m_BotState.ChangingMaps = false;
        m_BotState.CurrentPlayer.Id = 69420;
        m_BotState.CurrentPlayer.Name = "SneakySneaky";
        m_BotThread = std::thread(&SimpleFarmingBot::RunBot, this);
    }

    SimpleFarmingBot::~SimpleFarmingBot() {
        if (m_BotThread.joinable()) {
            m_BotThread.join();
        }
    }
} // namespace dfs

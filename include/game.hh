#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>

#include "utils.hh"

namespace dfs
{
    class GameMap;
    class GameMapCell;

    using MapCoordinates = std::unordered_map<uint32_t, Vec2>;

    class GameData {
      public:
        GameData() = default;
        GameData(const GameData &) = delete;
        GameData operator=(const GameData &) = delete;

        bool Initialize();

        std::unique_ptr<GameMap> GetMap(int32_t map_id) const;

      private:
        Vec2 GetCoordinates(int32_t map_id) const;

        bool ParseMapCoordinates();
        bool ParseWorldGraph();

      private:
        MapCoordinates m_MapCoordinates;
        std::unordered_map<int, std::vector<WorldGraphEdge>> m_WorldGraph;
        mutable std::unordered_map<uint32_t, std::shared_ptr<std::vector<GameMapCell>>> m_MapCells;
    };
} // namespace dfs

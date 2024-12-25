#pragma once

#include <memory>
#include <unordered_set>

#include "utils.hh"

namespace dfs
{
    struct GameMapCell
    {
        int CellId;
        int MoveZone;
        int Floor;
        int LinkedZone;
        int Speed;
        int SpecialEffects;
        int MapChangeData;
        bool Los;
        bool Visible;
        bool Mov;
        bool NonWalkableDuringFight;
        bool FarmCell;
        Vec2 Position;
    };

    class GameMap {
      public:
        GameMap(int32_t map_id, Vec2 coords, const std::shared_ptr<std::vector<GameMapCell>> &cells,
                const std::vector<WorldGraphEdge> &neighbors);
        GameMap(const GameMap &) = delete;
        GameMap operator=(const GameMap &) = delete;

        Vec2 const &GetCoordinates() const;

        std::vector<PathElement> GetShortestPath(int32_t start_cell, int32_t end_cell, bool diagonals,
                                                 bool allow_through_entity = true, bool avoid_obstacles = true) const;
        void MarkCellOccupied(int cell_id);
        void ClearOccupiedCells();
        int GetCellForResource(int current_cell, int resource_cell);

        const WorldGraphEdge *GetCellToMap(int neighbor_id) const;

        int GetId() const {
            return m_MapId;
        }

      public:
        static constexpr const int MAP_WIDTH = 14;
        static constexpr const int MAP_HEIGHT = 20;
        static constexpr const int MIN_X_COORD = 0;
        static constexpr const int MAX_X_COORD = 33;
        static constexpr const int MIN_Y_COORD = -19;
        static constexpr const int MAX_Y_COORD = 13;

      private:
        float GetPointWeight(const GameMapCell &current, bool allow_through_entity = true) const;
        float GetPointWeight(const GameMapCell &current, const GameMapCell &end,
                             bool allow_through_entity = true) const;
        bool IsEntityOnCell(int cell_id) const;
        int PointSpecialEffects(int x, int y) const;
        bool PointMov(int x, int y, bool allow_through_entity, int previous, int end,
                      bool avoid_obstacles = true) const;
        bool IsChangeZone(int cell_a, int cell_b) const;
        void PrintMap(const std::unordered_set<int> highlight) const;

      private:
        int32_t m_MapId;
        Vec2 m_Coords;
        const std::shared_ptr<std::vector<GameMapCell>> m_Cells;
        const std::vector<WorldGraphEdge> &m_Neighbors;
        std::array<bool, MAP_WIDTH * MAP_HEIGHT * 2> m_Entities;
    };

    namespace map_tools
    {
        bool IsValidCoord(int x, int y);
        Vec2 GetCellCordById(int cell_id);
        int GetCellIdXCoord(int cell_id);
        int GetCellIdYCoord(int cell_id);
        int GetDistance(int cell_a, int cell_b);
        int GetCellIdByCoord(int x, int y);
        int GetLookDirection8ExactByCoord(const Vec2 &param1, const Vec2 &param2);
        bool IsInMap(int x, int y);
    } // namespace map_tools
} // namespace dfs

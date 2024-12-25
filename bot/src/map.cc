#include <cmath>
#include <cstdint>
#include <fmt/base.h>
#include <memory>

#include "map.hh"
#include "utils.hh"

namespace dfs
{
    GameMap::GameMap(int32_t map_id, Vec2 coords, const std::shared_ptr<std::vector<GameMapCell>> &cells,
                     const std::vector<WorldGraphEdge> &neighbors)
        : m_MapId(map_id)
        , m_Coords(coords)
        , m_Cells(cells)
        , m_Neighbors(neighbors) {
        m_Entities.fill(false);
    };

    Vec2 const &GameMap::GetCoordinates() const {
        return m_Coords;
    }

    namespace map_tools
    {
        bool IsValidDirection(int direction) {
            return direction >= 0 && direction <= 7;
        }

        bool IsValidCoord(int x, int y) {
            if (y >= -x && y <= x && y <= GameMap::MAP_WIDTH + GameMap::MAX_Y_COORD - x) {
                return y >= x - (GameMap::MAP_HEIGHT - GameMap::MIN_Y_COORD);
            }

            return false;
        }

        Vec2 GetCellCordById(int cell_id) {
            int _loc2_ = cell_id / GameMap::MAP_WIDTH;
            int _loc3_ = (_loc2_ + 1) / 2;
            auto _loc4_ = _loc2_ - _loc3_;
            auto _loc5_ = cell_id - _loc2_ * GameMap::MAP_WIDTH;

            return Vec2(_loc3_ + _loc5_, _loc5_ - _loc4_);
        }

        int GetCellIdXCoord(int cell_id) {
            int _loc2_ = cell_id / GameMap::MAP_WIDTH;
            int _loc3_ = (_loc2_ + 1) / 2;
            int _loc4_ = cell_id - _loc2_ * GameMap::MAP_WIDTH;

            return _loc3_ + _loc4_;
        }

        int GetCellIdYCoord(int cell_id) {
            int _loc2_ = cell_id / GameMap::MAP_WIDTH;
            int _loc3_ = (_loc2_ + 1) / 2;
            int _loc4_ = _loc2_ - _loc3_;
            int _loc5_ = cell_id - _loc2_ * GameMap::MAP_WIDTH;

            return _loc5_ - _loc4_;
        }

        int GetDistance(int cell_a, int cell_b) {
            int _loc3_ = cell_a / GameMap::MAP_WIDTH;
            int _loc4_ = (_loc3_ + 1) / 2;
            int _loc5_ = cell_a - _loc3_ * GameMap::MAP_WIDTH;
            int _loc6_ = _loc4_ + _loc5_;
            int _loc7_ = cell_a / GameMap::MAP_WIDTH;
            int _loc8_ = (_loc7_ + 1) / 2;
            int _loc9_ = _loc7_ - _loc8_;
            int _loc10_ = cell_a - _loc7_ * GameMap::MAP_WIDTH;
            int _loc11_ = _loc10_ - _loc9_;
            int _loc12_ = cell_b / GameMap::MAP_WIDTH;
            int _loc13_ = (_loc12_ + 1) / 2;
            int _loc14_ = cell_b - _loc12_ * GameMap::MAP_WIDTH;
            int _loc15_ = _loc13_ + _loc14_;
            int _loc16_ = cell_b / GameMap::MAP_WIDTH;
            int _loc17_ = (_loc16_ + 1) / 2;
            int _loc18_ = _loc16_ - _loc17_;
            int _loc19_ = cell_b - _loc16_ * GameMap::MAP_WIDTH;
            int _loc20_ = _loc19_ - _loc18_;

            return std::abs(_loc15_ - _loc6_) + std::abs(_loc20_ - _loc11_);
        }

        int GetCellIdByCoord(int x, int y) {
            if (!IsValidCoord(x, y))
                return -1;

            return (x - y) * GameMap::MAP_WIDTH + y + (x - y) / 2;
        }

        int GetLookDirection4ExactByCoord(const Vec2 &param1, const Vec2 &param2) {
            if (!IsValidCoord(param1.X, param1.Y) || !IsValidCoord(param2.X, param2.Y)) {
                return -1;
            }
            int _loc3_ = param2.X - param1.X;
            int _loc4_ = param2.Y - param1.Y;
            if (_loc4_ == 0) {
                if (_loc3_ < 0) {
                    return 5;
                }
                return 1;
            }
            if (_loc3_ == 0) {
                if (_loc4_ < 0) {
                    return 3;
                }
                return 7;
            }
            return -1;
        }

        int GetLookDirection4DiagExactByCoord(const Vec2 &param1, const Vec2 &param2) {
            if (!IsValidCoord(param1.X, param1.Y) || !IsValidCoord(param2.X, param2.Y)) {
                return -1;
            }
            auto _loc3_ = param2.X - param1.X;
            auto _loc4_ = param2.Y - param1.Y;
            if (_loc3_ == -_loc4_) {
                if (_loc3_ < 0) {
                    return 6;
                }
                return 2;
            }

            if (_loc3_ == _loc4_) {
                if (_loc3_ < 0) {
                    return 4;
                }
                return 0;
            }

            return -1;
        }

        int GetLookDirection8ExactByCoord(const Vec2 &param1, const Vec2 &param2) {
            int _loc3_ = GetLookDirection4ExactByCoord(param1, param2);
            if (!IsValidDirection(_loc3_)) {
                _loc3_ = GetLookDirection4DiagExactByCoord(param1, param2);
            }

            return _loc3_;
        }

        bool IsInMap(int x, int y) {
            return x + y >= 0 && x - y >= 0 && x - y < GameMap::MAP_HEIGHT * 2 && x + y < GameMap::MAP_WIDTH * 2;
        }
    } // namespace map_tools

    const WorldGraphEdge *GameMap::GetCellToMap(int neighbor_id) const {
        // Check for the map `neighbor_id` in our neighboring maps.
        for (auto &edge : m_Neighbors) {
            if (edge.MapId == neighbor_id) {
                // This is the one
                if (edge.Transitions.size() != 1) {
                    // What does this mean?
                    fmt::println("Transition from {} to {} has {} steps", m_MapId, neighbor_id,
                                 edge.Transitions.size());

                    // Ignore
                    return nullptr;
                }

                return &edge;
            }
        }

        return nullptr;
    }
} // namespace dfs

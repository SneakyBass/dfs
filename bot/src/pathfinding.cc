#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fmt/base.h>
#include <fmt/color.h>
#include <limits>
#include <list>
#include <queue>
#include <unordered_set>
#include <vector>

#include "game.hh"
#include "map.hh"
#include "utils.hh"

/**
 * Horrible pathfinding source:
 * https://github.com/scalexm/DofusInvoker/blob/master/com/ankamagames/jerakine/pathfinding/Pathfinding.as
 * https://github.com/scalexm/DofusInvoker/blob/master/mapTools/MapTools.as
 * https://github.com/scalexm/DofusInvoker/blob/master/mapTools/MapToolsConfig.as
 * https://github.com/scalexm/DofusInvoker/blob/master/com/ankamagames/atouin/utils/DataMapProvider.as
 */
namespace dfs
{
    constexpr int HV_COST = 10;
    constexpr int DIAG_COST = 15;
    constexpr int HEURISTIC_COST = 10;
    constexpr int TOLERANCE_ELEVATION = 11;

    void GameMap::PrintMap(const std::unordered_set<int> highlight) const {
        auto k = 0;
        for (auto i = 0; i < GameMap::MAP_HEIGHT; i++) {
            for (auto j = 0; j < GameMap::MAP_WIDTH; j++) {
                auto color = fmt::color::gray;

                if (!m_Cells->at(k).Mov)
                    color = fmt::color::dark_red;
                else if (m_Entities[k])
                    color = fmt::color::dark_orange;
                else if (highlight.contains(k))
                    color = fmt::color::cyan;
                else if (m_Cells->at(k).MapChangeData != 0)
                    color = fmt::color::green_yellow;
                else if (m_Cells->at(k).FarmCell != 0)
                    color = fmt::color::yellow;

                fmt::print(fmt::fg(color), "{:3d}   ", m_Cells->at(k).CellId);
                k++;
            }
            fmt::print("\n");

            for (auto j = 0; j < GameMap::MAP_WIDTH; j++) {
                auto color = fmt::color::gray;

                if (!m_Cells->at(k).Mov)
                    color = fmt::color::dark_red;
                else if (m_Entities[k])
                    color = fmt::color::dark_orange;
                else if (highlight.contains(k))
                    color = fmt::color::cyan;
                else if (m_Cells->at(k).MapChangeData != 0)
                    color = fmt::color::green_yellow;
                else if (m_Cells->at(k).FarmCell != 0)
                    color = fmt::color::yellow;

                fmt::print(fmt::fg(color), "   {:3d}", m_Cells->at(k).CellId);
                k++;
            }
            fmt::print("\n");
        }
    }

    struct Node
    {
        Node(const GameMapCell &cell)
            : Cell(cell)
            , GCost(0)
            , FCost(0) {
        }

        Node(const Node &) = delete;
        Node operator=(const Node &) = delete;
        Node(Node &&) = default;

        const GameMapCell &Cell;

        float GCost;
        float FCost;
    };

    struct NodeComparator
    {
        bool operator()(const Node *a, const Node *b) const {
            return a->FCost > b->FCost;
        }
    };

    bool GameMap::IsEntityOnCell(int cell_id) const {
        return m_Entities[cell_id];
    }

    void GameMap::MarkCellOccupied(int cell_id) {
        m_Entities[cell_id] = true;
    }

    void GameMap::ClearOccupiedCells() {
        m_Entities.fill(false);
    }

    bool GameMap::IsChangeZone(int cell_a, int cell_b) const {
        auto &a = m_Cells->at(cell_a);
        auto &b = m_Cells->at(cell_b);

        auto diff = std::abs(std::abs(a.Floor) - std::abs(b.Floor));

        return a.MoveZone != b.MoveZone && diff == 0;
    }

    int GameMap::PointSpecialEffects(int x, int y) const {
        auto cell = map_tools::GetCellIdByCoord(x, y);

        return m_Cells->at(cell).SpecialEffects;
    }

    float GameMap::GetPointWeight(const GameMapCell &current, bool allow_through_entity) const {
        using namespace map_tools;

        float weight = 0.f;

        auto speed = current.Speed;
        if (allow_through_entity) {
            // entity = EntitiesManager.getInstance().getEntityOnCell(cellId,_playerClass);
            // if(entity && !entity["allowMovementThrough"])
            if (IsEntityOnCell(current.CellId)) {
                // Check if entity is on cell
                return 20;
            } else if (speed >= 0) {
                return 6 - speed;
            } else {
                return 12 + std::abs(speed);
            }
        } else {
            weight = 1.f;
            auto &pos = current.Position;

            if (IsEntityOnCell(current.CellId))
                weight += .3f;

            if (IsValidCoord(pos.X + 1, pos.Y) && IsEntityOnCell(GetCellIdByCoord(pos.X + 1, pos.Y)))
                weight += .3f;

            if (IsValidCoord(pos.X, pos.Y + 1) && IsEntityOnCell(GetCellIdByCoord(pos.X, pos.Y + 1)))
                weight += .3f;

            if (IsValidCoord(pos.X - 1, pos.Y) && IsEntityOnCell(GetCellIdByCoord(pos.X - 1, pos.Y)))
                weight += .3f;

            if (IsValidCoord(pos.X, pos.Y - 1) && IsEntityOnCell(GetCellIdByCoord(pos.X, pos.Y - 1)))
                weight += .3f;

            if ((PointSpecialEffects(pos.X, pos.Y) & 2) == 2) {
                weight += 0.2;
            }
        }

        return weight;
    }

    float GameMap::GetPointWeight(const GameMapCell &current, const GameMapCell &end, bool allow_through_entity) const {
        using namespace map_tools;

        float weight = 0.f;

        if (end.CellId == current.CellId) {
            return 1.f;
        }

        auto speed = current.Speed;
        if (allow_through_entity) {
            if (IsEntityOnCell(current.CellId)) {
                // Check if entity is on cell
                return 20;
            } else if (speed >= 0) {
                return 6 - speed;
            } else {
                return 12 + std::abs(speed);
            }
        } else {
            weight = 1.f;
            auto &pos = current.Position;

            if (IsEntityOnCell(current.CellId))
                weight += .3f;

            if (IsValidCoord(pos.X + 1, pos.Y) && IsEntityOnCell(GetCellIdByCoord(pos.X + 1, pos.Y)))
                weight += .3f;

            if (IsValidCoord(pos.X, pos.Y + 1) && IsEntityOnCell(GetCellIdByCoord(pos.X, pos.Y + 1)))
                weight += .3f;

            if (IsValidCoord(pos.X - 1, pos.Y) && IsEntityOnCell(GetCellIdByCoord(pos.X - 1, pos.Y)))
                weight += .3f;

            if (IsValidCoord(pos.X, pos.Y - 1) && IsEntityOnCell(GetCellIdByCoord(pos.X, pos.Y - 1)))
                weight += .3f;

            if ((PointSpecialEffects(pos.X, pos.Y) & 2) == 2) {
                weight += 0.2;
            }
        }

        return weight;
    }

    bool GameMap::PointMov(int x, int y, bool allow_through_entity, int previous, int end, bool avoid_obstacles) const {
        (void)end;
        (void)avoid_obstacles;

        bool use_new_system = true;
        int diff = 0;

        if (map_tools::IsInMap(x, y)) {
            int cell_id = map_tools::GetCellIdByCoord(x, y);
            auto &cell = m_Cells->at(cell_id);
            bool mov = cell.Mov /* TODO: For fight: && (!is_in_fight || !cell.NonWalkableDuringFight) */;

            // TODO: For fight I guess
            // if (m_UpdatedCell[cell_id] != null) {
            //     mov = m_UpdatedCell[cell_id];
            // }

            if (mov && use_new_system && previous != -1 && previous != cell_id) {
                auto &previous_cell = m_Cells->at(previous);
                diff = std::abs(std::abs(cell.Floor) - std::abs(previous_cell.Floor));

                if ((previous_cell.MoveZone != cell.MoveZone && diff > 0)
                    || (previous_cell.MoveZone == cell.MoveZone && cell.MoveZone == 0 && diff > TOLERANCE_ELEVATION)) {
                    mov = false;
                }
            }

            if (!allow_through_entity) {
                // TODO: Implement this for fight
                // for (auto &e : entities) {
                //     if (e && e is IObstacle && e.position && e.position.cellId == cellId) {
                //         o = e as IObstacle;
                //         if (!(endCellId == cellId && o.canWalkTo())) {
                //             if (!o.canWalkThrough()) {
                //                 return false;
                //             }
                //         }
                //     }
                // }
                // if (avoidObstacles && (this.obstaclesCells.indexOf(cellId) != -1 && cellId != end)) {
                //     return false;
                // }
            }

            return mov;
        }

        return false;
    }

    std::vector<PathElement> GameMap::GetShortestPath(
        int32_t start_cell, int32_t end_cell, bool diagonals, bool allow_through_entity, bool avoid_obstacles) const {
        using namespace map_tools;

        // Build graph
        auto graph = std::vector<Node>();
        graph.reserve(m_Cells->size());

        for (auto &c : *m_Cells)
            graph.push_back(Node{c});

        auto start = &graph[start_cell];
        auto end = &graph[end_cell];

        std::priority_queue<Node *, std::vector<Node *>, NodeComparator> open_list;
        std::unordered_set<int32_t> closed_list;
        std::array<int, MAP_WIDTH * MAP_HEIGHT * 2> parent_map;
        parent_map.fill(-1);
        std::array<bool, MAP_WIDTH * MAP_HEIGHT * 2> in_open_list;
        in_open_list.fill(false);

        fmt::println("We want to go from {} to {}", start_cell, end_cell);
        open_list.push(start);
        in_open_list[start_cell] = true;
        parent_map[start_cell] = -1;

        int distance_to_end = GetDistance(start_cell, end_cell);
        int end_cell_aux_id = start_cell;

        while (!open_list.empty()) {
            auto neighbor_nodes_count = open_list.size();

            auto parent_node = open_list.top();
            open_list.pop();

            auto parent_id = parent_node->Cell.CellId;

            closed_list.insert(parent_id);
            in_open_list[parent_id] = false;
            auto &parent_position = parent_node->Cell.Position;

            for (int y = parent_position.Y - 1; y <= parent_position.Y + 1; y++) {
                for (int x = parent_position.X - 1; x <= parent_position.X + 1; x++) {
                    auto cell_id = GetCellIdByCoord(x, y);

                    if (cell_id == -1 || closed_list.contains(cell_id) || cell_id == parent_id)
                        continue;

                    auto neighbor = &graph[cell_id];

                    // https://github.com/scalexm/DofusInvoker/blob/master/com/ankamagames/jerakine/pathfinding/Pathfinding.as#L121
                    bool disgusting_condition =
                        PointMov(x, y, allow_through_entity, parent_id, end_cell, avoid_obstacles)
                        && (y == parent_position.Y || x == parent_position.X
                            || (diagonals
                                && (PointMov(parent_position.X,
                                             y,
                                             allow_through_entity,
                                             parent_id,
                                             end_cell,
                                             avoid_obstacles)
                                    || PointMov(x,
                                                parent_position.Y,
                                                allow_through_entity,
                                                parent_id,
                                                end_cell,
                                                avoid_obstacles))));

                    if (!disgusting_condition)
                        continue;

                    int GCost =
                        parent_node->GCost
                        + (y == parent_node->Cell.Position.Y || x == parent_node->Cell.Position.X ? HV_COST : DIAG_COST)
                              * GetPointWeight(neighbor->Cell, end->Cell, allow_through_entity);

                    if (allow_through_entity) {
                        auto &end_cell = end->Cell;
                        auto &start_cell = start->Cell;

                        bool cell_on_end_column = x + y == end_cell.Position.X + end_cell.Position.Y;
                        bool cell_on_start_column = x + y == start_cell.Position.X + start_cell.Position.Y;
                        bool cell_on_end_line = x - y == end_cell.Position.X - end_cell.Position.Y;
                        bool cell_on_start_line = x - y == start_cell.Position.X - start_cell.Position.Y;

                        if ((!cell_on_end_column && !cell_on_end_line)
                            || (!cell_on_start_column && !cell_on_start_line)) {
                            GCost += GetDistance(cell_id, end_cell.CellId);
                            GCost += GetDistance(cell_id, start_cell.CellId);
                        }
                        if (x == end_cell.Position.X || y == end_cell.Position.Y) {
                            GCost -= 3;
                        }

                        if (cell_on_end_column || cell_on_end_line || x + y == parent_position.X + parent_position.Y
                            || x - y == parent_position.X - parent_position.Y) {
                            GCost -= 2;
                        }

                        if (static_cast<int>(neighbor_nodes_count) == start_cell.Position.X
                            || y == start_cell.Position.Y) {
                            GCost -= 3;
                        }

                        if (cell_on_start_column || cell_on_start_line) {
                            GCost -= 2;
                        }

                        auto distance_to_end_tmp = GetDistance(cell_id, end_cell.CellId);
                        if (distance_to_end_tmp < distance_to_end) {
                            end_cell_aux_id = cell_id;
                            distance_to_end = distance_to_end_tmp;
                        }
                    }

                    if (parent_map[cell_id] == -1 || GCost < neighbor->GCost) {
                        parent_map[cell_id] = parent_id;
                        float HCost =
                            static_cast<float>(HEURISTIC_COST)
                            * std::sqrt(std::pow(end->Cell.Position.Y - y, 2) + std::pow(end->Cell.Position.X - x, 2));
                        neighbor->GCost = GCost;
                        neighbor->FCost = GCost + HCost;

                        if (!in_open_list[cell_id]) {
                            in_open_list[cell_id] = true;
                            open_list.push(neighbor);
                        }
                    }
                }
            }
        }

        // Search finished
        auto start_id = start->Cell.CellId;
        auto end_id = end->Cell.CellId;

        if (parent_map[end_id] == -1) {
            end_id = end_cell_aux_id;
        }

        std::list<PathElement> path;

        for (auto cursor = end_id; cursor != start_id; cursor = parent_map[cursor]) {
            if (diagonals) {
                auto parent = parent_map[cursor];
                auto grand_parent = parent == -1 ? -1 : parent_map[parent];
                auto grandGrandParent = grand_parent == -1 ? -1 : parent_map[grand_parent];
                int kX = GetCellIdXCoord(cursor);
                int kY = GetCellIdYCoord(cursor);
                if (grand_parent != -1 && GetDistance(cursor, grand_parent) == 1) {
                    if (PointMov(kX, kY, allow_through_entity, grand_parent, end_id)) {
                        parent_map[cursor] = grand_parent;
                    }
                } else if (grandGrandParent != -1 && GetDistance(cursor, grandGrandParent) == 2) {
                    int nextX = GetCellIdXCoord(grandGrandParent);
                    int nextY = GetCellIdYCoord(grandGrandParent);
                    int interX = kX + std::round((nextX - kX) / 2);
                    int interY = kY + std::round((nextY - kY) / 2);
                    if (PointMov(interX, interY, allow_through_entity, cursor, end_id)
                        && GetPointWeight(m_Cells->at(GetCellIdByCoord(interX, interY))) < 2) {
                        parent_map[cursor] = GetCellIdByCoord(interX, interY);
                    }
                } else if (grand_parent != -1 && GetDistance(cursor, grand_parent) == 2) {
                    int nextX = GetCellIdXCoord(grand_parent);
                    int nextY = GetCellIdYCoord(grand_parent);
                    int interX = GetCellIdXCoord(parent);
                    int interY = GetCellIdYCoord(parent);
                    if (kX + kY == nextX + nextY && kX - kY != interX - interY
                        && !IsChangeZone(GetCellIdByCoord(kX, kY), GetCellIdByCoord(interX, interY))
                        && !IsChangeZone(GetCellIdByCoord(interX, interY), GetCellIdByCoord(nextX, nextY))) {
                        parent_map[cursor] = grand_parent;
                    } else if (kX - kY == nextX - nextY && kX - kY != interX - interY
                               && !IsChangeZone(GetCellIdByCoord(kX, kY), GetCellIdByCoord(interX, interY))
                               && !IsChangeZone(GetCellIdByCoord(interX, interY), GetCellIdByCoord(nextX, nextY))) {
                        parent_map[cursor] = grand_parent;
                    } else if (kX == nextX && kX != interX
                               && GetPointWeight(m_Cells->at(GetCellIdByCoord(kX, interY))) < 2
                               && PointMov(kX, interY, allow_through_entity, cursor, end_id)) {
                        parent_map[cursor] = GetCellIdByCoord(kX, interY);
                    } else if (kY == nextY && kY != interY
                               && GetPointWeight(m_Cells->at(GetCellIdByCoord(interX, kY))) < 2
                               && PointMov(interX, kY, allow_through_entity, cursor, end_id)) {
                        parent_map[cursor] = GetCellIdByCoord(interX, kY);
                    }
                }
            }

            auto parent_coords = map_tools::GetCellCordById(parent_map[cursor]);
            auto cursor_coords = map_tools::GetCellCordById(cursor);
            PathElement p{};
            p.CellId = parent_map[cursor];
            p.Dir = static_cast<Direction>(map_tools::GetLookDirection8ExactByCoord(parent_coords, cursor_coords));

            path.push_back(p);
        }

        std::reverse(path.begin(), path.end());

        // Add the end point
        if (path.size() > 0) {
            auto parent_coords = map_tools::GetCellCordById(path.back().CellId);
            auto cursor_coords = map_tools::GetCellCordById(end_id);

            PathElement end_point{};
            end_point.CellId = end_id;
            end_point.Dir =
                static_cast<Direction>(map_tools::GetLookDirection8ExactByCoord(parent_coords, cursor_coords));

            path.push_back(end_point);
        }

        std::unordered_set<int> highlight;
        for (auto &p : path) {
            highlight.emplace(p.CellId);
        }

        PrintMap(highlight);

        // Compress the path
        if (path.size() > 0) {
            auto it = path.end();
            --it;
            --it;

            while (it != path.begin()) {
                auto prev = it;
                --prev;

                if (it->Dir == prev->Dir) {
                    it = path.erase(it);
                    --it;
                } else {
                    --it;
                }
            }
        }

        return std::vector(path.begin(), path.end());
    }

    int GameMap::GetCellForResource(int current_cell, int resource_cell) {
        auto end = map_tools::GetCellCordById(resource_cell);
        auto start = map_tools::GetCellCordById(current_cell);

        auto min_cell_id = -1;
        double min_distance = std::numeric_limits<double>::max();

        for (auto y = end.Y - 1; y <= end.Y + 1; y++) {
            for (auto x = end.X - 1; x <= end.X + 1; x++) {
                auto cell_id = map_tools::GetCellIdByCoord(x, y);

                // Invalid cell
                if (cell_id == -1)
                    continue;

                // Not a neighbor of the resource
                if (x == end.X && y == end.Y)
                    continue;

                // Cell is not walkable or has some entity on it
                if (!m_Cells->at(cell_id).Mov || m_Entities[cell_id])
                    continue;

                auto distance = std::sqrt(std::pow(start.Y - y, 2) + std::pow(start.X - x, 2));

                // We want to bias the distance to get to a cell adjacent to the resource
                auto look_direction = map_tools::GetLookDirection8ExactByCoord(Vec2{x, y}, end);
                if (look_direction == Direction::DIRECTION_NORTH || look_direction == DIRECTION_SOUTH
                    || look_direction == DIRECTION_EAST || look_direction == DIRECTION_WEST) {
                    distance += 1;
                }

                if (distance <= min_distance) {
                    min_distance = distance;
                    min_cell_id = cell_id;
                }
            }
        }

        return min_cell_id;
    }
} // namespace dfs

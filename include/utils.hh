#pragma once

#include <chrono>
#include <cstdint>
#include <ratio>

namespace dfs
{
    struct Vec2
    {
        Vec2()
            : X(0)
            , Y(0) {
        }

        Vec2(int x, int y)
            : X(x)
            , Y(y) {
        }

        int X;
        int Y;
    };

    enum Direction
    {
        DIRECTION_EAST = 0,
        DIRECTION_SOUTH_EAST = 1,
        DIRECTION_SOUTH = 2,
        DIRECTION_SOUTH_WEST = 3,
        DIRECTION_WEST = 4,
        DIRECTION_NORTH_WEST = 5,
        DIRECTION_NORTH = 6,
        DIRECTION_NORTH_EAST = 7,
    };

    struct WorldGraphEdgeTransition
    {
        int Type;
        Direction Dir;
        int SkillId;
        int TransitionMapId;
        int CellId;
    };

    struct WorldGraphEdge
    {
        int MapId;
        int ZoneId;
        std::vector<WorldGraphEdgeTransition> Transitions;
    };

    struct PathElement
    {
        int32_t CellId;
        Direction Dir;

        bool operator==(const PathElement &) const = default;

        static PathElement FromCompressed(int32_t key_cell);
        int32_t ToCompressed() const;
    };

    std::chrono::duration<int64_t, std::milli> GetMovementDuration(const std::vector<int32_t> cells, bool cautious);
} // namespace dfs

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fmt/base.h>

#include "map.hh"
#include "utils.hh"

namespace dfs
{
    static constexpr const uint32_t WALKING_HORIZONTAL_TIMING = 510;
    static constexpr const uint32_t WALKING_STRAIGHT_TIMING = 480;
    static constexpr const uint32_t WALKING_VERTICAL_TIMING = 425;

    static constexpr const uint32_t RUNNING_HORIZONTAL_TIMING = 255;
    static constexpr const uint32_t RUNNING_STRAIGHT_TIMING = 170;
    static constexpr const uint32_t RUNNING_VERTICAL_TIMING = 150;

    PathElement PathElement::FromCompressed(int32_t key_cell) {
        PathElement c{};
        c.CellId = key_cell & 0x3ff;
        c.Dir = static_cast<enum Direction>((key_cell >> 12) & 7);

        return c;
    }

    int32_t PathElement::ToCompressed() const {
        return ((int)Dir & 7) << 12 | (CellId & 0xfff);
    }

    std::chrono::duration<int64_t, std::milli> GetMovementDuration(const std::vector<int32_t> cells, bool cautious) {
        auto x_speed = RUNNING_HORIZONTAL_TIMING;
        auto y_speed = RUNNING_VERTICAL_TIMING;
        auto speed = RUNNING_STRAIGHT_TIMING;

        if (cautious || cells.size() <= 3) {
            x_speed = WALKING_HORIZONTAL_TIMING;
            y_speed = WALKING_VERTICAL_TIMING;
            speed = WALKING_STRAIGHT_TIMING;
        }

        auto runtime = 0;

        for (size_t i = 0; i < cells.size() - 1; i++) {
            // If we are moving from one cell (in cell id), it means that we are moving horizontally.
            if (std::abs(cells[i] - cells[i + 1]) == 1)
                runtime += x_speed;
            else if (std::abs(cells[i] - cells[i + 1]) == GameMap::MAP_WIDTH)
                runtime += y_speed;
            else
                runtime += speed;
        }

        // Add some random delta between 1 and 50ms
        auto random_delta_ms = 1 + rand() % 50;
        runtime += random_delta_ms;

        return std::chrono::milliseconds(runtime);
    }
} // namespace dfs

#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include "game.hh"
#include "map.hh"
#include "utils.hh"

namespace dfs
{
    class PathfindingTest : public testing::Test {
      protected:
        PathfindingTest() {
            dfs::GameData game_data{};
            m_TestMap = game_data.GetMap(189793795);
        }

        std::unique_ptr<GameMap> m_TestMap;
    };

    TEST_F(PathfindingTest, PathfindingSimpleLine) {
        // clang-format off
        auto ref = std::vector{
            PathElement{62, Direction::DIRECTION_SOUTH_WEST},
            PathElement{183, Direction::DIRECTION_SOUTH_WEST},
        };
        // clang-format on

        auto path = m_TestMap->GetShortestPath(62, 183, true);

        ASSERT_EQ(ref.size(), path.size()) << "Vectors are of unequal length";
        for (size_t i = 0; i < ref.size(); ++i) {
            EXPECT_EQ(ref[i], path[i]) << "Vectors differ at index " << i;
        }
    }

    TEST_F(PathfindingTest, PathfiningMix) {
        // clang-format off
        auto ref = std::vector{
            PathElement{156, Direction::DIRECTION_SOUTH_EAST},
            PathElement{214, Direction::DIRECTION_EAST},
            PathElement{218, Direction::DIRECTION_EAST},
        };
        // clang-format on

        auto path = m_TestMap->GetShortestPath(156, 218, true);

        ASSERT_EQ(ref.size(), path.size()) << "Vectors are of unequal length";
        for (size_t i = 0; i < ref.size(); ++i) {
            EXPECT_EQ(ref[i], path[i]) << "Vectors differ at index " << i;
        }
    }

    TEST_F(PathfindingTest, PathfiningMix2) {
        // clang-format off
        auto ref = std::vector{
            PathElement{218, Direction::DIRECTION_NORTH_WEST},
            PathElement{146, Direction::DIRECTION_WEST},
            PathElement{144, Direction::DIRECTION_WEST},
        };
        // clang-format on

        auto path = m_TestMap->GetShortestPath(218, 144, true);

        ASSERT_EQ(ref.size(), path.size()) << "Vectors are of unequal length";
        for (size_t i = 0; i < ref.size(); ++i) {
            EXPECT_EQ(ref[i], path[i]) << "Vectors differ at index " << i;
        }
    }

    TEST_F(PathfindingTest, PathfiningTwoCells) {
        // clang-format off
        auto ref = std::vector{
            PathElement{144, Direction::DIRECTION_WEST},
            PathElement{142, Direction::DIRECTION_WEST},
        };
        // clang-format on

        auto path = m_TestMap->GetShortestPath(144, 142, true);

        ASSERT_EQ(ref.size(), path.size()) << "Vectors are of unequal length";
        for (size_t i = 0; i < ref.size(); ++i) {
            EXPECT_EQ(ref[i], path[i]) << "Vectors differ at index " << i;
        }
    }

    TEST_F(PathfindingTest, PathfiningZigZag) {
        // clang-format off
        auto ref = std::vector{
            PathElement{347, Direction::DIRECTION_NORTH},
            PathElement{291, Direction::DIRECTION_NORTH_EAST},
            PathElement{223, Direction::DIRECTION_NORTH},
            PathElement{195, Direction::DIRECTION_NORTH},
        };
        // clang-format on

        auto path = m_TestMap->GetShortestPath(347, 195, true);

        ASSERT_EQ(ref.size(), path.size()) << "Vectors are of unequal length";
        for (size_t i = 0; i < ref.size(); ++i) {
            EXPECT_EQ(ref[i], path[i]) << "Vectors differ at index " << i;
        }
    }

    TEST_F(PathfindingTest, PathfiningCircle) {
        // clang-format off
        auto ref = std::vector{
            PathElement{400, Direction::DIRECTION_NORTH_WEST},
            PathElement{371, Direction::DIRECTION_NORTH},
            PathElement{287, Direction::DIRECTION_NORTH},
        };
        // clang-format on

        auto path = m_TestMap->GetShortestPath(400, 287, true);

        ASSERT_EQ(ref.size(), path.size()) << "Vectors are of unequal length";
        for (size_t i = 0; i < ref.size(); ++i) {
            EXPECT_EQ(ref[i], path[i]) << "Vectors differ at index " << i;
        }
    }

    TEST_F(PathfindingTest, PathfiningCircleClosing) {
        // clang-format off
        auto ref = std::vector{
            PathElement{287, Direction::DIRECTION_SOUTH_EAST},
            PathElement{316, Direction::DIRECTION_SOUTH},
            PathElement{400, Direction::DIRECTION_SOUTH},
        };
        // clang-format on

        auto path = m_TestMap->GetShortestPath(287, 400, true);

        ASSERT_EQ(ref.size(), path.size()) << "Vectors are of unequal length";
        for (size_t i = 0; i < ref.size(); ++i) {
            EXPECT_EQ(ref[i], path[i]) << "Vectors differ at index " << i;
        }
    }

    TEST_F(PathfindingTest, PathfiningLineZigZag) {
        // clang-format off
        auto ref = std::vector{
            PathElement{201, Direction::DIRECTION_EAST},
            PathElement{203, Direction::DIRECTION_SOUTH_EAST},
            PathElement{217, Direction::DIRECTION_EAST},
            PathElement{218, Direction::DIRECTION_EAST},
        };
        // clang-format on

        auto path = m_TestMap->GetShortestPath(201, 218, true);

        ASSERT_EQ(ref.size(), path.size()) << "Vectors are of unequal length";
        for (size_t i = 0; i < ref.size(); ++i) {
            EXPECT_EQ(ref[i], path[i]) << "Vectors differ at index " << i;
        }
    }

    TEST_F(PathfindingTest, Pathfining3XMinus11Y) {
        // clang-format off
        auto ref = std::vector{
            PathElement{514, Direction::DIRECTION_NORTH},
            PathElement{430, Direction::DIRECTION_NORTH_EAST},
            PathElement{389, Direction::DIRECTION_NORTH},
            PathElement{361, Direction::DIRECTION_NORTH},
        };
        // clang-format on

        auto path = m_TestMap->GetShortestPath(514, 361, true);

        ASSERT_EQ(ref.size(), path.size()) << "Vectors are of unequal length";
        for (size_t i = 0; i < ref.size(); ++i) {
            EXPECT_EQ(ref[i], path[i]) << "Vectors differ at index " << i;
        }
    }
} // namespace dfs

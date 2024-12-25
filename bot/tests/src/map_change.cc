#include <gtest/gtest.h>
#include <memory>

#include "game.hh"
#include "map.hh"

namespace dfs
{
    class MapChangeTest : public testing::Test {
      protected:
        MapChangeTest() {
            dfs::GameData game_data{};
            game_data.Initialize();
            m_TestMap = game_data.GetMap(189793795);
        }

        std::unique_ptr<GameMap> m_TestMap;
    };

    TEST_F(MapChangeTest, ChangeMapTop) {
        auto cells_for_top_change = m_TestMap->GetCellToMap(189793794);
        ASSERT_EQ(cells_for_top_change->Transitions[0].CellId, 9);
    }
} // namespace dfs

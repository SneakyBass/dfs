#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fmt/base.h>
#include <fmt/color.h>
#include <memory>
#include <vector>

#include <rapidjson/document.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/filewritestream.h>
#include <rapidjson/rapidjson.h>
#include <rapidjson/writer.h>

#include "game.hh"
#include "map.hh"
#include "utils.hh"

#define DO(ACTION_NAME, ACTION)                                                                                        \
    fmt::println("Parsing " ACTION_NAME "...");                                                                        \
    if (ACTION) {                                                                                                      \
        fmt::print(fmt::fg(fmt::color::green_yellow), "[OK] ");                                                        \
        fmt::println(ACTION_NAME);                                                                                     \
    } else {                                                                                                           \
        fmt::print(fmt::fg(fmt::color::red), "[KO] ");                                                                 \
        fmt::println(ACTION_NAME);                                                                                     \
        return false;                                                                                                  \
    }

#define CHECK_OBJECT(VAR, NAME)                                                                                        \
    if (!VAR.HasMember(NAME) || !VAR[NAME].IsObject()) {                                                               \
        fmt::println(stderr, "Missing object field: {}", NAME);                                                        \
        return false;                                                                                                  \
    }

#define CHECK_ARRAY(VAR, NAME)                                                                                         \
    if (!VAR.HasMember(NAME) || !VAR[NAME].IsArray()) {                                                                \
        fmt::println(stderr, "Missing array field: {}", NAME);                                                         \
        return false;                                                                                                  \
    }

namespace dfs
{
    static void GenerateMaps() {
        static constexpr const char *MAPS_DIRECTORY = "data/maps";
        if (!std::filesystem::exists(MAPS_DIRECTORY))
            std::filesystem::create_directory(MAPS_DIRECTORY);
        const std::filesystem::path exported_maps("data/maps-json");

        for (const auto &entry : std::filesystem::directory_iterator(exported_maps)) {
            auto filename = entry.path().filename();

            // Ignore non map files
            if (!filename.string().starts_with("map_"))
                continue;

            FILE *fp = fopen(entry.path().c_str(), "r");
            if (!fp) {
                fmt::println("[ERROR] Failed to open {}", entry.path().c_str());
                return;
            }

            rapidjson::Document document;

            char read_buffer[2048];
            rapidjson::FileReadStream is(fp, read_buffer, sizeof(read_buffer));
            document.ParseStream(is);

            fclose(fp);

            // We only want to keep the `cellsData` field.
            if (!document.HasMember("m_Name") || !document["m_Name"].IsString()) {
                fmt::println("[ERROR] map {} is invalid: missing name", filename.c_str());
                return;
            }

            auto map_name = document["m_Name"].GetString();

            if (!document.HasMember("mapData") || !document["mapData"].IsObject()) {
                fmt::println("[ERROR] map {} is invalid: missing map data", filename.c_str());
                return;
            }
            auto map_data = document["mapData"].GetObject();

            if (!map_data.HasMember("cellsData") || !map_data["cellsData"].IsObject()) {
                fmt::println("[ERROR] map {} is invalid: missing cells", filename.c_str());
                return;
            }

            auto cells_data_obj = map_data["cellsData"].GetObject();
            if (!cells_data_obj.HasMember("Array") || !cells_data_obj["Array"].IsArray()) {
                fmt::println("[ERROR] map {} is invalid: missing cell array", filename.c_str());
                return;
            }

            rapidjson::Value &cells_array = cells_data_obj["Array"];

            char write_buffer[2048];
            FILE *wp = fopen(fmt::format("data/maps/{}.json", map_name).c_str(), "w");
            if (!wp) {
                fmt::println("[ERROR] Failed to open data/maps/{}.json for writing", map_name);
                return;
            }

            rapidjson::FileWriteStream os(wp, write_buffer, sizeof(write_buffer));
            rapidjson::Writer writer(os);

            cells_array.Accept(writer);

            fclose(wp);
            fmt::println("[OK] {}", map_name);
        }

        fmt::println("You may remove the data/maps-json folder to avoid generating maps in the future.");
    }

    static std::shared_ptr<std::vector<GameMapCell>> GetMapCells(int32_t map_id) {
        // Read the map from filesystem
        FILE *fp = fopen(fmt::format("data/maps/map_{}.json", map_id).c_str(), "r");
        if (!fp) {
            fmt::println("Failed to get cells for map {}: faile to open map file", map_id);
            return {};
        }

        rapidjson::Document document;

        char read_buffer[4096];
        rapidjson::FileReadStream is(fp, read_buffer, sizeof(read_buffer));
        document.ParseStream(is);

        if (!document.IsArray()) {
            fmt::println("Invalid map: map_{}.json", map_id);
            return {};
        }

        auto map_cells = std::make_shared<std::vector<GameMapCell>>();
        map_cells->reserve(560);

        for (auto it = document.GetArray().Begin(); it != document.GetArray().End(); it++) {
            GameMapCell cell{};
            cell.CellId = (*it)["cellNumber"].GetInt();
            cell.Speed = (*it)["speed"].GetInt();
            cell.MapChangeData = (*it)["mapChangeData"].GetInt();
            cell.MoveZone = (*it)["moveZone"].GetInt();
            cell.LinkedZone = (*it)["linkedZone"].GetInt();
            cell.Mov = (*it)["mov"].GetInt();
            cell.Los = (*it)["los"].GetInt();
            cell.NonWalkableDuringFight = (*it)["nonWalkableDuringFight"].GetInt();
            cell.FarmCell = (*it)["farmCell"].GetInt();
            cell.Visible = (*it)["visible"].GetInt();

            if (it->HasMember("floor"))
                cell.Floor = (*it)["floor"].GetInt();

            map_cells->push_back(cell);
        }

        fclose(fp);

        return map_cells;
    }

    std::unique_ptr<GameMap> GameData::GetMap(int32_t map_id) const {
        std::shared_ptr<std::vector<GameMapCell>> cells = nullptr;

        auto cells_cached = m_MapCells.find(map_id);
        if (cells_cached != m_MapCells.end()) {
            cells = cells_cached->second;
        } else {
            cells = GetMapCells(map_id);

            for (auto &c : *cells) {
                c.Position = map_tools::GetCellCordById(c.CellId);
            }

            if (cells->empty()) {
                // This map was not found
                return nullptr;
            }

            m_MapCells.emplace(map_id, cells);
        }

        auto it = m_WorldGraph.find(map_id);
        if (it == m_WorldGraph.end()) {
            fmt::println("Map {} is not in the worldgraph! Moving between maps will not be available.", map_id);

            // Return a pointer to an empty neighbors list to avoid segfaults
            it = m_WorldGraph.find(-1);
        }

        auto coordinates = GetCoordinates(map_id);

        return std::make_unique<GameMap>(map_id, coordinates, cells, it->second);
    }

    bool GameData::ParseMapCoordinates() {
        FILE *fp = fopen("data/map-coordinates.json", "r");
        if (!fp) {
            fmt::println("Failed to open map-coordinates");
            return false;
        }

        rapidjson::Document document;

        char read_buffer[2048];
        rapidjson::FileReadStream is(fp, read_buffer, sizeof(read_buffer));
        document.ParseStream(is);

        if (!document.IsArray())
            return false;

        m_MapCoordinates.reserve(document.GetArray().Size());

        for (auto it = document.GetArray().Begin(); it != document.GetArray().End(); it++) {
            if (!it->HasMember("map-ids") || !(*it)["map-ids"].IsArray())
                return false;

            if (!it->HasMember("position") || !(*it)["position"].IsObject())
                return false;

            Vec2 map_coords{};
            auto position = (*it)["position"].GetObject();
            map_coords.X = position["x"].GetInt();
            map_coords.Y = position["y"].GetInt();

            auto map_ids = (*it)["map-ids"].GetArray();
            for (auto it = map_ids.Begin(); it != map_ids.End(); it++) {
                m_MapCoordinates.emplace(it->GetInt(), map_coords);
            }
        }

        fclose(fp);

        return true;
    }

    bool GameData::ParseWorldGraph() {
        FILE *fp = fopen("data/worldgraph.json", "r");
        if (!fp) {
            fmt::println("Failed to open worldgraph");
            return false;
        }

        rapidjson::Document document;

        char read_buffer[2048];
        rapidjson::FileReadStream is(fp, read_buffer, sizeof(read_buffer));
        document.ParseStream(is);

        CHECK_OBJECT(document, "m_edges");
        auto current_node = document["m_edges"].GetObject();

        CHECK_OBJECT(current_node, "m_values");
        current_node = current_node["m_values"].GetObject();

        CHECK_ARRAY(current_node, "Array");
        auto values = current_node["Array"].GetArray();

        m_WorldGraph.reserve(values.Size() + 1);
        m_WorldGraph[-1] = {};

        for (auto it = values.Begin(); it != values.End(); it++) {
            CHECK_OBJECT((*it), "m_values");
            auto current_node = (*it)["m_values"].GetObject();

            CHECK_ARRAY(current_node, "Array");
            auto values = current_node["Array"].GetArray();

            for (auto it = values.Begin(); it != values.End(); it++) {
                CHECK_OBJECT((*it), "m_from");
                CHECK_OBJECT((*it), "m_to");
                CHECK_OBJECT((*it), "m_transitions");

                auto from = (*it)["m_from"].GetObject();
                auto to = (*it)["m_to"].GetObject();
                auto transitions = (*it)["m_transitions"].GetObject()["Array"].GetArray();

                auto from_id = from["m_mapId"].GetInt();

                WorldGraphEdge edge{};
                edge.MapId = to["m_mapId"].GetInt();
                edge.ZoneId = to["m_zoneId"].GetInt();

                for (auto tit = transitions.Begin(); tit != transitions.End(); tit++) {
                    WorldGraphEdgeTransition t{};
                    t.Type = (*tit)["m_type"].GetInt();
                    t.Dir = static_cast<Direction>((*tit)["m_direction"].GetInt());
                    t.SkillId = (*tit)["m_skillId"].GetInt();
                    t.TransitionMapId = (*tit)["m_transitionMapId"].GetInt();
                    t.CellId = (*tit)["m_cellId"].GetInt();

                    edge.Transitions.push_back(t);
                }

                auto it_wg = m_WorldGraph.find(from_id);
                if (it_wg == m_WorldGraph.end()) {
                    // Insert the edge
                    m_WorldGraph.emplace(from_id, std::vector<WorldGraphEdge>{edge});
                } else {
                    it_wg->second.push_back(edge);
                }
            }
        }

        fclose(fp);

        return true;
    }

    bool GameData::Initialize() {
        fmt::println("Reading game files...");

        if (std::filesystem::exists("data/maps-json")) {
            fmt::println("Generating maps data... This may take some time.");
            GenerateMaps();
        }

        DO("Coordinates", ParseMapCoordinates());
        DO("WorldGraph", ParseWorldGraph());

        return true;
    }

    Vec2 GameData::GetCoordinates(int32_t map_id) const {
        auto map_coords = m_MapCoordinates.find(map_id);
        if (map_coords != m_MapCoordinates.end())
            return map_coords->second;

        // Return [0, 0]
        return Vec2{};
    }
} // namespace dfs

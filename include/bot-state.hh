#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <unordered_map>

namespace dfs
{
    using now_t =
        std::chrono::time_point<std::chrono::system_clock, std::chrono::duration<long, std::ratio<1, 1000000000>>>;

    class GameMap;
    class GameData;

    struct GenericActor
    {
        int64_t Id;
        int32_t CurrentCell;
        int32_t TargetCell;
        bool Moving;
        std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds> ArrivalTime;

        virtual void UpdateState(const now_t &now);
    };

    struct Player : public GenericActor
    {
        std::string Name;
        bool Collecting;
        int CollectingId;

        virtual void UpdateState(const now_t &now) override;
    };

    struct Monster : public GenericActor
    {
        uint16_t EnnemyCount;
        uint16_t TotalLevel;
    };

    enum CollectibleState
    {
        Available,
        InCooldown,
        Unknown,
    };

    enum ElementType
    {
        /// Frene
        Frene = 1,

        /// Ble
        Wheat = 38,

        /// Orge
        Barley = 43,

        /// Ortie
        Nettle = 254,
    };

    struct InteractiveElementSkill
    {
        int32_t SkillId;
        int32_t SkillInstanceUid;
    };

    struct Collectible
    {
        int32_t Id;
        int32_t ElementTypeId;
        int32_t CellId;
        CollectibleState State;
        std::vector<InteractiveElementSkill> EnabledSkills;
        std::vector<InteractiveElementSkill> DisabledSkills;
    };

    struct BotState
    {
        BotState(const GameData &);

        bool Active;
        Player CurrentPlayer;
        std::unique_ptr<GameMap> CurrentMap;
        bool InCombat;
        bool ChangingMaps;
        const GameData &Data;

        std::unordered_map<int64_t, Player> OtherPlayers;
        std::unordered_map<int64_t, Monster> Monsters;
        std::unordered_map<int64_t, GenericActor> NPCs;
        std::unordered_map<int64_t, GenericActor> Actors;
        std::unordered_map<int64_t, Collectible> Collectibles;
    };
} // namespace dfs

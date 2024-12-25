#pragma once

#include "utils.hh"
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

// clang-format off
namespace com {
namespace ankama {
namespace dofus {
namespace server {
namespace game {
namespace protocol {
    class Request;
    class Response;
    class Event;
} // namespace protocol
} // namespace game
} // namespace server
} // namespace dofus
} // namespace ankama
} // namespace com
// clang-format on

namespace dfs
{
    class BotDescriptor;

    class Messages {
      private:
        enum Request
        {
            MapMovementRequest,
            MapChangeRequest,
            ChatChannelMessageRequest,
            InteractiveUseRequest,
            MapMovementConfirmRequest,
            MapInformationRequest,
            PingRequest,
        };

        enum Response
        {
            MapMovementConfirmResponse,
        };

        enum Event
        {
            MapMovementEvent,
            ChatChannelMessageEvent,
            MapComplementaryInformationEvent,
            MapChangeOrientationEvent,
            MapCurrentEvent,
            GameRolePlayShowActorsEvent,
            InteractiveUsedEvent,
            InteractiveUseEndedEvent,
            InteractiveUseErrorEvent,
            StatedElementUpdatedEvent,
            InteractiveElementUpdatedEvent,
            TreasureHuntLegendaryEvent,
            TreasureHuntEvent,
            PongEvent,
            TimeEvent,
            CharacterCharacteristicsEvent,
        };

      public:
        Messages();
        ~Messages() = default;

        Messages(const Messages &) = delete;
        Messages operator=(const Messages &) = delete;

        std::vector<uint8_t> HandleMessage(const uint8_t *payload, size_t length, int len_offset,
                                           BotDescriptor *bot) const;

        static std::vector<uint8_t> ForgeMapMovementRequest(const std::vector<PathElement> &path, int map_id,
                                                            bool cautious);
        static std::vector<uint8_t> ForgeMapChangeRequest(int map_id, bool autopilot);
        static std::vector<uint8_t> ForgeMapMovementConfirmRequest();
        static std::vector<uint8_t> ForgeInteractiveUseRequest(int element_id, int skill_instance_uid);

      private:
        void ReadBindings();
        bool ParseRequest(const com::ankama::dofus::server::game::protocol::Request &request, BotDescriptor *bot) const;
        void ParseResponse(const com::ankama::dofus::server::game::protocol::Response &response,
                           BotDescriptor *bot) const;
        void ParseEvent(const com::ankama::dofus::server::game::protocol::Event &event, BotDescriptor *bot) const;
        std::optional<std::string> HandleGameMessage(const uint8_t *payload, size_t length, int len_offset,
                                                     BotDescriptor *bot) const;
        std::string HandleConnectionMessage(const uint8_t *payload, size_t length, int len_offset,
                                            BotDescriptor *bot) const;

      private:
        std::unordered_map<std::string, Request> m_RequestBindings;
        std::unordered_map<std::string, Response> m_ResponseBindings;
        std::unordered_map<std::string, Event> m_EventBindings;
    };
} // namespace dfs

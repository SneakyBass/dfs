#include <chrono>
#include <connection/login_message.pb.h>
#include <cstdint>
#include <cstdlib>
#include <fmt/base.h>
#include <fmt/color.h>
#include <fmt/core.h>
#include <game/chat.pb.h>
#include <game/common.pb.h>
#include <game/game_message.pb.h>
#include <game/gamemap.pb.h>
#include <game/interactive_element.pb.h>
#include <google/protobuf/any.pb.h>
#include <optional>
#include <string>
#include <unistd.h>

#include "bot-state.hh"
#include "bot.hh"
#include "game.hh"
#include "map.hh"
#include "messages.hh"
#include "utils.hh"

namespace dfs
{
    static bool s_Connected = false;

    template <typename T>
    using ProtoVec = google::protobuf::RepeatedPtrField<T>;

    using DofusActorPositionInformation = com::ankama::dofus::server::game::protocol::common::ActorPositionInformation;
    using DofusInteractiveElement = com::ankama::dofus::server::game::protocol::common::InteractiveElement;
    using DofusStatedElement = com::ankama::dofus::server::game::protocol::common::StatedElement;

    Messages::Messages() {
        ReadBindings();
    }

    static constexpr const auto MAP_MOVEMENT_REQUEST_TYPE_URL = "type.ankama.com/ifv";
    static constexpr const auto MAP_CHANGE_REQUEST_TYPE_URL = "type.ankama.com/iga";
    static constexpr const auto MAP_MOVEMENT_CONFIRM_REQUEST_TYPE_URL = "type.ankama.com/ifx";
    static constexpr const auto INTERACTIVE_USE_REQUEST_TYPE_URL = "type.ankama.com/hzk";

    void Messages::ReadBindings() {
        // Requests
        m_RequestBindings.emplace(MAP_MOVEMENT_REQUEST_TYPE_URL, MapMovementRequest);
        m_RequestBindings.emplace(MAP_MOVEMENT_CONFIRM_REQUEST_TYPE_URL, MapMovementConfirmRequest);
        m_RequestBindings.emplace(MAP_CHANGE_REQUEST_TYPE_URL, MapChangeRequest);
        m_RequestBindings.emplace("type.ankama.com/iyb", ChatChannelMessageRequest);
        m_RequestBindings.emplace(INTERACTIVE_USE_REQUEST_TYPE_URL, InteractiveUseRequest);
        m_RequestBindings.emplace("type.ankama.com/ige", MapInformationRequest);
        m_RequestBindings.emplace("type.ankama.com/iwu", PingRequest);

        // Responses
        m_ResponseBindings.emplace("type.ankama.com/egj", MapMovementConfirmResponse);

        // Events
        m_EventBindings.emplace("type.ankama.com/igg", MapMovementEvent);
        m_EventBindings.emplace("type.ankama.com/igh", MapChangeOrientationEvent);
        m_EventBindings.emplace("type.ankama.com/igi", MapCurrentEvent);
        m_EventBindings.emplace("type.ankama.com/igr", MapComplementaryInformationEvent);
        m_EventBindings.emplace("type.ankama.com/igs", GameRolePlayShowActorsEvent);
        m_EventBindings.emplace("type.ankama.com/iwv", PongEvent);
        m_EventBindings.emplace("type.ankama.com/jps", TimeEvent);
        m_EventBindings.emplace("type.ankama.com/iyp", CharacterCharacteristicsEvent);

        // Chat events
        m_EventBindings.emplace("type.ankama.com/iyc", ChatChannelMessageEvent);

        m_EventBindings.emplace("type.ankama.com/hzm", TreasureHuntLegendaryEvent);
        m_EventBindings.emplace("type.ankama.com/hem", TreasureHuntEvent);

        // Collectibles events
        m_EventBindings.emplace("type.ankama.com/hzn", InteractiveUseEndedEvent);
        m_EventBindings.emplace("type.ankama.com/hzl", InteractiveUseErrorEvent);
        m_EventBindings.emplace("type.ankama.com/hzq", InteractiveElementUpdatedEvent);
        m_EventBindings.emplace("type.ankama.com/hzr", StatedElementUpdatedEvent);
    }

    static std::vector<uint8_t> encode_uvarint(uint64_t value) {
        std::vector<uint8_t> encoded;
        encoded.reserve(sizeof(uint64_t) + value);

        while (value >= 0x80) {
            encoded.push_back(static_cast<uint8_t>(value) | 0x80);
            value >>= 7;
        }

        encoded.push_back(static_cast<uint8_t>(value));

        return encoded;
    }

    static bool HandleMapMovementRequest(const std::string &value, BotDescriptor *bot) {
        using namespace com::ankama::dofus::server::game::protocol::gamemap;

        if (bot->GetState().Active) {
            fmt::println("Ignoring movement request. The bot is in control. Type 'stop' in the chat to regain "
                         "control.");
            return true;
        }

        MapMovementRequest req;
        if (!req.ParseFromString(value)) {
            fmt::println(stderr, "Failed to parse MapMovementRequest");
            return false;
        }

        fmt::println("MapMovementRequest:");

        auto map = bot->GetState().CurrentMap.get();
        auto first_map_cell = PathElement::FromCompressed(req.key_cells(0));
        auto last_map_cell = PathElement::FromCompressed(req.key_cells(req.key_cells_size() - 1));
        auto path = map->GetShortestPath(first_map_cell.CellId, last_map_cell.CellId, true);

        fmt::print("  key_cells:");
        for (size_t i = 0; i < path.size(); i++) {
            fmt::print(" {} -> {}, ", path[i].CellId, (int)path[i].Dir);
        }
        fmt::print("\n");

        // Key cells
        fmt::print("  key_cells:");
        for (int i = 0; i < req.key_cells_size(); i++) {
            auto map_cell = PathElement::FromCompressed(req.key_cells(i));
            fmt::print(" {} -> {}, ", map_cell.CellId, static_cast<int>(map_cell.Dir));
        }

        fmt::println("\n");

        if ((int)path.size() != req.key_cells_size()) {
            fmt::print(fmt::fg(fmt::color::orange_red), "==== [KO] ==== Paths don't match\n");
        } else {
            auto i = 0;
            for (; i < req.key_cells_size(); i++) {
                if (req.key_cells(i) != path[i].ToCompressed()) {
                    break;
                }
            }

            if (i != req.key_cells_size()) {
                fmt::print(fmt::fg(fmt::color::orange_red), "==== [KO] ==== Paths don't match\n");
            } else {
                fmt::print(fmt::fg(fmt::color::lime_green), "==== [OK] ==== Paths match\n");
            }
        }

        // Map id
        fmt::println("  map_id: {}", req.map_id());

        // Cautious
        fmt::println("  cautious: {}", req.cautious());

        return false;
    }

    static bool HandleMapChangeRequest(const std::string &value, BotDescriptor *bot) {
        using namespace com::ankama::dofus::server::game::protocol::gamemap;

        if (bot->GetState().Active) {
            fmt::println("Ignoring map change request.");
            return true;
        }

        MapChangeRequest req;
        if (!req.ParseFromString(value)) {
            fmt::println(stderr, "Failed to parse MapChangeRequest");
            return false;
        }

        bot->GetState().ChangingMaps = true;

        fmt::println("MapChangeRequest:");
        fmt::println("  map_id: {}", req.map_id());
        fmt::println("  autopilot: {}", req.auto_pilot());
        return false;
    }

    static bool HandleMapMovementConfirmRequest(BotDescriptor *bot) {
        auto &state = bot->GetState();

        // Skip the client movement confirm request if the bot is running (we'll send it ourselves).
        if (state.Active)
            return true;

        fmt::println("Movement confirm request:");

        state.CurrentPlayer.Moving = false;
        state.CurrentPlayer.CurrentCell = state.CurrentPlayer.TargetCell;

        bot->MarkUpdated();
        return false;
    }

    static void HandleInteractiveUseRequest(const std::string &value, BotDescriptor *bot) {
        using namespace com::ankama::dofus::server::game::protocol::interactive;

        element::InteractiveUseRequest req;
        if (!req.ParseFromString(value)) {
            fmt::println(stderr, "Failed to parse InteractiveUseRequest");
            return;
        }

        fmt::println("InteractiveUseRequest:");
        fmt::println("  element_id: {}", req.element_id());
        fmt::println("  skill_instance_uid: {}", req.skill_instance_uid());
        if (req.specific_instance_id()) {
            fmt::println("  specific_instance_id: {}", req.specific_instance_id());
        }

        auto &state = bot->GetState();
        state.CurrentPlayer.Collecting = true;

        bot->MarkUpdated();
    }

    static bool HandleChatMessageRequest(const std::string &value, BotDescriptor *bot) {
        using namespace com::ankama::dofus::server::game::protocol::chat;

        ChatChannelMessageRequest req;
        if (!req.ParseFromString(value)) {
            fmt::println(stderr, "Failed to parse ChatChannelMessageRequest");
            return false;
        }

        fmt::println("Chat channel message request:");
        fmt::println("  channel: {}", (int)req.channel());
        fmt::println("  message: {}", req.content());

        auto &state = bot->GetState();
        bool updated = false;

        if (req.content() == "start") {
            fmt::println("Manually starting bot");
            state.Active = true;
            updated = true;
        } else if (req.content() == "stop") {
            fmt::println("Manually stopping bot");
            state.Active = false;
            updated = true;
        } else if (req.content() == "skip") {
            fmt::println("Clearing the collectible list");
            // Clear the list of collectibles to skip collecting this map
            state.Collectibles.clear();
            updated = true;
        } else if (req.content().starts_with("mt ")) {
            auto cell_id = atoi(req.content().substr(3, req.content().size() - 3).c_str());
            bot->MoveTo(cell_id);
            updated = true;
        }

        if (updated)
            bot->MarkUpdated();

        return updated;
    }

    bool Messages::ParseRequest(const com::ankama::dofus::server::game::protocol::Request &request,
                                BotDescriptor *bot) const {
        using namespace com::ankama::dofus::server::game::protocol;

        auto req = m_RequestBindings.find(request.content().type_url());
        if (req == m_RequestBindings.end()) {
            fmt::println("  REQ {} ({})", request.content().type_url(), request.content().value().length());
            return false;
        }

        auto cancel_request = false;

        switch (req->second) {
        case MapMovementRequest:
            cancel_request = HandleMapMovementRequest(request.content().value(), bot);
            break;
        case MapChangeRequest:
            cancel_request = HandleMapChangeRequest(request.content().value(), bot);
            break;
        case ChatChannelMessageRequest:
            cancel_request = HandleChatMessageRequest(request.content().value(), bot);
            break;
        case InteractiveUseRequest:
            HandleInteractiveUseRequest(request.content().value(), bot);
            break;
        case MapMovementConfirmRequest:
            cancel_request = HandleMapMovementConfirmRequest(bot);
            break;
        case MapInformationRequest:
            fmt::println("MapInformationRequest");
            break;
        case PingRequest:
            fmt::println("Ping Request");
            break;
        }

        return cancel_request;
    }

    void Messages::ParseResponse(const com::ankama::dofus::server::game::protocol::Response &response,
                                 BotDescriptor *bot) const {
        using namespace com::ankama::dofus::server::game::protocol;

        auto res = m_ResponseBindings.find(response.content().type_url());
        if (res == m_ResponseBindings.end()) {
            fmt::println("  RES {} ({})", response.content().type_url(), response.content().value().length());
            return;
        }

        switch (res->second) {
        case MapMovementConfirmResponse: {
            fmt::println("Position confirmed");

            auto &state = bot->GetState();
            state.CurrentPlayer.Moving = false;
            state.CurrentPlayer.CurrentCell = state.CurrentPlayer.TargetCell;

            bot->MarkUpdated();
        } break;
        }
    }

    static void HandleChatChannelMessageEvent(const std::string &value) {
        using namespace com::ankama::dofus::server::game::protocol::chat;

        ChatChannelMessageEvent evt;
        if (!evt.ParseFromString(value)) {
            fmt::println(stderr, "Failed to parse ChatChannelMessageEvent");
            return;
        }

        std::string channel;
        switch (evt.channel()) {
        case GLOBAL:
            channel = "GLOBAL";
            break;
        case TEAM:
            channel = "TEAM";
            break;
        case GUILD:
            channel = "GUILD";
            break;
        case PARTY:
            channel = "PARTY";
            break;
        case ALLIANCE:
        case SALES:
        case SEEK:
        case NOOB:
        case ADMIN:
        case ARENA:
        case PRIVATE:
        case INFO:
        case FIGHT_LOG:
        case ADS:
        case EVENT:
        case EXCHANGE:
        case Channel_INT_MIN_SENTINEL_DO_NOT_USE_:
        case Channel_INT_MAX_SENTINEL_DO_NOT_USE_:
            // TODO. Can't be fucked now.
            channel = "OTHER";
            break;
        }

        fmt::println("Received chat message on channel: {}: {}", channel, evt.content());
    }

    static void HandleMapMovementEvent(const std::string &value, BotDescriptor *bot) {
        using namespace com::ankama::dofus::server::game::protocol::gamemap;

        MapMovementEvent evt;
        if (!evt.ParseFromString(value)) {
            fmt::println(stderr, "Failed to parse MapMovementEvent");
            return;
        }

        if (evt.cells().size() < 2) {
            // You're not moving
            return;
        }

        std::vector<int32_t> cells;
        cells.reserve(evt.cells().size());

        for (auto i = 0; i < evt.cells().size(); i++)
            cells.push_back(evt.cells(i));

        auto &state = bot->GetState();

        auto arrival_time = std::chrono::high_resolution_clock::now();
        arrival_time += GetMovementDuration(cells, evt.cautious());

        if (state.CurrentPlayer.Id == evt.character_id()) {
            // It's us
            state.CurrentPlayer.Moving = true;
            state.CurrentPlayer.CurrentCell = evt.cells(0);
            state.CurrentPlayer.TargetCell = evt.cells(evt.cells().size() - 1);
            state.CurrentPlayer.ArrivalTime = arrival_time;
        } else if (state.OtherPlayers.find(evt.character_id()) != state.OtherPlayers.end()) {
            // It's some rando
            auto other = &state.OtherPlayers.find(evt.character_id())->second;
            other->Moving = true;
            other->CurrentCell = evt.cells(0);
            other->TargetCell = evt.cells(evt.cells().size() - 1);
            other->ArrivalTime = arrival_time;
        } else if (state.Monsters.find(evt.character_id()) != state.Monsters.end()) {
            auto monster = &state.Monsters.find(evt.character_id())->second;
            monster->Moving = true;
            monster->CurrentCell = evt.cells(0);
            monster->TargetCell = evt.cells(evt.cells().size() - 1);
            monster->ArrivalTime = arrival_time;
        } else {
            fmt::println("Character {} is not on the map?", evt.character_id());
            return;
        }

        // Move the actor
        if (state.Actors.find(evt.character_id()) != state.Actors.end()) {
            auto actor = &state.Actors.find(evt.character_id())->second;
            actor->Moving = true;
            actor->CurrentCell = evt.cells(0);
            actor->TargetCell = evt.cells(evt.cells().size() - 1);
            actor->ArrivalTime = arrival_time;
        }

        bot->MarkUpdated(arrival_time);
    }

    static void RegisterMonster(const DofusActorPositionInformation &actor, BotState &state) {
        Monster m{};
        m.Id = actor.actor_id();

        if (!actor.disposition().has_cell_id())
            return; // What the hell maaaaaan?

        m.CurrentCell = actor.disposition().cell_id();

        auto identification = actor.actor_information().role_play_actor().monster_group_actor().identification();

        m.EnnemyCount = 1; // The main creature
        m.EnnemyCount += identification.underlings_size();

        m.TotalLevel = identification.main_creature().level();

        for (auto i = 0; i < identification.underlings_size(); i++)
            m.TotalLevel += identification.underlings(i).level();

        state.Monsters.emplace(m.Id, m);
    }

    static void RegisterGenericActor(const DofusActorPositionInformation &actor, BotState &state) {
        GenericActor a{};
        a.Id = actor.actor_id();

        if (!actor.disposition().has_cell_id())
            return; // What the hell maaaaaan?

        a.CurrentCell = actor.disposition().cell_id();

        state.Actors.emplace(a.Id, a);
    }

    static void RegisterNPC(const DofusActorPositionInformation &actor, BotState &state) {
        GenericActor npc{};
        npc.Id = actor.actor_id();

        if (!actor.disposition().has_cell_id())
            return; // What the hell maaaaaan?

        npc.CurrentCell = actor.disposition().cell_id();

        state.NPCs.emplace(npc.Id, npc);
    }

    static void RegisterPlayer(const DofusActorPositionInformation &actor, BotState &state) {
        if (actor.actor_id() == state.CurrentPlayer.Id) {
            state.CurrentPlayer.CurrentCell = actor.disposition().cell_id();
            return;
        }

        Player p{};
        p.Id = actor.actor_id();

        if (!actor.disposition().has_cell_id())
            return; // What the hell maaaaaan?

        p.CurrentCell = actor.disposition().cell_id();

        auto named_actor = actor.actor_information().role_play_actor().named_actor();

        p.Name = named_actor.name();

        state.OtherPlayers.emplace(p.Id, p);
    }

    static bool RegisterActors(const ProtoVec<DofusActorPositionInformation> &actors, BotDescriptor *bot) {
        using namespace com::ankama::dofus::server::game::protocol::common;

        bool modified = false;

        if (actors.size() == 0)
            return modified;

        auto &state = bot->GetState();

        if (state.CurrentMap == nullptr)
            return modified;

        for (auto i = 0; i < actors.size(); i++) {
            auto actor = actors.at(i);

            if (!actor.has_actor_information())
                continue;

            RegisterGenericActor(actor, state);
            modified = true;

            // Isn't that pretty?
            switch (actor.actor_information().information_case()) {
            case ActorPositionInformation_ActorInformation::kRolePlayActor:
                switch (actor.actor_information().role_play_actor().actor_case()) {
                case ActorPositionInformation_ActorInformation_RolePlayActor::kNamedActor:
                    switch (actor.actor_information().role_play_actor().named_actor().actor_case()) {
                    case ActorPositionInformation_ActorInformation_RolePlayActor_NamedActor::kHumanoidInformation:
                        RegisterPlayer(actor, state);
                        break;
                    case ActorPositionInformation_ActorInformation_RolePlayActor_NamedActor::kMountInformation:
                        break;
                    case ActorPositionInformation_ActorInformation_RolePlayActor_NamedActor::ACTOR_NOT_SET:
                        break;
                    }
                    break;
                case ActorPositionInformation_ActorInformation_RolePlayActor::kTaxCollectorActor:
                case ActorPositionInformation_ActorInformation_RolePlayActor::kMonsterGroupActor:
                    RegisterMonster(actor, state);
                    break;
                case ActorPositionInformation_ActorInformation_RolePlayActor::kNpcActor:
                    RegisterNPC(actor, state);
                    break;
                case ActorPositionInformation_ActorInformation_RolePlayActor::kPrismActor:
                case ActorPositionInformation_ActorInformation_RolePlayActor::kPortalActor:
                case ActorPositionInformation_ActorInformation_RolePlayActor::kTreasureHuntNpcId:
                case ActorPositionInformation_ActorInformation_RolePlayActor::ACTOR_NOT_SET:
                    break;
                }
            case ActorPositionInformation_ActorInformation::kFighter:
                // Wtf is this? Maybe some info when we are in combat?
                break;
            case ActorPositionInformation_ActorInformation::INFORMATION_NOT_SET:
                break;
            }
        }

        return modified;
    }

    static bool RegisterInteractiveElements(const ProtoVec<DofusInteractiveElement> &interactive_elements,
                                            BotDescriptor *bot) {
        // Note: The interactive elements are known before the stated elements. Indeed, an interactive element
        // describes what's interactive that could be on our screen. I'll only get the collectibles, but
        // it may also (huge guess) be some other things like doors or shit like that. Idk and idc for now.
        //
        // We rely on stated elements to know if the collectible we want to register is available or not.

        bool modified = false;

        if (interactive_elements.size() == 0)
            return modified;

        auto &state = bot->GetState();

        for (auto i = 0; i < interactive_elements.size(); i++) {
            if (!interactive_elements.at(i).on_current_map())
                continue; // Get the fuck out

            Collectible c{};
            c.State = CollectibleState::Unknown;
            c.ElementTypeId = interactive_elements.at(i).element_type_id();
            c.Id = interactive_elements.at(i).element_id();

            for (auto j = 0; j < interactive_elements.at(i).enabled_skills_size(); j++) {
                InteractiveElementSkill s{};
                s.SkillId = interactive_elements.at(i).enabled_skills(j).skill_id();
                s.SkillInstanceUid = interactive_elements.at(i).enabled_skills(j).skill_instance_uid();

                c.EnabledSkills.push_back(s);
            }

            for (auto j = 0; j < interactive_elements.at(i).disabled_skills_size(); j++) {
                InteractiveElementSkill s{};
                s.SkillId = interactive_elements.at(i).disabled_skills(j).skill_id();
                s.SkillInstanceUid = interactive_elements.at(i).disabled_skills(j).skill_instance_uid();

                c.DisabledSkills.push_back(s);
            }

            state.Collectibles.emplace(c.Id, c);

            modified = true;
        }

        return modified;
    }

    static bool RegisterStatedElements(const ProtoVec<DofusStatedElement> &stated_elements, BotDescriptor *bot) {
        // Note: We should have everything we need in the map already because of the previous register of interactive
        // elements. Unless Ankama does some weird shit that I don't want to think about because I can't be fucked
        // with this shit. So here we'll juste populate the values of the interactive elements. What we want here
        // is actually the cell_id and the state of the resource (provided it's indeed a resource).

        bool modified = false;

        if (stated_elements.size() == 0)
            return modified;

        auto &state = bot->GetState();

        for (auto i = 0; i < stated_elements.size(); i++) {
            if (!stated_elements.at(i).on_current_map())
                continue;

            auto element = state.Collectibles.find(stated_elements.at(i).element_id());
            if (element == state.Collectibles.end())
                continue;

            element->second.CellId = stated_elements.at(i).cell_id();
            element->second.State = static_cast<CollectibleState>(stated_elements.at(i).state());
        }

        return modified;
    }

    static void HandleMapComplementaryInformationEvent(const std::string &value, BotDescriptor *bot) {
        using namespace com::ankama::dofus::server::game::protocol::gamemap;

        MapComplementaryInformationEvent evt;
        if (!evt.ParseFromString(value)) {
            fmt::println(stderr, "Failed to parse map complementary info");
            return;
        }

        bool modified = false;
        modified |= RegisterActors(evt.actors(), bot);
        modified |= RegisterInteractiveElements(evt.interactive_elements(), bot);
        modified |= RegisterStatedElements(evt.stated_elements(), bot);

        // We are not changing maps after we receive this message
        bot->GetState().ChangingMaps = false;

        if (evt.obstacles_size() > 0) {
            // We have fuking obstacles. What even is this shit?
            fmt::println("  obstacles #: {}", evt.obstacles_size());
        }

        fmt::println("There are {} players, {} monster groups, {} interactive elements",
                     bot->GetState().OtherPlayers.size(), bot->GetState().Monsters.size(),
                     bot->GetState().Collectibles.size());

        if (modified)
            bot->MarkUpdated();
    }

    void HandleMapChangeOrientationEvent(const std::string &value, BotDescriptor *bot) {
        using namespace com::ankama::dofus::server::game::protocol::gamemap;

        // Note: this happens when someone changes map

        MapChangeOrientationEvent evt;
        if (!evt.ParseFromString(value)) {
            fmt::println(stderr, "Failed to parse map change orientation event");
            return;
        }

        auto &state = bot->GetState();
        state.OtherPlayers.erase(evt.actor_id());
        state.Actors.erase(evt.actor_id());

        bot->MarkUpdated();
    }

    void HandleMapCurrentEvent(const std::string &value, BotDescriptor *bot) {
        using namespace com::ankama::dofus::server::game::protocol::gamemap;

        MapCurrentEvent evt;
        if (!evt.ParseFromString(value)) {
            fmt::println(stderr, "Failed to parse map current event");
            return;
        }

        // Clear the bot state because we changed maps.
        bot->ClearState();

        auto &state = bot->GetState();
        state.CurrentMap = state.Data.GetMap(evt.map_id());

        fmt::println(" === We are now on map {} ===", evt.map_id());

        bot->MarkUpdated();
    }

    void HandleGameRolePlayShowActorsEvent(const std::string &value, BotDescriptor *bot) {
        using namespace com::ankama::dofus::server::game::protocol::gamemap;

        GameRolePlayShowActorsEvent evt;
        if (!evt.ParseFromString(value)) {
            fmt::println(stderr, "Failed to parse GameRolePlayShowActorsEvent");
            return;
        }

        if (RegisterActors(evt.actors(), bot))
            bot->MarkUpdated();
    }

    void HandleInteractiveUsedEvent(const std::string &value, BotDescriptor *bot) {
        using namespace com::ankama::dofus::server::game::protocol::interactive::element;

        InteractiveUsedEvent evt;
        if (!evt.ParseFromString(value)) {
            fmt::println(stderr, "Failed to parse interactive used event");
            return;
        }

        auto &state = bot->GetState();

        // Either us or some shithead is interacting with some element (probably a collectible).
        if (evt.entity_id() == state.CurrentPlayer.CollectingId) {
            // That's cool
            state.CurrentPlayer.Collecting = true;

            // Why the fuck are the times not in milliseconds Ankama?
            state.CurrentPlayer.ArrivalTime =
                std::chrono::high_resolution_clock::now() + std::chrono::milliseconds(evt.duration() * 100);

            bot->MarkUpdated();
        }
    }

    void HandleInteractiveUseErrorEvent(const std::string &value, BotDescriptor *bot) {
        using namespace com::ankama::dofus::server::game::protocol::interactive::element;

        InteractiveUseErrorEvent evt;
        if (!evt.ParseFromString(value)) {
            fmt::println(stderr, "Failed to parse interactive use error event");
            return;
        }

        auto &state = bot->GetState();

        fmt::println("Interactive use error on entity {}", evt.element_id());

        auto elt = state.Collectibles.find(evt.element_id());
        if (elt != state.Collectibles.end()) {
            // Mark this element as non collectible
            elt->second.State = CollectibleState::Unknown;
        }

        state.CurrentPlayer.Collecting = false;
        bot->MarkUpdated();
    }

    void HandleInteractiveUseEndedEvent(const std::string &value, BotDescriptor *bot) {
        using namespace com::ankama::dofus::server::game::protocol::interactive::element;

        InteractiveUseEndedEvent evt;
        if (!evt.ParseFromString(value)) {
            fmt::println(stderr, "Failed to parse interactive use ended event");
            return;
        }

        auto &state = bot->GetState();

        fmt::println("Interactive use ended on entity {}", evt.element_id());

        auto now = std::chrono::high_resolution_clock::now();

        if (state.CurrentPlayer.ArrivalTime > now) {
            fmt::println(
                "We received a interactive use ended event but we have not finished collecting yet. Delta is {}ms",
                (state.CurrentPlayer.ArrivalTime - now) / std::chrono::milliseconds(1));
        }

        state.CurrentPlayer.Collecting = false;
        bot->MarkUpdated();
    }

    void HandleInteractiveElementUpdatedEvent(const std::string &value, BotDescriptor *bot) {
        using namespace com::ankama::dofus::server::game::protocol::interactive::element;

        InteractiveElementUpdatedEvent evt;
        if (!evt.ParseFromString(value)) {
            fmt::println(stderr, "Failed to parse interactive element updated event");
            return;
        }

        auto &state = bot->GetState();

        auto collectible = state.Collectibles.find(evt.interactive_element().element_id());
        if (collectible == state.Collectibles.end()) {
            fmt::println("Interactive element {} not found", evt.interactive_element().element_id());
            return;
        }

        collectible->second.EnabledSkills.clear();
        collectible->second.DisabledSkills.clear();

        for (auto j = 0; j < evt.interactive_element().enabled_skills_size(); j++) {
            InteractiveElementSkill s{};
            s.SkillId = evt.interactive_element().enabled_skills(j).skill_id();
            s.SkillInstanceUid = evt.interactive_element().enabled_skills(j).skill_instance_uid();

            collectible->second.EnabledSkills.push_back(s);
        }

        for (auto j = 0; j < evt.interactive_element().disabled_skills_size(); j++) {
            InteractiveElementSkill s{};
            s.SkillId = evt.interactive_element().disabled_skills(j).skill_id();
            s.SkillInstanceUid = evt.interactive_element().disabled_skills(j).skill_instance_uid();

            collectible->second.DisabledSkills.push_back(s);
        }

        fmt::println("Collectible (type {}) {} updated", collectible->second.Id, collectible->second.ElementTypeId);

        bot->MarkUpdated();
    }

    void HandleStatedElementUpdatedEvent(const std::string &value, BotDescriptor *bot) {
        using namespace com::ankama::dofus::server::game::protocol::interactive::element;

        StatedElementUpdatedEvent evt;
        if (!evt.ParseFromString(value)) {
            fmt::println(stderr, "Failed to parse stated element updated event");
            return;
        }

        auto &state = bot->GetState();

        auto collectible = state.Collectibles.find(evt.stated_element().element_id());
        if (collectible == state.Collectibles.end()) {
            fmt::println("Collectible {} not found", evt.stated_element().element_id());
            return;
        }

        collectible->second.State = static_cast<CollectibleState>(evt.stated_element().state());
        collectible->second.CellId = evt.stated_element().cell_id();

        fmt::println("Collectible {} (type {}) is now of state {}", collectible->second.Id,
                     collectible->second.ElementTypeId, (int)collectible->second.State);

        bot->MarkUpdated();
    }

    void Messages::ParseEvent(const com::ankama::dofus::server::game::protocol::Event &event,
                              BotDescriptor *bot) const {
        using namespace com::ankama::dofus::server::game::protocol;

        if (event.content().type_url().ends_with("jaz")) {
            fmt::println("We are in combat! Disabling bot.");
            auto &state = bot->GetState();
            state.Active = false;
            bot->GetState().InCombat = true;
            return;
        }

        auto req = m_EventBindings.find(event.content().type_url());
        if (req == m_EventBindings.end()) {
            fmt::println("  EVT {} ({})", event.content().type_url(), event.content().value().length());
            return;
        }

        switch (req->second) {
        case MapMovementEvent:
            HandleMapMovementEvent(event.content().value(), bot);
            break;
        case ChatChannelMessageEvent:
            HandleChatChannelMessageEvent(event.content().value());
            break;
        case MapComplementaryInformationEvent:
            HandleMapComplementaryInformationEvent(event.content().value(), bot);
            break;
        case MapChangeOrientationEvent:
            HandleMapChangeOrientationEvent(event.content().value(), bot);
            break;
        case MapCurrentEvent:
            HandleMapCurrentEvent(event.content().value(), bot);
            break;
        case GameRolePlayShowActorsEvent:
            HandleGameRolePlayShowActorsEvent(event.content().value(), bot);
            break;
        case InteractiveUsedEvent:
            HandleInteractiveUsedEvent(event.content().value(), bot);
            break;
        case InteractiveUseEndedEvent:
            HandleInteractiveUseEndedEvent(event.content().value(), bot);
            break;
        case StatedElementUpdatedEvent:
            HandleStatedElementUpdatedEvent(event.content().value(), bot);
            break;
        case InteractiveElementUpdatedEvent:
            HandleInteractiveElementUpdatedEvent(event.content().value(), bot);
            break;
        case TreasureHuntLegendaryEvent:
        case TreasureHuntEvent:
            break;
        case PongEvent:
            fmt::println("Pong event");
            break;
        case TimeEvent:
            fmt::println("Time event");
            break;
        case CharacterCharacteristicsEvent:
            fmt::println("Character characteristics event received");
            break;
        case InteractiveUseErrorEvent:
            HandleInteractiveUseErrorEvent(event.content().value(), bot);
            break;
        }
    }

    std::optional<std::string> Messages::HandleGameMessage(const uint8_t *payload, size_t length, int len_offset,
                                                           BotDescriptor *bot) const {
        using namespace com::ankama::dofus::server::game::protocol;

        std::string_view msg(reinterpret_cast<const char *>(payload + len_offset), length);

        GameMessage m;
        if (!m.ParseFromString(msg)) {
            fmt::println(stderr, "Failed to parse game message!");

            // Return the initial message
            std::string initial_message;
            initial_message.assign(reinterpret_cast<const char *>(payload), len_offset + length);

            return initial_message;
        }

        switch (m.content_case()) {
        case GameMessage::kRequest:
            // ParseRequest returns true if we want to cancel the request
            if (ParseRequest(m.request(), bot))
                return std::nullopt;
            break;
        case GameMessage::kResponse:
            ParseResponse(m.response(), bot);
            break;
        case GameMessage::kEvent:
            ParseEvent(m.event(), bot);
            break;
        case GameMessage::CONTENT_NOT_SET:
            break;
        }

        auto message = m.SerializeAsString();

        if (message.size() != length) {
            fmt::print(fmt::fg(fmt::color::orange_red),
                       "[WARNING] This message's length does not match the input length!\n");

            return std::string(msg);
        }

        // Compare the messages
        for (size_t i = 0; i < length; i++) {
            if (message[i] != msg[i]) {
                fmt::print(fmt::fg(fmt::color::orange_red),
                           "[WARNING] This message's does not match the input ! (position {})\n", i);
            }

            return std::string(msg);
        }

        return message;
    }

    static void HandleConnectionRequest(const com::ankama::dofus::server::connection::protocol::Request &req) {
        using namespace com::ankama::dofus::server::connection::protocol;

        switch (req.content_case()) {
        case Request::kPing:
            fmt::println("Connection Request: Ping");
            break;
        case Request::kIdentification:
            fmt::println("Connection Request: Identification");
            break;
        case Request::kSelectServer:
            fmt::println("Connection Request: SelectServer");
            break;
        case Request::kForceAccount:
            fmt::println("Connection Request: ForceAccount");
            break;
        case Request::kReleaseAccount:
            fmt::println("Connection Request: ReleaseAccount");
            break;
        case Request::kFriendListRequest:
            fmt::println("Connection Request: FriendListRequest");
            break;
        case Request::kAcquaintanceServersRequest:
            fmt::println("Connection Request: AcquaintanceServersRequest");
            break;
        case Request::CONTENT_NOT_SET:
            break;
        }
    }

    static void HandleConnectionResponse(const com::ankama::dofus::server::connection::protocol::Response &res) {
        using namespace com::ankama::dofus::server::connection::protocol;

        switch (res.content_case()) {
        case Response::kPong:
            fmt::println("Connection Response: Ping");
            break;
        case Response::kIdentification:
            fmt::println("Connection Response: Identification");
            break;
        case Response::kSelectServer:
            fmt::println("Connection Response: SelectServer");
            s_Connected = true;
            break;
        case Response::kForceAccount:
            fmt::println("Connection Response: ForceAccount");
            break;
        case Response::kFriendList:
            fmt::println("Connection Response: FriendListRequest");
            break;
        case Response::kAcquaintanceServersResponse:
            fmt::println("Connection Response: AcquaintanceServersRequest");
            break;
        case Response::CONTENT_NOT_SET:
            break;
        }
    }

    std::vector<uint8_t> Messages::HandleMessage(const uint8_t *payload, size_t length, int len_offset,
                                                 BotDescriptor *bot) const {
        // Make sure we don't make progress in the bot while reading/writing to state
        auto lock = bot->Lock();

        std::string message;

        if (s_Connected) {
            auto message_opt = HandleGameMessage(payload, length, len_offset, bot);
            if (message_opt == std::nullopt)
                return {};

            message = *message_opt;
        } else {
            message = HandleConnectionMessage(payload, length, len_offset, bot);
        }

        auto data = encode_uvarint(message.length());
        data.insert(data.end(), message.begin(), message.end());

        return data;
    }

    std::string Messages::HandleConnectionMessage(const uint8_t *payload, size_t length, int len_offset,
                                                  BotDescriptor *) const {
        using namespace com::ankama::dofus::server::connection::protocol;

        std::string_view msg(reinterpret_cast<const char *>(payload + len_offset), length);

        LoginMessage m;
        if (!m.ParseFromString(msg)) {
            fmt::println(stderr, "Failed to parse game message!");

            // Return the initial message
            std::string initial_message;
            initial_message.assign(reinterpret_cast<const char *>(payload), len_offset + length);

            return initial_message;
        }

        // Forwarding the message
        fmt::println("Forwarding client connection message");

        switch (m.content_case()) {
        case LoginMessage::kRequest:
            HandleConnectionRequest(m.request());
            break;
        case LoginMessage::kResponse:
            HandleConnectionResponse(m.response());
            break;
        case LoginMessage::kEvent:
            fmt::println("Message is of type event");
            break;
        case LoginMessage::CONTENT_NOT_SET:
            break;
        }

        return m.SerializeAsString();
    }

    std::vector<uint8_t> Messages::ForgeMapMovementRequest(const std::vector<PathElement> &path, int map_id,
                                                           bool cautious) {
        using namespace com::ankama::dofus::server::game::protocol;
        using namespace com::ankama::dofus::server::game::protocol::gamemap;
        using GameRequest = com::ankama::dofus::server::game::protocol::Request;

        class MapMovementRequest req;

        for (auto step : path)
            req.add_key_cells(step.ToCompressed());

        req.set_map_id(map_id);
        req.set_cautious(cautious);

        std::string message = req.SerializeAsString();
        google::protobuf::Any content;
        *content.mutable_value() = std::move(message);
        *content.mutable_type_url() = MAP_MOVEMENT_REQUEST_TYPE_URL;

        GameRequest request;
        *request.mutable_content() = std::move(content);

        GameMessage msg;
        *msg.mutable_request() = std::move(request);

        auto payload = msg.SerializeAsString();

        auto data = encode_uvarint(payload.length());
        data.insert(data.end(), payload.begin(), payload.end());

        return data;
    }

    std::vector<uint8_t> Messages::ForgeMapChangeRequest(int map_id, bool autopilot) {
        using namespace com::ankama::dofus::server::game::protocol;
        using namespace com::ankama::dofus::server::game::protocol::gamemap;
        using GameRequest = com::ankama::dofus::server::game::protocol::Request;

        class MapChangeRequest req;

        req.set_map_id(map_id);
        req.set_auto_pilot(autopilot);

        std::string message = req.SerializeAsString();
        google::protobuf::Any content;
        *content.mutable_value() = std::move(message);
        *content.mutable_type_url() = MAP_CHANGE_REQUEST_TYPE_URL;

        GameRequest request;
        *request.mutable_content() = std::move(content);

        GameMessage msg;
        *msg.mutable_request() = std::move(request);

        auto payload = msg.SerializeAsString();

        auto data = encode_uvarint(payload.length());
        data.insert(data.end(), payload.begin(), payload.end());

        return data;
    }

    std::vector<uint8_t> Messages::ForgeInteractiveUseRequest(int element_id, int skill_instance_uid) {
        using namespace com::ankama::dofus::server::game::protocol;
        using namespace com::ankama::dofus::server::game::protocol::interactive::element;
        using GameRequest = com::ankama::dofus::server::game::protocol::Request;

        class InteractiveUseRequest req;

        req.set_element_id(element_id);
        req.set_skill_instance_uid(skill_instance_uid);

        std::string message = req.SerializeAsString();
        google::protobuf::Any content;
        *content.mutable_value() = std::move(message);
        *content.mutable_type_url() = INTERACTIVE_USE_REQUEST_TYPE_URL;

        GameRequest request;
        *request.mutable_content() = std::move(content);

        GameMessage msg;
        *msg.mutable_request() = std::move(request);

        auto payload = msg.SerializeAsString();

        auto data = encode_uvarint(payload.length());
        data.insert(data.end(), payload.begin(), payload.end());

        return data;
    }

    std::vector<uint8_t> Messages::ForgeMapMovementConfirmRequest() {
        using namespace com::ankama::dofus::server::game::protocol;
        using namespace com::ankama::dofus::server::game::protocol::gamemap;
        using GameRequest = com::ankama::dofus::server::game::protocol::Request;

        class MapMovementConfirmRequest req;

        std::string message = req.SerializeAsString();
        google::protobuf::Any content;
        *content.mutable_value() = std::move(message);
        *content.mutable_type_url() = MAP_MOVEMENT_CONFIRM_REQUEST_TYPE_URL;

        GameRequest request;
        *request.mutable_content() = std::move(content);

        GameMessage msg;
        *msg.mutable_request() = std::move(request);

        auto payload = msg.SerializeAsString();

        auto data = encode_uvarint(payload.length());
        data.insert(data.end(), payload.begin(), payload.end());

        return data;
    }
} // namespace dfs

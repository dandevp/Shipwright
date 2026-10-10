#include <nlohmann/json.hpp>
#include <random>

#include "soh/Network/Anchor/Anchor.h"
#include "soh/Network/Anchor/CoopSettings.h"

extern "C" {
extern PlayState* gPlayState;
}

/**
 * UPDATE_ROOM_STATE
 */

nlohmann::json Anchor::PrepRoomState() {
    nlohmann::json payload;
    payload["ownerClientId"] = ownClientId;
    bool isGlobalRoom = (std::string("soh-global") == CVarGetString(CVAR_REMOTE_ANCHOR("RoomId"), ""));

    if (isGlobalRoom) {
        // Global room uses hardcoded settings
        payload["pvpMode"] = 0;
        payload["showLocationsMode"] = 0;
        payload["teleportMode"] = 0;
        payload["syncItemsAndFlags"] = 0;
        payload["coopSync"] = 0;
    } else {
        payload["pvpMode"] = CVarGetInteger(CVAR_REMOTE_ANCHOR("RoomSettings.PvpMode"), 1);
        payload["showLocationsMode"] = CVarGetInteger(CVAR_REMOTE_ANCHOR("RoomSettings.ShowLocationsMode"), 1);
        payload["teleportMode"] = CVarGetInteger(CVAR_REMOTE_ANCHOR("RoomSettings.TeleportMode"), 1);
        payload["syncItemsAndFlags"] = CVarGetInteger(CVAR_REMOTE_ANCHOR("RoomSettings.SyncItemsAndFlags"), 1);
        payload["coopSync"] = CVarGetInteger(CVAR_REMOTE_ANCHOR("RoomSettings.CoopSync"), 0);
        if (payload["coopSync"] != 0) {
            payload["coopSettings"] = PrepCoopSettings();
            payload["coopEnemySeed"] = GetCoopEnemySeed();
        }
    }

    return payload;
}

// Our values of the settings everyone in the room shares in co-op. Unset ones are left out, so everyone uses the
// default for them.
nlohmann::json Anchor::PrepCoopSettings() {
    nlohmann::json settings = nlohmann::json::object();
    for (const std::string& cvar : CoopSettings::GetSharedCVars()) {
        if (CVarGet(cvar.c_str()) != nullptr) {
            settings[cvar] = CVarGetInteger(cvar.c_str(), 0);
        }
    }
    return settings;
}

// The room's enemy seed, rolled once and kept so a room started again plays out the same
uint32_t Anchor::GetCoopEnemySeed() {
    uint32_t seed = CVarGetInteger(CVAR_REMOTE_ANCHOR("RoomSettings.CoopEnemySeed"), 0);
    if (seed == 0) {
        seed = std::random_device{}() | 1;
        CVarSetInteger(CVAR_REMOTE_ANCHOR("RoomSettings.CoopEnemySeed"), seed);
    }
    return seed;
}

// Called once a frame. The room owner's shared settings go out again when they change them in the menu.
void Anchor::UpdateCoopSettings() {
    CoopSettings::Update();

    if (!isConnected || roomState.ownerClientId != ownClientId ||
        !CVarGetInteger(CVAR_REMOTE_ANCHOR("RoomSettings.CoopSync"), 0)) {
        lastSentCoopSettings = nullptr;
        return;
    }

    nlohmann::json settings = PrepCoopSettings();
    if (settings != lastSentCoopSettings) {
        lastSentCoopSettings = settings;
        SendPacket_UpdateRoomState();
    }
}

void Anchor::SendPacket_UpdateRoomState() {
    nlohmann::json payload;
    payload["type"] = UPDATE_ROOM_STATE;
    payload["state"] = PrepRoomState();

    Network::SendJsonToRemote(payload);
}

void Anchor::HandlePacket_UpdateRoomState(nlohmann::json payload) {
    if (!payload.contains("state")) {
        return;
    }

    roomState.ownerClientId = payload["state"]["ownerClientId"].get<uint32_t>();
    roomState.pvpMode = payload["state"]["pvpMode"].get<u8>();
    roomState.showLocationsMode = payload["state"]["showLocationsMode"].get<u8>();
    roomState.teleportMode = payload["state"]["teleportMode"].get<u8>();
    roomState.syncItemsAndFlags = payload["state"]["syncItemsAndFlags"].get<u8>();
    // Rooms created by older clients don't have this setting
    roomState.coopSync = payload["state"].value("coopSync", (u8)0);
    roomState.coopSettings = payload["state"].value("coopSettings", nlohmann::json());
    roomState.coopEnemySeed = payload["state"].value("coopEnemySeed", (uint32_t)0);
}

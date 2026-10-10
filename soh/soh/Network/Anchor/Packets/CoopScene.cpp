#include <algorithm>
#include <nlohmann/json.hpp>

#include "soh/Network/Anchor/Anchor.h"
#include "soh/ResourceManagerHelpers.h"

extern "C" {
#include "functions.h"
extern PlayState* gPlayState;
}

/**
 * COOP_SCENE_FLAG, COOP_SCENE_SYNC_REQUEST, COOP_SCENE_SYNC
 *
 * When co-op sync is enabled for the room, teammates in the same scene share its live state (play->actorCtx.flags),
 * including the temporary flags that are never written to the save and so aren't covered by SET_FLAG.
 *
 * Switch flags 0x38-0x3F and temp clear flags are reset on every room transition, so they are only shared between
 * players in the same room. Everything else is shared across the whole scene.
 *
 * The flags that a boss fight began are shared too (if items and flags aren't synced anyway), so a teammate entering a
 * boss room after the fight started skips the intro and joins in.
 *
 * These packets are only sent to clients in the same scene and never queued on the server; a client entering a scene
 * or room catches up with COOP_SCENE_SYNC_REQUEST instead, which also returns the actors destroyed in its room (see
 * CoopActors.cpp).
 */

#define ROOM_SWITCH_FLAGS_MASK 0xFF000000 // tempSwch bits for switch flags 0x38-0x3F

static bool IsRoomScopedFlag(u8 coopFlagType, s16 flag) {
    return coopFlagType == COOP_FLAG_TEMP_CLEAR || (coopFlagType == COOP_FLAG_SWITCH && flag >= 0x38);
}

static void SetLiveFlagBit(u32* low, u32* high, s16 flag, bool set) {
    u32* field = flag < 0x20 ? low : high;
    u32 bit = 1u << (flag & 0x1F);

    if (set) {
        *field |= bit;
    } else {
        *field &= ~bit;
    }
}

nlohmann::json Anchor::PrepCoopPayload(const std::string& type) {
    nlohmann::json payload;
    payload["type"] = type;
    payload["sceneNum"] = gPlayState->sceneNum;
    payload["roomNum"] = gPlayState->roomCtx.curRoom.num;
    payload["isMasterQuest"] = (bool)ResourceMgr_IsSceneMasterQuest(gPlayState->sceneNum);
    return payload;
}

// Scene layouts differ between vanilla and Master Quest dungeons, so their flags and actors mean different things
bool Anchor::IsCoopPayloadForThisScene(nlohmann::json& payload) {
    if (!IsCoopSyncActive()) {
        return false;
    }

    s16 sceneNum = payload.at("sceneNum").get<s16>();
    return sceneNum == gPlayState->sceneNum &&
           payload.at("isMasterQuest").get<bool>() == (bool)ResourceMgr_IsSceneMasterQuest(sceneNum);
}

void Anchor::SendPacket_CoopSceneFlag(u8 coopFlagType, s16 flag, bool set) {
    // Flags applied from a packet must not be echoed back
    if (isProcessingIncomingPacket) {
        return;
    }

    std::vector<uint32_t> peers = GetCoopPeers(IsRoomScopedFlag(coopFlagType, flag));
    if (peers.empty()) {
        return;
    }

    nlohmann::json payload = PrepCoopPayload(COOP_SCENE_FLAG);
    payload["flagType"] = coopFlagType;
    payload["flag"] = flag;
    payload["set"] = set;

    for (uint32_t clientId : peers) {
        payload["targetClientId"] = clientId;
        SendJsonToRemote(payload);
    }
}

void Anchor::HandlePacket_CoopSceneFlag(nlohmann::json payload) {
    if (!IsCoopPayloadForThisScene(payload)) {
        return;
    }

    u8 coopFlagType = payload.at("flagType").get<u8>();
    s16 flag = payload.at("flag").get<s16>();
    bool set = payload.at("set").get<bool>();

    if (coopFlagType == COOP_FLAG_BOSS_BATTLE) {
        if (set && flag >= EVENTCHKINF_BEGAN_GOHMA_BATTLE && flag <= EVENTCHKINF_BEGAN_GANONDORF_BATTLE) {
            Flags_SetEventChkInf(flag);
        }
        return;
    }

    if (flag < 0 || flag >= 0x40) {
        return;
    }

    if (IsRoomScopedFlag(coopFlagType, flag) && payload.at("roomNum").get<s8>() != gPlayState->roomCtx.curRoom.num) {
        return;
    }

    // Write the live flags directly rather than through Flags_Set*, so the change doesn't fire the flag hooks and get
    // sent again. Persistent flags reach the save through Play_SaveSceneFlags when the scene is left.
    auto* flags = &gPlayState->actorCtx.flags;
    switch (coopFlagType) {
        case COOP_FLAG_SWITCH:
            SetLiveFlagBit(&flags->swch, &flags->tempSwch, flag, set);
            break;
        case COOP_FLAG_CLEAR:
            if (flag < 0x20) {
                SetLiveFlagBit(&flags->clear, &flags->clear, flag, set);
            }
            break;
        case COOP_FLAG_TEMP_CLEAR:
            if (flag < 0x20) {
                SetLiveFlagBit(&flags->tempClear, &flags->tempClear, flag, set);
            }
            break;
        case COOP_FLAG_COLLECTIBLE:
            // Only temp collectibles; permanent ones (heart pieces, keys, ...) belong to whoever picks them up
            if (flag >= 0x20) {
                SetLiveFlagBit(&flags->collect, &flags->tempCollect, flag, set);
            }
            break;
    }
}

void Anchor::SendPacket_CoopSceneSyncRequest() {
    // Prefer someone in our room, since only they can provide the room scoped flags
    std::vector<uint32_t> peers = GetCoopPeers(true);
    if (peers.empty()) {
        peers = GetCoopPeers(false);
    }
    if (peers.empty()) {
        return;
    }

    nlohmann::json payload = PrepCoopPayload(COOP_SCENE_SYNC_REQUEST);
    payload["targetClientId"] = *std::min_element(peers.begin(), peers.end());

    SendJsonToRemote(payload);
}

void Anchor::HandlePacket_CoopSceneSyncRequest(nlohmann::json payload) {
    if (!IsCoopPayloadForThisScene(payload)) {
        return;
    }

    SendPacket_CoopSceneSync(payload.at("clientId").get<uint32_t>(), payload.at("roomNum").get<s8>());
}

void Anchor::SendPacket_CoopSceneSync(uint32_t targetClientId, s8 roomNum) {
    auto* flags = &gPlayState->actorCtx.flags;

    nlohmann::json payload = PrepCoopPayload(COOP_SCENE_SYNC);
    payload["targetClientId"] = targetClientId;
    payload["swch"] = flags->swch;
    payload["tempSwch"] = flags->tempSwch;
    payload["clear"] = flags->clear;
    payload["tempClear"] = flags->tempClear;
    payload["tempCollect"] = flags->tempCollect;
    payload["destroyed"] = PrepCoopDestroyedActors(roomNum);
    payload["blocks"] = PrepCoopMovedBlocks(roomNum);
    payload["statues"] = PrepCoopTurnedStatues(roomNum);
    payload["torches"] = PrepCoopLitTorches(roomNum);
    payload["enemyHealth"] = PrepCoopEnemyHealth(roomNum);

    SendJsonToRemote(payload);
}

void Anchor::HandlePacket_CoopSceneSync(nlohmann::json payload) {
    if (!IsCoopPayloadForThisScene(payload)) {
        return;
    }

    auto* flags = &gPlayState->actorCtx.flags;
    bool sameRoom = payload.at("roomNum").get<s8>() == gPlayState->roomCtx.curRoom.num;

    // Permanent progress is merged, temporary state is taken over from the teammate who was already here
    flags->swch |= payload.at("swch").get<u32>();
    flags->clear |= payload.at("clear").get<u32>();
    flags->tempCollect |= payload.at("tempCollect").get<u32>();

    u32 tempSwch = payload.at("tempSwch").get<u32>();
    if (sameRoom) {
        flags->tempSwch = tempSwch;
        flags->tempClear = payload.at("tempClear").get<u32>();
    } else {
        flags->tempSwch = (flags->tempSwch & ROOM_SWITCH_FLAGS_MASK) | (tempSwch & ~ROOM_SWITCH_FLAGS_MASK);
    }

    if (payload.contains("destroyed")) {
        ApplyCoopDestroyedActors(payload["destroyed"]);
    }
    if (sameRoom && payload.contains("blocks")) {
        ApplyCoopMovedBlocks(payload["blocks"]);
    }
    if (sameRoom && payload.contains("statues")) {
        ApplyCoopTurnedStatues(payload["statues"]);
    }
    if (sameRoom && payload.contains("torches")) {
        ApplyCoopLitTorches(payload["torches"]);
    }
    if (sameRoom && payload.contains("enemyHealth")) {
        ApplyCoopEnemyHealth(payload["enemyHealth"]);
    }
}

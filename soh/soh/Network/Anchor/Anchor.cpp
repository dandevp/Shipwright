#include <nlohmann/json.hpp>

#include "Anchor.h"
#include "soh/Enhancements/nametag.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ObjectExtension/ObjectExtension.h"
#include "soh/OTRGlobals.h"
#include "soh/Enhancements/randomizer/SeedContext.h"

extern "C" {
#include "macros.h"
#include "variables.h"
#include "functions.h"
extern PlayState* gPlayState;
}

// MARK: - Overrides

void Anchor::Enable() {
    // Connecting and disconnecting happen on the network thread, but the game hooks are run (and so may only be
    // registered) on the game thread. They're registered at the start of the next frame instead.
    static bool sRegistersHooksOnGameThread = false;
    if (!sRegistersHooksOnGameThread) {
        sRegistersHooksOnGameThread = true;
        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>([]() {
            if (Anchor::Instance->hooksNeedRegistering.exchange(false)) {
                Anchor::Instance->RegisterHooks();
            }
            // Also when disconnected, so our own settings come back into effect
            Anchor::Instance->UpdateCoopSettings();
        });
    }

    Network::Enable(CVarGetString(CVAR_REMOTE_ANCHOR("Host"), "anchor.hm64.org"),
                    CVarGetInteger(CVAR_REMOTE_ANCHOR("Port"), 43383));
    ownClientId = CVarGetInteger(CVAR_REMOTE_ANCHOR("LastClientId"), 0);
    roomState.ownerClientId = 0;
}

void Anchor::Disable() {
    Network::Disable();

    clients.clear();
    RefreshClientActors();
}

void Anchor::OnConnected() {
    SendPacket_Handshake();
    hooksNeedRegistering = true;

    if (IsSaveLoaded()) {
        SendPacket_RequestTeamState();
    }
}

void Anchor::OnDisconnected() {
    hooksNeedRegistering = true;
}

void Anchor::ProcessOutgoingPackets() {
    // Copy all queued packets while holding the lock, then send them after releasing
    std::queue<nlohmann::json> packetsToSend;
    {
        std::lock_guard<std::mutex> lock(outgoingPacketQueueMutex);
        packetsToSend.swap(outgoingPacketQueue);
    }

    // Send packets without holding the lock
    while (!packetsToSend.empty()) {
        nlohmann::json payload = packetsToSend.front();
        packetsToSend.pop();

        if (!payload.contains("quiet")) {
            SPDLOG_DEBUG("[Anchor] Sending payload:\n{}", payload.dump());
        }
        Network::SendJsonToRemote(payload);
    }
}

void Anchor::SendJsonToRemote(nlohmann::json payload) {
    if (!isConnected) {
        return;
    }

    payload["clientId"] = ownClientId;
    if (!payload.contains("quiet")) {
        SPDLOG_DEBUG("[Anchor] Queuing payload:\n{}", payload.dump());
    }

    if (payload["type"] == HANDSHAKE) {
        Network::SendJsonToRemote(payload);
        return;
    }

    // Queue the packet to be sent on the network thread
    std::lock_guard<std::mutex> lock(outgoingPacketQueueMutex);
    outgoingPacketQueue.push(payload);
}

void Anchor::OnIncomingJson(nlohmann::json payload) {
    // If it doesn't contain a type, it's not a valid payload
    if (!payload.contains("type")) {
        return;
    }

    // If it's not a quiet payload, log it
    if (!payload.contains("quiet")) {
        SPDLOG_DEBUG("[Anchor] Received payload:\n{}", payload.dump());
    }

    std::string packetType = payload["type"].get<std::string>();

    // Ignore packets from mismatched clients, except for ALL_CLIENT_STATE, UPDATE_CLIENT_STATE, and PLAYER_UPDATE
    if (packetType != ALL_CLIENT_STATE && packetType != UPDATE_CLIENT_STATE && packetType != PLAYER_UPDATE) {
        if (payload.contains("clientId")) {
            uint32_t clientId = payload["clientId"].get<uint32_t>();
            if (clients.contains(clientId) && clients[clientId].clientVersion != clientVersion) {
                return;
            }
        }
    }

    // Queue all packets to be processed on the game thread
    std::lock_guard<std::mutex> lock(incomingPacketQueueMutex);
    incomingPacketQueue.push(payload);
}

void Anchor::ProcessIncomingPacketQueue() {
    // Copy all queued packets while holding the lock, then process them after releasing
    std::queue<nlohmann::json> packetsToProcess;
    {
        std::lock_guard<std::mutex> lock(incomingPacketQueueMutex);
        packetsToProcess.swap(incomingPacketQueue);
    }

    // Process packets without holding the lock
    while (!packetsToProcess.empty()) {
        nlohmann::json payload = packetsToProcess.front();
        packetsToProcess.pop();

        std::string packetType = payload["type"].get<std::string>();

        isProcessingIncomingPacket = true;

        try {
            // packetType here is a string so we can't use a switch statement
            if (packetType == ALL_CLIENT_STATE)
                HandlePacket_AllClientState(payload);
            else if (packetType == COOP_ACTOR_DESTROYED)
                HandlePacket_CoopActorDestroyed(payload);
            else if (packetType == COOP_BOSS_AIM)
                HandlePacket_CoopBossAim(payload);
            else if (packetType == COOP_BOSS_EVENT)
                HandlePacket_CoopBossEvent(payload);
            else if (packetType == COOP_BLOCK_PUSH)
                HandlePacket_CoopBlockPush(payload);
            else if (packetType == COOP_BLOCK_SETTLED)
                HandlePacket_CoopBlockSettled(payload);
            else if (packetType == COOP_ENEMY_ATTACK)
                HandlePacket_CoopEnemyAttack(payload);
            else if (packetType == COOP_ENEMY_HEALTH)
                HandlePacket_CoopEnemyHealth(payload);
            else if (packetType == COOP_ENEMY_HIT)
                HandlePacket_CoopEnemyHit(payload);
            else if (packetType == COOP_ENEMY_PROJECTILE)
                HandlePacket_CoopEnemyProjectile(payload);
            else if (packetType == COOP_ENEMY_SNAPSHOT)
                HandlePacket_CoopEnemySnapshot(payload);
            else if (packetType == COOP_ITEM_COLLECTED)
                HandlePacket_CoopItemCollected(payload);
            else if (packetType == COOP_ITEM_DROP)
                HandlePacket_CoopItemDrop(payload);
            else if (packetType == COOP_SCENE_FLAG)
                HandlePacket_CoopSceneFlag(payload);
            else if (packetType == COOP_SCENE_SYNC)
                HandlePacket_CoopSceneSync(payload);
            else if (packetType == COOP_SCENE_SYNC_REQUEST)
                HandlePacket_CoopSceneSyncRequest(payload);
            else if (packetType == COOP_SHOT_STATE)
                HandlePacket_CoopShotState(payload);
            else if (packetType == COOP_STATUE_SETTLED)
                HandlePacket_CoopStatueSettled(payload);
            else if (packetType == COOP_STATUE_TURN)
                HandlePacket_CoopStatueTurn(payload);
            else if (packetType == COOP_TORCH_LIT)
                HandlePacket_CoopTorchLit(payload);
            else if (packetType == DAMAGE_PLAYER)
                HandlePacket_DamagePlayer(payload);
            else if (packetType == DISABLE_ANCHOR)
                HandlePacket_DisableAnchor(payload);
            else if (packetType == ENTRANCE_DISCOVERED)
                HandlePacket_EntranceDiscovered(payload);
            else if (packetType == GAME_COMPLETE)
                HandlePacket_GameComplete(payload);
            else if (packetType == GIVE_ITEM)
                HandlePacket_GiveItem(payload);
            else if (packetType == OCARINA_SFX)
                HandlePacket_OcarinaSfx(payload);
            else if (packetType == PLAYER_UPDATE)
                HandlePacket_PlayerUpdate(payload);
            else if (packetType == PLAYER_SFX)
                HandlePacket_PlayerSfx(payload);
            else if (packetType == UPDATE_TEAM_STATE)
                HandlePacket_UpdateTeamState(payload);
            else if (packetType == REQUEST_TEAM_STATE)
                HandlePacket_RequestTeamState(payload);
            else if (packetType == REQUEST_TELEPORT)
                HandlePacket_RequestTeleport(payload);
            else if (packetType == SERVER_MESSAGE)
                HandlePacket_ServerMessage(payload);
            else if (packetType == SET_CHECK_STATUS)
                HandlePacket_SetCheckStatus(payload);
            else if (packetType == SET_FLAG)
                HandlePacket_SetFlag(payload);
            else if (packetType == TELEPORT_TO)
                HandlePacket_TeleportTo(payload);
            else if (packetType == UNSET_FLAG)
                HandlePacket_UnsetFlag(payload);
            else if (packetType == UPDATE_BEANS_COUNT)
                HandlePacket_UpdateBeansCount(payload);
            else if (packetType == UPDATE_CLIENT_STATE)
                HandlePacket_UpdateClientState(payload);
            else if (packetType == UPDATE_ROOM_STATE)
                HandlePacket_UpdateRoomState(payload);
            else if (packetType == UPDATE_DUNGEON_ITEMS)
                HandlePacket_UpdateDungeonItems(payload);
        } catch (const std::exception& e) {
            SPDLOG_ERROR("[Anchor] Exception while processing incoming packet {}", e.what());
            SPDLOG_ERROR("[Anchor] Packet: {}", payload.dump());
        }

        isProcessingIncomingPacket = false;
    }
}

// MARK: - Misc/Helpers

// Kills all existing anchor actors and respawns them with the new client data

struct DummyPlayerClientId {
    uint32_t clientId = 0;
};
static ObjectExtension::Register<DummyPlayerClientId> DummyPlayerClientIdRegister;

uint32_t Anchor::GetDummyPlayerClientId(const Actor* actor) {
    const DummyPlayerClientId* clientId = ObjectExtension::GetInstance().Get<DummyPlayerClientId>(actor);
    return clientId != nullptr ? clientId->clientId : 0;
}

void Anchor::SetDummyPlayerClientId(const Actor* actor, uint32_t clientId) {
    ObjectExtension::GetInstance().Set<DummyPlayerClientId>(actor, DummyPlayerClientId{ clientId });
}

void Anchor::RefreshClientActors() {
    if (!IsSaveLoaded()) {
        return;
    }

    Actor* actor = gPlayState->actorCtx.actorLists[ACTORCAT_NPC].head;

    while (actor != NULL) {
        if (actor->id == ACTOR_EN_OE2 && actor->update == DummyPlayer_Update) {
            NameTag_RemoveAllForActor(actor);
            Actor_Kill(actor);
        }
        actor = actor->next;
    }

    for (auto& [clientId, client] : clients) {
        if (!client.online || client.self) {
            continue;
        }

        spawningDummyPlayerForClientId = clientId;
        // We are using a hook `ShouldActorInit` to override the init/update/draw/destroy functions of the Player we
        // spawn We quickly store a mapping of "index" to clientId, then within the init function we use this to get the
        // clientId and store it on player->zTargetActiveTimer (unused s32 for the dummy) for convenience
        auto dummy =
            Actor_Spawn(&gPlayState->actorCtx, gPlayState, ACTOR_PLAYER, client.posRot.pos.x, client.posRot.pos.y,
                        client.posRot.pos.z, client.posRot.rot.x, client.posRot.rot.y, client.posRot.rot.z, 0);
        client.player = (Player*)dummy;
    }
    spawningDummyPlayerForClientId = 0;
}

bool Anchor::IsSaveLoaded() {
    if (gPlayState == nullptr) {
        return false;
    }

    if (GET_PLAYER(gPlayState) == nullptr) {
        return false;
    }

    if (gSaveContext.fileNum < 0 || gSaveContext.fileNum > 2) {
        return false;
    }

    if (gSaveContext.gameMode != GAMEMODE_NORMAL) {
        return false;
    }

    return true;
}

uint32_t Anchor::GetOwnSeed() {
    return IS_RANDO ? Rando::Context::GetInstance()->GetSeed() : 0;
}

bool Anchor::IsCoopSyncActive() {
    return isConnected && roomState.coopSync && IsSaveLoaded();
}

// A co-op peer is a teammate playing the same game (version, seed) in the same scene as us
bool Anchor::IsCoopPeer(const AnchorClient& client) {
    return !client.self && client.online && client.isSaveLoaded && client.clientVersion == clientVersion &&
           client.teamId == CVarGetString(CVAR_REMOTE_ANCHOR("TeamId"), "default") && client.seed == GetOwnSeed() &&
           client.sceneNum == gPlayState->sceneNum;
}

std::vector<uint32_t> Anchor::GetCoopPeers(bool sameRoomOnly) {
    std::vector<uint32_t> peers;

    if (!IsCoopSyncActive()) {
        return peers;
    }

    for (auto& [clientId, client] : clients) {
        if (IsCoopPeer(client) && (!sameRoomOnly || client.curRoomNum == gPlayState->roomCtx.curRoom.num)) {
            peers.push_back(clientId);
        }
    }

    return peers;
}

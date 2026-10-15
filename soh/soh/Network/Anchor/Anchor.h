#pragma once

#ifdef __cplusplus

#include "soh/Network/Network.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include <ship/window/gui/GuiWindow.h>
#include <spdlog/spdlog.h>
#include <atomic>
#include <queue>
#include <mutex>
#include <vector>
#include <map>
#include <set>
#include <chrono>

extern "C" {
#include "variables.h"
#include "z64.h"
}

void DummyPlayer_Init(Actor* actor, PlayState* play);
void DummyPlayer_Update(Actor* actor, PlayState* play);
void DummyPlayer_Draw(Actor* actor, PlayState* play);
void DummyPlayer_Destroy(Actor* actor, PlayState* play);

// Identifies a destructible actor (grass, pot, crate) or enemy across clients by its type and spawn position. Unlike
// the actor list index this is the same for every player, whatever their age or time of day, and it also covers actors
// that are spawned by other actors, like the grass in a grass circle.
struct CoopActorIdentity {
    s16 actorId;
    s16 x;
    s16 y;
    s16 z;
    s16 params = 0;     // Only for bosses, to tell apart the parts of one spawned in the same spot (Barinade)
    s16 childIndex = 0; // For enemies spawned in the same spot by another one (small Floormasters): their slot in it

    auto operator<=>(const CoopActorIdentity&) const = default;
};

inline void to_json(nlohmann::json& j, const CoopActorIdentity& identity) {
    j = { identity.actorId, identity.x, identity.y, identity.z, identity.params, identity.childIndex };
}

inline void from_json(const nlohmann::json& j, CoopActorIdentity& identity) {
    identity = { j.at(0).get<s16>(), j.at(1).get<s16>(), j.at(2).get<s16>(),
                 j.at(3).get<s16>(), j.at(4).get<s16>(), j.at(5).get<s16>() };
}

CoopActorIdentity GetCoopActorIdentity(Actor* actor);
void SetCoopChildIdentity(Actor* child, const CoopActorIdentity& identity);
void SetCoopChildSlot(Actor* child, Actor* parent, s16 slot);
bool GetCoopSpawnPos(Actor* actor, Vec3f* spawnPos);
Actor* FindCoopActor(const CoopActorIdentity& identity);
bool IsCoopEnemy(Actor* actor);
bool IsCoopBoss(Actor* actor);
bool IsCoopDrivenPuppet(Actor* actor);
bool IsCoopBossPart(Actor* actor);
void ResetCoopBossThreat();
void RecordCoopDamage(Actor* actor, s32 healthBefore, s32 healthAfter);
s32 GetCoopHealth(Actor* actor);
void SetCoopHealth(Actor* actor, s32 health);
bool IsRoomLoaded(s8 roomNum);

// Something a player carries or just threw (see CoopActors.cpp). Things placed in the room are matched up with the
// teammate's copy, anything else (bombs, ...) gets a copy of its own spawned.
struct CoopCarriedObject {
    uint32_t key; // Tells the things a player tracks apart
    bool isPlaced;
    CoopActorIdentity identity; // If placed
    s16 actorId;
    s16 params;
    Vec3f pos;
    Vec3s rot;
    f32 scale;      // If not placed (a bag bomb starts at 0 and grows in its own update, which copies don't run)
    bool isShot;    // Something they shot or threw (arrow, seed, Deku Nut, boomerang), shown until it's gone
    Vec3s worldRot; // If a shot
};

// A mod that changes gameplay, by its file name and a hash of its contents
struct CoopModInfo {
    std::string name;
    std::string hash;
};

typedef struct {
    uint32_t clientId;
    std::string name;
    Color_RGB8 color;
    std::string clientVersion;
    std::string teamId;
    bool online;
    bool self;
    uint32_t seed;
    std::string modelId;  // Their Link model, drawn on their dummy player (see PuppetModels.h), empty for vanilla
    uint32_t modelSize;   // Size of their model's archive, for asking before downloading it
    std::string modsHash; // Their gameplay mods, which have to match ours for co-op (see CoopMods.cpp)
    std::vector<CoopModInfo> gameplayMods;
    bool isSaveLoaded;
    bool isArchipelago; // Playing an Archipelago save, which never syncs items and flags
    bool isGameComplete;
    s16 sceneNum;
    s8 curRoomNum;
    s32 entranceIndex;

    // Only available in PLAYER_UPDATE packets
    s32 linkAge;
    PosRot posRot;
    Vec3s jointTable[24];
    u8 movementFlags;
    Vec3s prevTransl;
    Vec3s upperLimbRot;
    s8 currentBoots;
    s8 currentShield;
    s8 currentTunic;
    u32 stateFlags1;
    u32 stateFlags2;
    u8 buttonItem0;
    s8 itemAction;
    s8 heldItemAction;
    u8 modelGroup;
    s8 invincibilityTimer;
    f32 unk_85C;
    s16 unk_862;
    s8 actionVar1;
    u8 ocarinaNote;
    f32 ocarinaModulator;
    s8 ocarinaBend;

    // When the last PLAYER_UPDATE came in. These stop while the game is paused.
    std::chrono::steady_clock::time_point lastPlayerUpdate;

    // Whether they run the enemies of their room for everyone in it (see CoopEnemyMirror.cpp)
    bool claimsCoopAuthority;

    // Their game left out the boss of their scene, they haven't found its soul yet (see CoopEnemyMirror.cpp)
    bool isBossLeftOut;

    // The mirrored platform they're standing on, if any (see CoopEnemyMirror.cpp)
    bool isStandingOnCoopObject;
    CoopActorIdentity standingOnCoopObject;

    // What they're carrying or just threw
    std::vector<CoopCarriedObject> carriedCoopObjects;

    // Ptr to the dummy player
    Player* player;
} AnchorClient;

typedef struct {
    uint32_t ownerClientId;
    u8 pvpMode;                  // 0 = off, 1 = on, 2 = on with friendly fire
    u8 showLocationsMode;        // 0 = none, 1 = team, 2 = all
    u8 teleportMode;             // 0 = off, 1 = team, 2 = all
    u8 syncItemsAndFlags;        // 0 = off, 1 = on
    u8 coopSync;                 // 0 = off, 1 = on (share live room state with teammates in the same scene)
    nlohmann::json coopSettings; // The owner's values of the settings everyone shares in co-op (see CoopSettings.h)
    uint32_t coopEnemySeed;      // Seed for anything random about enemies, the same for everyone
} RoomState;

// Flag kinds shared through COOP_SCENE_FLAG. These are live scene flags (play->actorCtx.flags), independent of the
// FlagType enum used for save context flags.
typedef enum {
    COOP_FLAG_SWITCH,
    COOP_FLAG_CLEAR,
    COOP_FLAG_TEMP_CLEAR,
    COOP_FLAG_COLLECTIBLE,
    COOP_FLAG_BOSS_BATTLE, // EVENTCHKINF_BEGAN_*_BATTLE, so teammates joining a boss fight skip its intro
} CoopFlagType;

class Anchor : public Network {
  private:
    uint32_t spawningDummyPlayerForClientId = 0;
    bool shouldRefreshActors = false;
    bool justLoadedSave = false;
    // Our player's pose as it was last drawn, with the upper body animation (like carrying something) and blending
    // applied. Those only happen after all actors updated, after PLAYER_UPDATE is put together.
    Player* drawnPosePlayer = nullptr;
    Vec3s drawnPoseJointTable[24];
    bool isHandlingUpdateTeamState = false;
    bool isProcessingIncomingPacket = false;
    s8 lastCoopRoomNum = -1;
    std::map<CoopActorIdentity, s8> coopDestroyedActors; // Destroyed actors in loaded rooms, and their room
    Actor* coopDropSource = nullptr;                     // Actor currently breaking, whose drops get shared
    Actor* coopUpdatingActor = nullptr;                  // Actor whose update is currently running
    std::vector<Actor*> coopPendingDrops;                // Drops spawned this frame, shared at the end of it
    uint32_t coopNextDropId = 0;
    std::vector<Actor*> coopPendingProjectiles; // Shot this frame by mirrored enemies we run, shared at the end of it
    uint32_t coopNextShotId = 0;
    struct CoopPendingHit {
        nlohmann::json payload;
        u8 frames;                        // How long it has been waiting for its enemy to be hittable
        const char* dropReason = nullptr; // Why it couldn't land the last time it was tried, logged if it's dropped
    };
    std::vector<CoopPendingHit> coopPendingHits; // Teammates' hits on the puppets of enemies we run
    struct CoopTrackedCarry {
        Actor* actor;
        uint32_t key;
        u16 framesSinceRelease;
        bool isShot = false;
    };
    std::vector<CoopTrackedCarry> coopCarriedActors; // What our player carries or just threw, shown to teammates
    uint32_t coopNextCarryKey = 0;
    std::map<std::pair<uint32_t, uint32_t>, Actor*> coopCarriedCopies; // Our copies of teammates' bombs etc., by key
    bool coopNpcRefreshPending = false; // A teammate here changed the story, so NPCs may need to change too
    bool coopClaimsAuthority = false;   // We run the enemies of our room for everyone in it
    uint32_t coopRoomAuthority = 0;     // Who does, 0 while nobody does yet
    u32 coopRoomFrames = 0;             // Frames since we entered the room (or came back from a pause)
    std::chrono::steady_clock::time_point coopLastActiveTime;
    std::queue<nlohmann::json> incomingPacketQueue;
    std::mutex incomingPacketQueueMutex;
    std::queue<nlohmann::json> outgoingPacketQueue;
    std::mutex outgoingPacketQueueMutex;

    nlohmann::json PrepClientState();
    nlohmann::json PrepRoomState();
    nlohmann::json PrepCoopSettings();
    uint32_t GetCoopEnemySeed();
    nlohmann::json lastSentCoopSettings; // What we last sent as room owner, see UpdateCoopSettings
    void RegisterHooks();
    void RefreshClientActors();
    void SetDummyPlayerClientId(const Actor* actor, uint32_t clientId);
    bool IsCoopPeer(const AnchorClient& client);
    bool IsSameSeedPeer(const AnchorClient& client);
    bool IsFromSameSeedPeer(nlohmann::json& payload);
    std::vector<uint32_t> GetCoopPeers(bool sameRoomOnly, bool sameSeedOnly = false);
    nlohmann::json PrepCoopPayload(const std::string& type);
    bool IsCoopPayloadForThisScene(nlohmann::json& payload);
    void OnCoopRoomChanged();
    void OnCoopActorBroken(Actor* actor);
    void OnCoopItemSpawned(Actor* actor);
    void OnCoopActorUpdating(Actor* actor);
    void OnCoopActorUpdated(Actor* actor);
    void OnCoopActorKilled(Actor* actor);
    void OnCoopItemCollected(Actor* actor);
    void FlushCoopPendingDrops();
    bool ShouldCoopActorInit(Actor* actor);
    nlohmann::json PrepCoopDestroyedActors(s8 roomNum);
    void ApplyCoopDestroyedActors(nlohmann::json& destroyed, bool fromSameSeed);
    void UpdateCoopRoomAuthority();
    Actor* SpawnCoopPuppet(const CoopActorIdentity& identity, nlohmann::json& enemy);
    nlohmann::json PrepCoopEnemyHealth(s8 roomNum);
    void ApplyCoopEnemyHealth(nlohmann::json& enemies);
    void SetCoopEnemyHealth(const CoopActorIdentity& identity, s32 health);
    uint32_t GetCoopRoomAuthority();
    void OnCoopTorchUpdated(Actor* actor);
    void OnCoopBlockInit(Actor* actor);
    void OnCoopBlockUpdated(Actor* actor);
    nlohmann::json PrepCoopMovedBlocks(s8 roomNum);
    void ApplyCoopMovedBlocks(nlohmann::json& blocks);
    void OnCoopPushableUpdating(Actor* actor);
    void OnCoopStatueInit(Actor* actor);
    void OnCoopStatueUpdated(Actor* actor);
    nlohmann::json PrepCoopTurnedStatues(s8 roomNum);
    nlohmann::json PrepCoopLitTorches(s8 roomNum);
    void ApplyCoopLitTorches(nlohmann::json& torches);
    void ApplyCoopTurnedStatues(nlohmann::json& statues);
    void UnlockCoopDoors();
    bool UpdateCoopPuppet(Actor* actor);
    void CorrectCoopDrivenPuppet(Actor* actor);
    void UpdateCoopBossAim(Actor* actor);
    bool UpdateCoopShot(Actor* actor);
    void SendCoopShotState(Actor* actor, bool isRemoved = false);
    void ApplyCoopPendingHits();
    void UpdateCoopEnemyCulling();
    void RecordCoopOcColliders();
    bool ApplyCoopEnemyHit(nlohmann::json& payload, const char** dropReason);
    bool UpdateCoopEnemyWithTarget(Actor* actor);
    void OnCoopActorSpawned(Actor* actor);
    void FlushCoopPendingProjectiles();
    void DisarmCoopProjectileCopy(Actor* actor);
    void CorrectCoopMirroredObject(Actor* actor);
    void ApplyCoopTeammatesStandingOn(Actor* actor);
    void OnCoopFloormasterInit(Actor* actor);
    void SendCoopEnemySnapshots();
    struct CoopModelUpload {
        uint32_t clientId;
        std::string modelId;
        std::shared_ptr<std::vector<uint8_t>> data;
        size_t offset;
    };
    struct CoopModelDownload {
        uint32_t clientId;
        std::chrono::steady_clock::time_point lastChunkTime; // Dropped when stalled, like when the sender left
        std::vector<uint8_t> data;
    };
    std::string ownModelId;                             // Our Link model, see CoopModels.cpp
    std::shared_ptr<std::vector<uint8_t>> ownModelData; // Its archive, sent to teammates who ask
    std::atomic<bool> ownModelNeedsBuilding = true;     // Bundled once connected, and again when picked in the menu
    std::vector<CoopModelUpload> coopModelUploads;      // Teammates we're sending our model to, one at a time
    std::map<std::string, CoopModelDownload> coopModelDownloads; // Models we're receiving, by id
    std::set<std::string> coopModelsDeclined;                    // Models we were asked about, so we only ask once
    void OnCoopClientModel(uint32_t clientId);
    std::vector<CoopModInfo> ownGameplayMods; // See CoopMods.cpp
    std::string ownModsHash;
    std::set<std::string> coopModsAsked; // Room owners' mods we were asked to match, so we only ask once
    nlohmann::json PrepCoopMods();
    void OnCoopClientMods(uint32_t clientId);
    void SendCoopModelChunks();
    void SendPacket_ModelRequest(uint32_t clientId, const std::string& modelId);
    void AddCoopCarriedObject(nlohmann::json& payload);
    bool UpdateCoopCarriedByTeammate(Actor* actor);
    void UpdateCoopCarriedCopies();
    void ReadCoopCarriedObjects(AnchorClient& client, nlohmann::json& payload);
    void OnCoopTeammateFlagSet(uint32_t clientId, s16 flagType);
    void RefreshCoopNpcs();

    void HandlePacket_AllClientState(nlohmann::json payload);
    void HandlePacket_ConsumeAdultTradeItem(nlohmann::json payload);
    void HandlePacket_CoopActorDestroyed(nlohmann::json payload);
    void HandlePacket_CoopBlockPush(nlohmann::json payload);
    void HandlePacket_CoopBossAim(nlohmann::json payload);
    void HandlePacket_CoopBossEvent(nlohmann::json payload);
    void HandlePacket_CoopBlockSettled(nlohmann::json payload);
    void HandlePacket_CoopEnemyAttack(nlohmann::json payload);
    void HandlePacket_CoopEnemyHealth(nlohmann::json payload);
    void HandlePacket_CoopEnemyHit(nlohmann::json payload);
    void HandlePacket_CoopEnemyProjectile(nlohmann::json payload);
    void HandlePacket_CoopEnemySnapshot(nlohmann::json payload);
    void HandlePacket_CoopItemCollected(nlohmann::json payload);
    void HandlePacket_CoopItemDrop(nlohmann::json payload);
    void HandlePacket_CoopSceneFlag(nlohmann::json payload);
    void HandlePacket_CoopSceneSync(nlohmann::json payload);
    void HandlePacket_CoopShotState(nlohmann::json payload);
    void HandlePacket_CoopSceneSyncRequest(nlohmann::json payload);
    void HandlePacket_CoopStatueSettled(nlohmann::json payload);
    void HandlePacket_CoopStatueTurn(nlohmann::json payload);
    void HandlePacket_CoopTorchLit(nlohmann::json payload);
    void HandlePacket_DamagePlayer(nlohmann::json payload);
    void HandlePacket_DisableAnchor(nlohmann::json payload);
    void HandlePacket_EntranceDiscovered(nlohmann::json payload);
    void HandlePacket_GameComplete(nlohmann::json payload);
    void HandlePacket_GiveItem(nlohmann::json payload);
    void HandlePacket_OcarinaSfx(nlohmann::json payload);
    void HandlePacket_PlayerSfx(nlohmann::json payload);
    void HandlePacket_PlayerUpdate(nlohmann::json payload);
    void HandlePacket_RequestTeamState(nlohmann::json payload);
    void HandlePacket_RequestTeleport(nlohmann::json payload);
    void HandlePacket_ServerMessage(nlohmann::json payload);
    void HandlePacket_SetCheckStatus(nlohmann::json payload);
    void HandlePacket_SetFlag(nlohmann::json payload);
    void HandlePacket_TeleportTo(nlohmann::json payload);
    void HandlePacket_UnsetFlag(nlohmann::json payload);
    void HandlePacket_UpdateBeansCount(nlohmann::json payload);
    void HandlePacket_UpdateClientState(nlohmann::json payload);
    void HandlePacket_UpdateDungeonItems(nlohmann::json payload);
    void HandlePacket_UpdateRoomState(nlohmann::json payload);
    void HandlePacket_UpdateTeamState(nlohmann::json payload);

  public:
    uint32_t ownClientId;
    std::atomic<bool> hooksNeedRegistering = false; // See Enable
    inline static const std::string clientVersion = (char*)gGitCommitHash;

    // Packet types //
    inline static const std::string ALL_CLIENT_STATE = "ALL_CLIENT_STATE";
    inline static const std::string COOP_ACTOR_DESTROYED = "COOP_ACTOR_DESTROYED";
    inline static const std::string COOP_BLOCK_PUSH = "COOP_BLOCK_PUSH";
    inline static const std::string COOP_BOSS_AIM = "COOP_BOSS_AIM";
    inline static const std::string COOP_BOSS_EVENT = "COOP_BOSS_EVENT";
    inline static const std::string COOP_BLOCK_SETTLED = "COOP_BLOCK_SETTLED";
    inline static const std::string COOP_ENEMY_ATTACK = "COOP_ENEMY_ATTACK";
    inline static const std::string COOP_ENEMY_HEALTH = "COOP_ENEMY_HEALTH";
    inline static const std::string COOP_ENEMY_HIT = "COOP_ENEMY_HIT";
    inline static const std::string COOP_ENEMY_PROJECTILE = "COOP_ENEMY_PROJECTILE";
    inline static const std::string COOP_ENEMY_SNAPSHOT = "COOP_ENEMY_SNAPSHOT";
    inline static const std::string COOP_ITEM_COLLECTED = "COOP_ITEM_COLLECTED";
    inline static const std::string COOP_ITEM_DROP = "COOP_ITEM_DROP";
    inline static const std::string COOP_SCENE_FLAG = "COOP_SCENE_FLAG";
    inline static const std::string COOP_SCENE_SYNC = "COOP_SCENE_SYNC";
    inline static const std::string COOP_SCENE_SYNC_REQUEST = "COOP_SCENE_SYNC_REQUEST";
    inline static const std::string COOP_SHOT_STATE = "COOP_SHOT_STATE";
    inline static const std::string COOP_STATUE_SETTLED = "COOP_STATUE_SETTLED";
    inline static const std::string COOP_STATUE_TURN = "COOP_STATUE_TURN";
    inline static const std::string COOP_TORCH_LIT = "COOP_TORCH_LIT";
    inline static const std::string MODEL_CHUNK = "MODEL_CHUNK";
    inline static const std::string MODEL_REQUEST = "MODEL_REQUEST";
    inline static const std::string DAMAGE_PLAYER = "DAMAGE_PLAYER";
    inline static const std::string DISABLE_ANCHOR = "DISABLE_ANCHOR";
    inline static const std::string ENTRANCE_DISCOVERED = "ENTRANCE_DISCOVERED";
    inline static const std::string GAME_COMPLETE = "GAME_COMPLETE";
    inline static const std::string GIVE_ITEM = "GIVE_ITEM";
    inline static const std::string HANDSHAKE = "HANDSHAKE";
    inline static const std::string OCARINA_SFX = "OCARINA_SFX";
    inline static const std::string PLAYER_SFX = "PLAYER_SFX";
    inline static const std::string PLAYER_UPDATE = "PLAYER_UPDATE";
    inline static const std::string REQUEST_TEAM_STATE = "REQUEST_TEAM_STATE";
    inline static const std::string REQUEST_TELEPORT = "REQUEST_TELEPORT";
    inline static const std::string SERVER_MESSAGE = "SERVER_MESSAGE";
    inline static const std::string SET_CHECK_STATUS = "SET_CHECK_STATUS";
    inline static const std::string SET_FLAG = "SET_FLAG";
    inline static const std::string TELEPORT_TO = "TELEPORT_TO";
    inline static const std::string UNSET_FLAG = "UNSET_FLAG";
    inline static const std::string UPDATE_BEANS_COUNT = "UPDATE_BEANS_COUNT";
    inline static const std::string UPDATE_CLIENT_STATE = "UPDATE_CLIENT_STATE";
    inline static const std::string UPDATE_DUNGEON_ITEMS = "UPDATE_DUNGEON_ITEMS";
    inline static const std::string UPDATE_ROOM_STATE = "UPDATE_ROOM_STATE";
    inline static const std::string UPDATE_TEAM_STATE = "UPDATE_TEAM_STATE";

    static Anchor* Instance;
    uint32_t GetOwnSeed();
    std::map<uint32_t, AnchorClient> clients;
    RoomState roomState;

    void Enable();
    void Disable();
    void OnIncomingJson(nlohmann::json payload);
    void OnConnected();
    void OnDisconnected();
    void ProcessOutgoingPackets();
    void DrawMenu();
    void ProcessIncomingPacketQueue();
    void SendJsonToRemote(nlohmann::json packet);
    bool IsSaveLoaded();
    bool CanTeleportTo(uint32_t clientId);
    bool IsItemSyncActive();
    bool IsCoopSyncActive();
    bool IsCoopRoomAuthority();
    bool IsOwnBossLeftOut();
    bool IsCoopMirroredEnemy(Actor* actor);
    bool IsCoopMirroredAttacker(Actor* actor);
    bool GetCoopStandingOn(CoopActorIdentity* identity);
    std::string GetCoopActorDebugInfo(Actor* actor);
    uint32_t GetDummyPlayerClientId(const Actor* actor);

    void SendPacket_ClearTeamState(std::string teamId);
    void SendPacket_CoopActorDestroyed(Actor* actor);
    void SendPacket_CoopEnemyAttack(uint32_t clientId, Actor* attacker, ColliderInfo* attackInfo);
    void SendPacket_CoopEnemyHealth(Actor* actor);
    void SendPacket_CoopItemCollected(uint64_t dropId);
    void SendPacket_CoopItemDrop(EnItem00* item, uint64_t dropId);
    void SendPacket_CoopSceneFlag(u8 coopFlagType, s16 flag, bool set);
    void SendPacket_CoopSceneSync(uint32_t targetClientId, s8 roomNum);
    void SendPacket_CoopSceneSyncRequest();
    void SendPacket_DamagePlayer(u32 clientId, u8 damageEffect, u8 damage);
    void SendPacket_EntranceDiscovered(u16 entranceIndex);
    void SendPacket_GameComplete();
    void SendPacket_GiveItem(u16 modId, s16 getItemId);
    void SendPacket_Handshake();
    void SendPacket_OcarinaSfx(uint8_t note, float modulator, int8_t bend);
    void SendPacket_PlayerSfx(u16 sfxId);
    void SendPacket_PlayerUpdate();
    void SendPacket_RequestTeamState();
    void SendPacket_RequestTeleport(u32 clientId);
    void SendPacket_SetCheckStatus(RandomizerCheck rc);
    void SendPacket_SetFlag(s16 sceneNum, s16 flagType, s16 flag);
    void SendPacket_TeleportTo(u32 clientId);
    void SendPacket_UnsetFlag(s16 sceneNum, s16 flagType, s16 flag);
    void SendPacket_UpdateBeansCount();
    void SendPacket_UpdateClientState();
    void SendPacket_UpdateDungeonItems();
    void SendPacket_UpdateRoomState();
    struct CoopModelSource {
        std::string name; // File name of the mod
        size_t modelFiles;
        bool included;
    };
    std::vector<CoopModelSource> GetCoopModelSources();
    void SetCoopModelSourceIncluded(const std::string& name, bool included);
    size_t RestoreCoopDisabledMods();
    const std::string& GetOwnModelId() const {
        return ownModelId;
    }
    void BuildOwnCoopModel();
    static std::string ScanCoopMods(const std::vector<std::string>& archivePaths, std::vector<CoopModInfo>& mods);
    void UpdateOwnCoopModel();
    void HandlePacket_ModelRequest(nlohmann::json payload);
    void HandlePacket_ModelChunk(nlohmann::json payload);
    void UpdateCoopSettings();
    void SendPacket_UpdateTeamState();
};

typedef enum {
    // Starting at 5 to continue from the last value in the PlayerDamageResponseType enum
    DUMMY_PLAYER_HIT_RESPONSE_STUN = 5,
    DUMMY_PLAYER_HIT_RESPONSE_FIRE,
    DUMMY_PLAYER_HIT_RESPONSE_NORMAL,
} DummyPlayerDamageResponseType;

class AnchorRoomWindow : public Ship::GuiWindow {
  public:
    using GuiWindow::GuiWindow;

    void InitElement() override{};
    void DrawElement() override;
    void Draw() override;
    void UpdateElement() override{};
};

#endif // __cplusplus

#include <cmath>
#include <nlohmann/json.hpp>

#include "soh/Network/Anchor/Anchor.h"
#include "soh/Network/Anchor/JsonConversions.hpp"
#include "soh/ObjectExtension/ObjectExtension.h"

extern "C" {
#include "macros.h"
#include "functions.h"
#include "src/overlays/actors/ovl_Bg_Jya_Cobra/z_bg_jya_cobra.h"
#include "src/overlays/actors/ovl_En_Kanban/z_en_kanban.h"
#include "src/overlays/actors/ovl_Obj_Switch/z_obj_switch.h"
#include "src/overlays/actors/ovl_Obj_Oshihiki/z_obj_oshihiki.h"
#include "src/overlays/actors/ovl_Obj_Syokudai/z_obj_syokudai.h"
extern PlayState* gPlayState;
void func_80896918(BgJyaCobra* bgJyaCobra, PlayState* play); // Settle facing, go idle
void func_80896950(BgJyaCobra* bgJyaCobra, PlayState* play); // Idle, waiting to be pushed
void func_80896ABC(BgJyaCobra* bgJyaCobra, PlayState* play); // Turning
void ObjOshihiki_Fall(ObjOshihiki* objOshihiki, PlayState* play);
void ObjOshihiki_OnActor(ObjOshihiki* objOshihiki, PlayState* play);
void ObjOshihiki_OnScene(ObjOshihiki* objOshihiki, PlayState* play);
void ObjOshihiki_Push(ObjOshihiki* objOshihiki, PlayState* play);
void ObjOshihiki_SetupFall(ObjOshihiki* objOshihiki, PlayState* play);
void ObjOshihiki_SetupPush(ObjOshihiki* objOshihiki, PlayState* play);
void ObjSwitch_FloorUp(ObjSwitch* objSwitch, PlayState* play);
void ObjSwitch_FloorPressInit(ObjSwitch* objSwitch);
void ObjSwitch_FloorDown(ObjSwitch* objSwitch, PlayState* play);
void ObjSwitch_FloorReleaseInit(ObjSwitch* objSwitch);
}

/**
 * COOP_TORCH_LIT, COOP_SIGN_CUT, COOP_BLOCK_PUSH, COOP_BLOCK_SETTLED, COOP_STATUE_TURN, COOP_STATUE_SETTLED
 *
 * Torches: when a teammate in the room lights a torch, ours is lit too, by handing it the hit of a fire arrow. The
 * torch then lights itself as usual, including counting towards its group, so lighting a set of timed torches together
 * works and the puzzle's switch flag gets set by whoever lights the last one.
 *
 * Signs: when a teammate cuts a piece off a sign, ours is cut the same way, by handing it the hit of their sword (or
 * whatever they hit it with) along with their swing and where they stood. The piece then flies off by itself.
 *
 * Toggle floor switches (one press turns them on, the next off) only go down or up when they're stepped on, so when a
 * teammate toggles one, ours is moved to match its flag.
 *
 * Push blocks: when a teammate starts pushing a block, our copy is pushed the same way, so it slides (and falls) by
 * itself. When it comes to rest its position is shared once more, to correct it if the two copies got out of step.
 * A block's home position follows it around, so blocks are identified by where they were first spawned.
 *
 * Mirror statues (the cobra statues in the Spirit Temple) turn by eighths, and are synced the same way as push blocks.
 *
 * Only one player can push a block or turn a statue at a time: whoever starts first holds it until it comes to rest,
 * and for everyone else it won't budge in the meantime. If two players start in the same frame, the one with the lower
 * client id wins and the other's push is undone.
 */

#define FIRE_ARROW_DMG_FLAGS 0x800

struct CoopTorchState {
    s16 lastLitTimer;
    bool isBeingLitByTeammate;
};
static ObjectExtension::Register<CoopTorchState> CoopTorchStateRegister;

struct CoopBlockState {
    Vec3f spawnPos;
    ObjOshihikiActionFunc lastActionFunc = nullptr;
    bool isReplayingPush = false;
    bool hasSettlePos = false; // Where the teammate's copy came to rest, applied once ours does
    Vec3f settlePos;
};
static ObjectExtension::Register<CoopBlockState> CoopBlockStateRegister;

// Used in place of GetCoopActorIdentity's home position for blocks, which move their home as they're pushed
bool GetCoopSpawnPos(Actor* actor, Vec3f* spawnPos) {
    const CoopBlockState* block = ObjectExtension::GetInstance().Get<CoopBlockState>(actor);
    if (block == nullptr) {
        return false;
    }

    *spawnPos = block->spawnPos;
    return true;
}

static bool IsBlockAtRest(ObjOshihiki* block) {
    return block->actionFunc == ObjOshihiki_OnScene || block->actionFunc == ObjOshihiki_OnActor;
}

struct CoopStatueState {
    s16 spawnFacing;
    BgJyaCobraActionFunc lastActionFunc = nullptr;
    bool isReplayingTurn = false;
    bool hasSettleFacing = false; // Where the teammate's copy came to rest, applied once ours does
    s16 settleFacing;
};
static ObjectExtension::Register<CoopStatueState> CoopStatueStateRegister;

#define PUSH_LOCK_TIMEOUT_FRAMES 100 // In case the teammate holding it disappears before it comes to rest

struct CoopPushLock {
    uint32_t ownerClientId;
    u32 expiresAtFrame;
};
static ObjectExtension::Register<CoopPushLock> CoopPushLockRegister;

static void LockForTeammate(Actor* actor, uint32_t clientId) {
    ObjectExtension::GetInstance().Set<CoopPushLock>(
        actor, CoopPushLock{ clientId, gPlayState->gameplayFrames + PUSH_LOCK_TIMEOUT_FRAMES });
}

static void UnlockForTeammate(Actor* actor, uint32_t clientId) {
    CoopPushLock* lock = ObjectExtension::GetInstance().Get<CoopPushLock>(actor);
    if (lock != nullptr && lock->ownerClientId == clientId) {
        lock->ownerClientId = 0;
    }
}

// Keeps us from pushing a block or turning a statue that a teammate is moving. The player pushes by setting the push
// force on it, which is cleared here before it gets to act on it.
void Anchor::OnCoopPushableUpdating(Actor* actor) {
    CoopPushLock* lock = ObjectExtension::GetInstance().Get<CoopPushLock>(actor);
    if (lock == nullptr || lock->ownerClientId == 0 || gPlayState->gameplayFrames >= lock->expiresAtFrame) {
        return;
    }

    DynaPolyActor* dyna = (DynaPolyActor*)actor;
    if (fabsf(dyna->unk_150) > 0.001f) {
        dyna->unk_150 = 0.0f;
        GET_PLAYER(gPlayState)->stateFlags2 &= ~PLAYER_STATE2_MOVING_DYNAPOLY;
    }
}

// MARK: - Torches

void Anchor::OnCoopTorchUpdated(Actor* actor) {
    ObjSyokudai* torch = (ObjSyokudai*)actor;
    CoopTorchState* state = ObjectExtension::GetInstance().Get<CoopTorchState>(actor);
    if (state == nullptr) {
        ObjectExtension::GetInstance().Set<CoopTorchState>(actor, CoopTorchState{ torch->litTimer, false });
        return;
    }

    // Just lit by us (rather than by a teammate's packet)
    if (state->lastLitTimer == 0 && torch->litTimer != 0 && !state->isBeingLitByTeammate && IsCoopSyncActive()) {
        std::vector<uint32_t> peers = GetCoopPeers(true);
        nlohmann::json payload = PrepCoopPayload(COOP_TORCH_LIT);
        payload["actor"] = GetCoopActorIdentity(actor);
        for (uint32_t clientId : peers) {
            payload["targetClientId"] = clientId;
            SendJsonToRemote(payload);
        }
    }

    state->isBeingLitByTeammate = false;
    state->lastLitTimer = torch->litTimer;
}

static void LightCoopTorch(Actor* actor) {
    if (actor == nullptr || actor->id != ACTOR_OBJ_SYOKUDAI || ((ObjSyokudai*)actor)->litTimer != 0) {
        return;
    }

    // Packets are processed at the end of the frame, so the torch sees this hit in its next update
    static ColliderInfo sFireArrowInfo = {};
    sFireArrowInfo.toucher.dmgFlags = FIRE_ARROW_DMG_FLAGS;

    ObjSyokudai* torch = (ObjSyokudai*)actor;
    torch->colliderFlame.base.acFlags |= AC_HIT;
    torch->colliderFlame.info.acHitInfo = &sFireArrowInfo;

    CoopTorchState* state = ObjectExtension::GetInstance().Get<CoopTorchState>(actor);
    if (state != nullptr) {
        state->isBeingLitByTeammate = true;
    }
}

void Anchor::HandlePacket_CoopTorchLit(nlohmann::json payload) {
    if (!IsCoopPayloadForThisScene(payload) || payload.at("roomNum").get<s8>() != gPlayState->roomCtx.curRoom.num) {
        return;
    }

    LightCoopTorch(FindCoopActor(payload.at("actor").get<CoopActorIdentity>()));
}

// MARK: - Signs

#define ENKANBAN_STANDING 0 // A sign's actionState while it stands (ENKANBAN_SIGN)
#define SIGN_CUT_TIMEOUT_FRAMES 30

// Cuts teammates made on their copy of a sign, waiting to be made on ours
struct CoopSignCuts {
    struct Cut {
        u32 dmgFlags;
        s8 meleeWeaponAnimation; // Which way the sword swung, which decides the piece cut off
        s16 yaw;                 // From the sign to the teammate, the piece flies away from them
        u32 expiresAtFrame;
    };
    std::vector<Cut> cuts;
};
static ObjectExtension::Register<CoopSignCuts> CoopSignCutsRegister;

// The sign whose update is making a teammate's cut, and our player's swing to put back after it
static Actor* sCoopCuttingSign = nullptr;
static s8 sCoopSavedMeleeWeaponAnimation;

static void EndCoopSignCut() {
    if (sCoopCuttingSign != nullptr) {
        GET_PLAYER(gPlayState)->meleeWeaponAnimation = sCoopSavedMeleeWeaponAnimation;
        sCoopCuttingSign = nullptr;
    }
}

// Pieces cut off a sign are spawned where it stands, so they're told apart by their params
static Actor* FindCoopSign(const CoopActorIdentity& identity) {
    for (s32 i = 0; i < ACTORCAT_MAX; i++) {
        for (Actor* actor = gPlayState->actorCtx.actorLists[i].head; actor != NULL; actor = actor->next) {
            if (actor->update != NULL && actor->id == ACTOR_EN_KANBAN && actor->params != ENKANBAN_PIECE &&
                GetCoopActorIdentity(actor) == identity) {
                return actor;
            }
        }
    }
    return nullptr;
}

// Called before a sign updates. Its update cuts a piece off when it was hit (and the previous cut is over), working out
// which piece from our player's swing and where they stand, so a teammate's cut is made with theirs.
void Anchor::OnCoopSignUpdating(Actor* actor) {
    EndCoopSignCut();
    EnKanban* sign = (EnKanban*)actor;
    if (actor->params == ENKANBAN_PIECE || sign->actionState != ENKANBAN_STANDING || !IsCoopSyncActive() ||
        sign->invincibilityTimer > 1) {
        return;
    }

    // Our own hit, cut in our game this frame
    if (sign->collider.base.acFlags & AC_HIT) {
        std::vector<uint32_t> peers = GetCoopPeers(true);
        if (!IsCoopOwnedByUs(sign->collider.base.ac) || sign->collider.info.acHitInfo == nullptr || peers.empty()) {
            return;
        }
        nlohmann::json payload = PrepCoopPayload(COOP_SIGN_CUT);
        payload["actor"] = GetCoopActorIdentity(actor);
        payload["dmgFlags"] = sign->collider.info.acHitInfo->toucher.dmgFlags;
        payload["swing"] = GET_PLAYER(gPlayState)->meleeWeaponAnimation;
        payload["yaw"] = actor->yawTowardsPlayer;
        for (uint32_t clientId : peers) {
            payload["targetClientId"] = clientId;
            SendJsonToRemote(payload);
        }
        return;
    }

    CoopSignCuts* pending = ObjectExtension::GetInstance().Get<CoopSignCuts>(actor);
    if (pending == nullptr) {
        return;
    }
    std::erase_if(pending->cuts, [](const auto& cut) { return gPlayState->gameplayFrames > cut.expiresAtFrame; });
    if (pending->cuts.empty()) {
        return;
    }

    CoopSignCuts::Cut cut = pending->cuts.front();
    pending->cuts.erase(pending->cuts.begin());
    static ColliderInfo sTeammateHitInfo = {};
    sTeammateHitInfo.toucher.dmgFlags = cut.dmgFlags;
    sign->collider.base.acFlags |= AC_HIT;
    sign->collider.info.acHitInfo = &sTeammateHitInfo;
    actor->yawTowardsPlayer = cut.yaw;
    Player* player = GET_PLAYER(gPlayState);
    sCoopSavedMeleeWeaponAnimation = player->meleeWeaponAnimation;
    player->meleeWeaponAnimation = cut.meleeWeaponAnimation;
    sCoopCuttingSign = actor;
}

void Anchor::OnCoopSignUpdated(Actor* actor) {
    if (actor == sCoopCuttingSign) {
        EndCoopSignCut();
    }
}

void Anchor::HandlePacket_CoopSignCut(nlohmann::json payload) {
    if (!IsCoopPayloadForThisScene(payload) || payload.at("roomNum").get<s8>() != gPlayState->roomCtx.curRoom.num) {
        return;
    }

    Actor* sign = FindCoopSign(payload.at("actor").get<CoopActorIdentity>());
    s8 swing = payload.at("swing").get<s8>();
    if (sign == nullptr || swing < 0 || swing >= PLAYER_MWA_MAX) {
        return;
    }

    CoopSignCuts* pending = ObjectExtension::GetInstance().Get<CoopSignCuts>(sign);
    if (pending == nullptr) {
        ObjectExtension::GetInstance().Set<CoopSignCuts>(sign, CoopSignCuts{});
        pending = ObjectExtension::GetInstance().Get<CoopSignCuts>(sign);
    }
    pending->cuts.push_back({ payload.at("dmgFlags").get<u32>(), swing, payload.at("yaw").get<s16>(),
                              gPlayState->gameplayFrames + SIGN_CUT_TIMEOUT_FRAMES });
}

// MARK: - Toggle switches

// Called after a floor switch updated. A toggle switch (OBJSWITCH_SUBTYPE_FLOOR_1) whose flag a teammate changed is
// pressed down or let up to match, the way it would be if it had been toggled in our game. Like the Lake Hylia water
// switch: otherwise ours would stay up with the water already lowered, and our next press would only bring it down.
void Anchor::UpdateCoopToggleSwitch(Actor* actor) {
    ObjSwitch* floorSwitch = (ObjSwitch*)actor;
    s32 type = actor->params & 7;
    if (!IsCoopSyncActive() || (type != OBJSWITCH_TYPE_FLOOR && type != OBJSWITCH_TYPE_FLOOR_RUSTY) ||
        ((actor->params >> 4) & 7) != OBJSWITCH_SUBTYPE_FLOOR_1) {
        return;
    }

    bool isOn = Flags_GetSwitch(gPlayState, (actor->params >> 8) & 0x3F);
    if (isOn && floorSwitch->actionFunc == ObjSwitch_FloorUp) {
        floorSwitch->cooldownOn = false;
        ObjSwitch_FloorPressInit(floorSwitch);
    } else if (!isOn && floorSwitch->actionFunc == ObjSwitch_FloorDown) {
        floorSwitch->cooldownOn = false;
        ObjSwitch_FloorReleaseInit(floorSwitch);
    }
}

// MARK: - Push blocks

// Moves a block straight to where a teammate's copy is, letting it fall into place if it's above the floor there
static void PlaceCoopBlock(ObjOshihiki* block, Vec3f pos) {
    Actor* actor = &block->dyna.actor;
    if (fabsf(actor->world.pos.x - pos.x) < 1.0f && fabsf(actor->world.pos.z - pos.z) < 1.0f &&
        fabsf(actor->world.pos.y - pos.y) < 1.0f) {
        return;
    }

    actor->world.pos = pos;
    actor->home.pos = pos;
    block->dyna.unk_150 = 0.0f;
    block->pushDist = 0.0f;
    block->pushSpeed = 0.0f;
    ObjOshihiki_SetupFall(block, gPlayState);
}

void Anchor::OnCoopBlockInit(Actor* actor) {
    ObjectExtension::GetInstance().Set<CoopBlockState>(actor, CoopBlockState{ actor->home.pos });
}

void Anchor::OnCoopBlockUpdated(Actor* actor) {
    ObjOshihiki* block = (ObjOshihiki*)actor;
    CoopBlockState* state = ObjectExtension::GetInstance().Get<CoopBlockState>(actor);
    if (state == nullptr) {
        return;
    }

    ObjOshihikiActionFunc lastActionFunc = state->lastActionFunc;
    state->lastActionFunc = block->actionFunc;
    if (lastActionFunc == block->actionFunc || !IsCoopSyncActive()) {
        return;
    }

    // Our replay of a teammate's push came to rest
    if (state->isReplayingPush) {
        if (IsBlockAtRest(block)) {
            state->isReplayingPush = false;
            if (state->hasSettlePos) {
                state->hasSettlePos = false;
                PlaceCoopBlock(block, state->settlePos);
            }
        }
        return;
    }

    std::vector<uint32_t> peers = GetCoopPeers(true);
    if (peers.empty()) {
        return;
    }

    nlohmann::json payload;
    if (block->actionFunc == ObjOshihiki_Push) {
        // The home position is only moved at the end of a push, so it's still where the push started
        payload = PrepCoopPayload(COOP_BLOCK_PUSH);
        payload["from"] = actor->home.pos;
        payload["yaw"] = block->dyna.unk_158;
        payload["direction"] = block->direction;
    } else if (IsBlockAtRest(block) && (lastActionFunc == ObjOshihiki_Push || lastActionFunc == ObjOshihiki_Fall)) {
        payload = PrepCoopPayload(COOP_BLOCK_SETTLED);
        payload["pos"] = actor->world.pos;
    } else {
        return;
    }

    payload["actor"] = GetCoopActorIdentity(actor);
    for (uint32_t clientId : peers) {
        payload["targetClientId"] = clientId;
        SendJsonToRemote(payload);
    }
}

void Anchor::HandlePacket_CoopBlockPush(nlohmann::json payload) {
    if (!IsCoopPayloadForThisScene(payload) || payload.at("roomNum").get<s8>() != gPlayState->roomCtx.curRoom.num) {
        return;
    }

    Actor* actor = FindCoopActor(payload.at("actor").get<CoopActorIdentity>());
    if (actor == nullptr || actor->id != ACTOR_OBJ_OSHIHIKI) {
        return;
    }

    ObjOshihiki* block = (ObjOshihiki*)actor;
    CoopBlockState* state = ObjectExtension::GetInstance().Get<CoopBlockState>(actor);
    if (state == nullptr) {
        return;
    }

    // We started pushing it at the same time. The lower client id keeps their push, the other one is undone.
    uint32_t clientId = payload.at("clientId").get<uint32_t>();
    if (block->actionFunc == ObjOshihiki_Push && !state->isReplayingPush && clientId < ownClientId) {
        actor->world.pos.x = actor->home.pos.x;
        actor->world.pos.z = actor->home.pos.z;
        GET_PLAYER(gPlayState)->stateFlags2 &= ~PLAYER_STATE2_MOVING_DYNAPOLY;
    } else if (!IsBlockAtRest(block)) {
        return;
    }

    // Start from the same spot as the teammate's copy, then push it the same way
    Vec3f from = payload.at("from").get<Vec3f>();
    if (fabsf(actor->home.pos.x - from.x) >= 1.0f || fabsf(actor->home.pos.z - from.z) >= 1.0f) {
        actor->home.pos.x = actor->world.pos.x = from.x;
        actor->home.pos.z = actor->world.pos.z = from.z;
    }

    block->dyna.unk_158 = payload.at("yaw").get<s16>();
    block->direction = payload.at("direction").get<f32>();
    block->pushDist = 0.0f;
    block->pushSpeed = 0.0f;
    block->timer = 0;
    state->isReplayingPush = true;
    state->hasSettlePos = false;
    ObjOshihiki_SetupPush(block, gPlayState);
    LockForTeammate(actor, clientId);
}

void Anchor::HandlePacket_CoopBlockSettled(nlohmann::json payload) {
    if (!IsCoopPayloadForThisScene(payload) || payload.at("roomNum").get<s8>() != gPlayState->roomCtx.curRoom.num) {
        return;
    }

    Actor* actor = FindCoopActor(payload.at("actor").get<CoopActorIdentity>());
    if (actor == nullptr || actor->id != ACTOR_OBJ_OSHIHIKI) {
        return;
    }

    ObjOshihiki* block = (ObjOshihiki*)actor;
    CoopBlockState* state = ObjectExtension::GetInstance().Get<CoopBlockState>(actor);
    if (state == nullptr) {
        return;
    }

    UnlockForTeammate(actor, payload.at("clientId").get<uint32_t>());

    // Still moving, check once it comes to rest
    if (!IsBlockAtRest(block)) {
        state->hasSettlePos = true;
        state->settlePos = payload.at("pos").get<Vec3f>();
        return;
    }

    PlaceCoopBlock(block, payload.at("pos").get<Vec3f>());
}

// MARK: - Mirror statues

static void SetCoopStatueFacing(BgJyaCobra* statue, s16 facing) {
    if (statue->unk_16C != facing) {
        statue->unk_16C = facing & 7;
        func_80896918(statue, gPlayState);
    }
}

void Anchor::OnCoopStatueInit(Actor* actor) {
    ObjectExtension::GetInstance().Set<CoopStatueState>(actor, CoopStatueState{ ((BgJyaCobra*)actor)->unk_16C });
}

void Anchor::OnCoopStatueUpdated(Actor* actor) {
    BgJyaCobra* statue = (BgJyaCobra*)actor;
    CoopStatueState* state = ObjectExtension::GetInstance().Get<CoopStatueState>(actor);
    if (state == nullptr) {
        return;
    }

    BgJyaCobraActionFunc lastActionFunc = state->lastActionFunc;
    state->lastActionFunc = statue->actionFunc;
    if (lastActionFunc == statue->actionFunc || !IsCoopSyncActive()) {
        return;
    }

    bool turnEnded = statue->actionFunc == func_80896950 && lastActionFunc == func_80896ABC;

    // Our replay of a teammate's turn came to rest
    if (state->isReplayingTurn) {
        if (turnEnded) {
            state->isReplayingTurn = false;
            if (state->hasSettleFacing) {
                state->hasSettleFacing = false;
                SetCoopStatueFacing(statue, state->settleFacing);
            }
        }
        return;
    }

    std::vector<uint32_t> peers = GetCoopPeers(true);
    if (peers.empty()) {
        return;
    }

    nlohmann::json payload;
    if (statue->actionFunc == func_80896ABC) {
        payload = PrepCoopPayload(COOP_STATUE_TURN);
        payload["from"] = statue->unk_16C;
        payload["turn"] = statue->unk_16A;
    } else if (turnEnded) {
        payload = PrepCoopPayload(COOP_STATUE_SETTLED);
        payload["facing"] = statue->unk_16C;
    } else {
        return;
    }

    payload["actor"] = GetCoopActorIdentity(actor);
    for (uint32_t clientId : peers) {
        payload["targetClientId"] = clientId;
        SendJsonToRemote(payload);
    }
}

void Anchor::HandlePacket_CoopStatueTurn(nlohmann::json payload) {
    if (!IsCoopPayloadForThisScene(payload) || payload.at("roomNum").get<s8>() != gPlayState->roomCtx.curRoom.num) {
        return;
    }

    Actor* actor = FindCoopActor(payload.at("actor").get<CoopActorIdentity>());
    CoopStatueState* state = actor != nullptr ? ObjectExtension::GetInstance().Get<CoopStatueState>(actor) : nullptr;
    if (state == nullptr || actor->id != ACTOR_BG_JYA_COBRA) {
        return;
    }

    // We started turning it at the same time. The lower client id keeps their turn, the other one is undone.
    BgJyaCobra* statue = (BgJyaCobra*)actor;
    uint32_t clientId = payload.at("clientId").get<uint32_t>();
    if (statue->actionFunc == func_80896ABC && !state->isReplayingTurn && clientId < ownClientId) {
        GET_PLAYER(gPlayState)->stateFlags2 &= ~PLAYER_STATE2_MOVING_DYNAPOLY;
        func_80896918(statue, gPlayState);
    } else if (statue->actionFunc != func_80896950) {
        return;
    }

    // Start from the same facing as the teammate's copy, then turn it the same way (like func_808969F8, which would
    // also drag our player along)
    SetCoopStatueFacing(statue, payload.at("from").get<s16>());
    statue->actionFunc = func_80896ABC;
    statue->unk_16A = payload.at("turn").get<s16>();
    statue->unk_170 = 0;
    statue->unk_172 = false;
    statue->unk_16E = CVarGetInteger(CVAR_ENHANCEMENT("FasterBlockPush"), 0) * 20;
    state->isReplayingTurn = true;
    state->hasSettleFacing = false;
    LockForTeammate(actor, clientId);
}

void Anchor::HandlePacket_CoopStatueSettled(nlohmann::json payload) {
    if (!IsCoopPayloadForThisScene(payload) || payload.at("roomNum").get<s8>() != gPlayState->roomCtx.curRoom.num) {
        return;
    }

    Actor* actor = FindCoopActor(payload.at("actor").get<CoopActorIdentity>());
    CoopStatueState* state = actor != nullptr ? ObjectExtension::GetInstance().Get<CoopStatueState>(actor) : nullptr;
    if (state == nullptr || actor->id != ACTOR_BG_JYA_COBRA) {
        return;
    }

    UnlockForTeammate(actor, payload.at("clientId").get<uint32_t>());

    // Still turning, check once it comes to rest
    BgJyaCobra* statue = (BgJyaCobra*)actor;
    s16 facing = payload.at("facing").get<s16>();
    if (statue->actionFunc != func_80896950) {
        state->hasSettleFacing = true;
        state->settleFacing = facing;
        return;
    }

    SetCoopStatueFacing(statue, facing);
}

// MARK: - Room catch up (sent in COOP_SCENE_SYNC)

nlohmann::json Anchor::PrepCoopMovedBlocks(s8 roomNum) {
    nlohmann::json blocks = nlohmann::json::array();

    for (Actor* actor = gPlayState->actorCtx.actorLists[ACTORCAT_PROP].head; actor != NULL; actor = actor->next) {
        CoopBlockState* state = ObjectExtension::GetInstance().Get<CoopBlockState>(actor);
        if (actor->id != ACTOR_OBJ_OSHIHIKI || state == nullptr || !IsBlockAtRest((ObjOshihiki*)actor) ||
            actor->room != roomNum) {
            continue;
        }

        if (fabsf(actor->world.pos.x - state->spawnPos.x) >= 1.0f ||
            fabsf(actor->world.pos.z - state->spawnPos.z) >= 1.0f ||
            fabsf(actor->world.pos.y - state->spawnPos.y) >= 1.0f) {
            blocks.push_back({ { "actor", GetCoopActorIdentity(actor) }, { "pos", actor->world.pos } });
        }
    }

    return blocks;
}

void Anchor::ApplyCoopMovedBlocks(nlohmann::json& blocks) {
    for (auto& entry : blocks) {
        Actor* actor = FindCoopActor(entry.at("actor").get<CoopActorIdentity>());
        if (actor != nullptr && actor->id == ACTOR_OBJ_OSHIHIKI && IsBlockAtRest((ObjOshihiki*)actor)) {
            PlaceCoopBlock((ObjOshihiki*)actor, entry.at("pos").get<Vec3f>());
        }
    }
}

nlohmann::json Anchor::PrepCoopTurnedStatues(s8 roomNum) {
    nlohmann::json statues = nlohmann::json::array();

    for (Actor* actor = gPlayState->actorCtx.actorLists[ACTORCAT_PROP].head; actor != NULL; actor = actor->next) {
        CoopStatueState* state = ObjectExtension::GetInstance().Get<CoopStatueState>(actor);
        if (actor->id != ACTOR_BG_JYA_COBRA || state == nullptr || actor->room != roomNum ||
            ((BgJyaCobra*)actor)->actionFunc != func_80896950) {
            continue;
        }

        if (((BgJyaCobra*)actor)->unk_16C != state->spawnFacing) {
            statues.push_back(
                { { "actor", GetCoopActorIdentity(actor) }, { "facing", ((BgJyaCobra*)actor)->unk_16C } });
        }
    }

    return statues;
}

void Anchor::ApplyCoopTurnedStatues(nlohmann::json& statues) {
    for (auto& entry : statues) {
        Actor* actor = FindCoopActor(entry.at("actor").get<CoopActorIdentity>());
        if (actor != nullptr && actor->id == ACTOR_BG_JYA_COBRA && ((BgJyaCobra*)actor)->actionFunc == func_80896950) {
            SetCoopStatueFacing((BgJyaCobra*)actor, entry.at("facing").get<s16>());
        }
    }
}

// Torches burning on a timer, so a teammate entering in the middle of a timed torch puzzle can help finish it. Their
// timers start over, which is close enough. Torches that stay lit follow the puzzle's switch flag already.
nlohmann::json Anchor::PrepCoopLitTorches(s8 roomNum) {
    nlohmann::json torches = nlohmann::json::array();

    for (Actor* actor = gPlayState->actorCtx.actorLists[ACTORCAT_PROP].head; actor != NULL; actor = actor->next) {
        if (actor->id == ACTOR_OBJ_SYOKUDAI && actor->room == roomNum && ((ObjSyokudai*)actor)->litTimer > 0) {
            torches.push_back(GetCoopActorIdentity(actor));
        }
    }

    return torches;
}

void Anchor::ApplyCoopLitTorches(nlohmann::json& torches) {
    for (auto& entry : torches) {
        LightCoopTorch(FindCoopActor(entry.get<CoopActorIdentity>()));
    }
}

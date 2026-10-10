#include <cmath>
#include <nlohmann/json.hpp>

#include "soh/Network/Anchor/Anchor.h"
#include "soh/Network/Anchor/JsonConversions.hpp"
#include "soh/ObjectExtension/ObjectExtension.h"

extern "C" {
#include "macros.h"
#include "functions.h"
#include "src/overlays/actors/ovl_Bg_Jya_Cobra/z_bg_jya_cobra.h"
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
}

/**
 * COOP_TORCH_LIT, COOP_BLOCK_PUSH, COOP_BLOCK_SETTLED, COOP_STATUE_TURN, COOP_STATUE_SETTLED
 *
 * Torches: when a teammate in the room lights a torch, ours is lit too, by handing it the hit of a fire arrow. The
 * torch then lights itself as usual, including counting towards its group, so lighting a set of timed torches together
 * works and the puzzle's switch flag gets set by whoever lights the last one.
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

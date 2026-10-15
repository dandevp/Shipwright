#include <algorithm>
#include <set>
#include <nlohmann/json.hpp>

#include "soh/Network/Anchor/Anchor.h"
#include "soh/Network/Anchor/JsonConversions.hpp"
#include "soh/ObjectExtension/ObjectExtension.h"
#include "soh/ObjectExtension/ActorListIndex.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"

extern "C" {
#include "macros.h"
#include "functions.h"
#include "src/overlays/actors/ovl_Boss_Dodongo/z_boss_dodongo.h"
#include "src/overlays/actors/ovl_Boss_Mo/z_boss_mo.h"
#include "src/overlays/actors/ovl_Boss_Va/z_boss_va.h"
#include "src/overlays/actors/ovl_En_Arrow/z_en_arrow.h"
#include "src/overlays/actors/ovl_En_Bom/z_en_bom.h"
#include "src/overlays/actors/ovl_En_Boom/z_en_boom.h"
#include "src/overlays/actors/ovl_En_Bombf/z_en_bombf.h"
#include "src/overlays/actors/ovl_En_Ishi/z_en_ishi.h"
#include "src/overlays/actors/ovl_En_Kusa/z_en_kusa.h"
#include "src/overlays/actors/ovl_Obj_Kibako/z_obj_kibako.h"
#include "src/overlays/actors/ovl_Obj_Kibako2/z_obj_kibako2.h"
#include "src/overlays/actors/ovl_Obj_Tsubo/z_obj_tsubo.h"
extern PlayState* gPlayState;
extern u8* gBossVaFightPhase;
void EnArrow_Fly(EnArrow* enArrow, PlayState* play);
void EnKusa_Main(EnKusa* enKusa, PlayState* play);
void EnKusa_SetupCut(EnKusa* enKusa);
void EnKusa_SpawnBugs(EnKusa* enKusa, PlayState* play);
void EnKusa_SpawnFragments(EnKusa* enKusa, PlayState* play);
void EnIshi_Wait(EnIshi* enIshi, PlayState* play);
void EnIshi_SpawnFragmentsSmall(EnIshi* enIshi, PlayState* play);
void EnIshi_SpawnFragmentsLarge(EnIshi* enIshi, PlayState* play);
void EnIshi_SpawnDustSmall(EnIshi* enIshi, PlayState* play);
void EnIshi_SpawnDustLarge(EnIshi* enIshi, PlayState* play);
void ObjKibako_AirBreak(ObjKibako* objKibako, PlayState* play);
void ObjKibako_Idle(ObjKibako* objKibako, PlayState* play);
void ObjKibako2_Break(ObjKibako2* objKibako2, PlayState* play);
void ObjKibako2_Idle(ObjKibako2* objKibako2, PlayState* play);
void ObjTsubo_AirBreak(ObjTsubo* objTsubo, PlayState* play);
void ObjTsubo_Idle(ObjTsubo* objTsubo, PlayState* play);
}

/**
 * COOP_ACTOR_DESTROYED, COOP_ENEMY_HEALTH, COOP_ITEM_DROP, COOP_ITEM_COLLECTED
 *
 * When a player breaks grass, a pot, a crate or a rock, it breaks for teammates in the same room too. Its drops are
 * rolled once by the player who broke it and appear for everyone, and whoever picks one up first removes it for the
 * others.
 *
 * Enemies placed in the room share their health: every hit lowers it for everyone, and when one dies it dies for
 * everyone, with the killer's drops shared like above. Each player still runs the enemy's AI against themselves, so
 * enemies aren't in the same place for everyone; that needs per enemy support (see CoopEnemyMirror.cpp).
 *
 * Bosses share their health too, but not their death: it never goes below 1 from a teammate's hit, so everyone deals
 * the final blow in their own game and gets their own defeat cutscene, heart container and blue warp. That's what
 * happens while a boss isn't mirrored yet (see CoopEnemyMirror.cpp, like during its intro), mirrored bosses are
 * defeated for everyone when the authority's is. Barinade's health is how far her fight still has to go.
 *
 * Each client remembers the destroyed actors of the rooms it has loaded, so that actors spawned later (like grass in a
 * grass circle you walk up to) stay destroyed, and so it can tell a teammate entering the room what is already gone.
 * Like in vanilla, everything comes back once the room is reloaded with nobody else in it.
 */

uint8_t ObjTsubo_RandomizerHoldsItem(ObjTsubo* potActor, PlayState* play);
uint8_t ObjKibako_RandomizerHoldsItem(ObjKibako* smallCrateActor, PlayState* play);
uint8_t ObjKibako2_RandomizerHoldsItem(ObjKibako2* crateActor, PlayState* play);
uint8_t EnKusa_RandomizerHoldsItem(EnKusa* grassActor, PlayState* play);
uint8_t Rock_RandomizerHoldsItem(CheckIdentity rockIdentity, PlayState* play, bool isBoulder);

struct CoopDropId {
    uint64_t id;
};
static ObjectExtension::Register<CoopDropId> CoopDropIdRegister;

// Enemies spawned in the same spot by another one, identified by their slot in it (see CoopActorIdentity)
struct CoopChildIdentity {
    CoopActorIdentity identity;
};
static ObjectExtension::Register<CoopChildIdentity> CoopChildIdentityRegister;

// Where an enemy was spawned. Some move their home position during init (a Skullwalltula snaps to its wall), so it's
// recorded before, to identify the enemy the same way before and after its init.
struct CoopSpawnPos {
    Vec3f pos;
};
static ObjectExtension::Register<CoopSpawnPos> CoopSpawnPosRegister;

void SetCoopChildSlot(Actor* child, Actor* parent, s16 slot) {
    if (child == nullptr) {
        return;
    }

    CoopActorIdentity identity = GetCoopActorIdentity(parent);
    identity.actorId = child->id;
    identity.childIndex = slot;
    SetCoopChildIdentity(child, identity);
}

void SetCoopChildIdentity(Actor* child, const CoopActorIdentity& identity) {
    ObjectExtension::GetInstance().Set<CoopChildIdentity>(child, CoopChildIdentity{ identity });
}

struct CoopEnemyHealth {
    s32 lastHealth;
};
static ObjectExtension::Register<CoopEnemyHealth> CoopEnemyHealthRegister;

static bool IsCoopEnemyType(Actor* actor) {
    // Gold Skulltulas are left alone, their tokens are synced as flags. Skullwalltulas are the same actor, and only
    // become enemies in their init.
    if (actor->id == ACTOR_EN_SW) {
        return (actor->params & 0xE000) == 0;
    }

    return actor->category == ACTORCAT_ENEMY;
}

// What some enemies leave behind when they die, as the same actor but no longer an enemy: a Deku Baba cut off at the
// stem leaves a Deku Stick to pick up, a Poe a soul to catch in a bottle
static bool IsCoopEnemyRemains(Actor* actor) {
    return (actor->id == ACTOR_EN_DEKUBABA || actor->id == ACTOR_EN_POH) && actor->category == ACTORCAT_MISC &&
           (GetActorListIndex(actor) >= 0 || ObjectExtension::GetInstance().Has<CoopChildIdentity>(actor));
}

// Only enemies placed in the room (or spawned in a known slot of one) are synced. Enemies spawned during play, like
// Keese from a spawner, can't be matched up between players.
bool IsCoopEnemy(Actor* actor) {
    return IsCoopEnemyType(actor) &&
           (GetActorListIndex(actor) >= 0 || ObjectExtension::GetInstance().Has<CoopChildIdentity>(actor));
}

// Bosses and their parts are often spawned by the fight itself, but there's only one of each in a boss room, so all
// of them can be matched up
bool IsCoopBoss(Actor* actor) {
    // Phantom Ganon's horse is a background actor
    return actor->category == ACTORCAT_BOSS || actor->id == ACTOR_EN_FHG;
}

// Most enemies and bosses keep their health in colChkInfo, a few in a field of their own
static s16* GetCustomHealth(Actor* actor) {
    switch (actor->id) {
        case ACTOR_BOSS_DODONGO:
            return &((BossDodongo*)actor)->health;
        default:
            return nullptr;
    }
}

// Barinade's body has no health, the fight goes by how many of her parts are gone (and hits on her body). Its health is
// how much is left of that.
#define BOSSVA_PHASE_DEATH 18

s32 GetCoopHealth(Actor* actor) {
    if (actor->id == ACTOR_BOSS_VA && actor->params == BOSSVA_BODY) {
        return BOSSVA_PHASE_DEATH - *gBossVaFightPhase;
    }
    s16* customHealth = GetCustomHealth(actor);
    return customHealth != nullptr ? *customHealth : actor->colChkInfo.health;
}

void SetCoopHealth(Actor* actor, s32 health) {
    if (actor->id == ACTOR_BOSS_VA && actor->params == BOSSVA_BODY) {
        *gBossVaFightPhase = BOSSVA_PHASE_DEATH - CLAMP(health, 0, BOSSVA_PHASE_DEATH);
        return;
    }
    s16* customHealth = GetCustomHealth(actor);
    if (customHealth != nullptr) {
        *customHealth = health;
    } else {
        actor->colChkInfo.health = health;
    }
}

static bool IsCoopDestructible(Actor* actor) {
    switch (actor->id) {
        case ACTOR_EN_KUSA:
        case ACTOR_EN_ISHI:
        case ACTOR_OBJ_KIBAKO:
        case ACTOR_OBJ_KIBAKO2:
        case ACTOR_OBJ_TSUBO:
            return true;
        default:
            return false;
    }
}

// Type 1 grass regrows on its own after a few seconds, so it never stays destroyed
static bool StaysDestroyed(Actor* actor) {
    return actor->id != ACTOR_EN_KUSA || (actor->params & 3) != ENKUSA_TYPE_1;
}

CoopActorIdentity GetCoopActorIdentity(Actor* actor) {
    const CoopChildIdentity* child = ObjectExtension::GetInstance().Get<CoopChildIdentity>(actor);
    if (child != nullptr) {
        return child->identity;
    }

    // Barinade spawns her parts where she is. There's one of each.
    if (actor->id == ACTOR_BOSS_VA) {
        return { actor->id, 0, 0, 0, actor->params };
    }

    // Twinrova spawns the sisters where she is, which depends on whether her intro played. There's one of each.
    if (actor->id == ACTOR_BOSS_TW && actor->params < 0x64) {
        return { actor->id, 0, 0, 0, actor->params };
    }

    // Morpha's tentacles are spawned where the core is, which depends on whether its intro played. The first is spawned
    // as the core's child, the second isn't.
    if (actor->id == ACTOR_BOSS_MO && actor->params == BOSSMO_TENTACLE) {
        return { actor->id, 0, 0, 0, actor->params, (s16)(actor->parent != nullptr ? 1 : 2) };
    }

    Vec3f pos = actor->home.pos;
    const CoopSpawnPos* spawnPos = ObjectExtension::GetInstance().Get<CoopSpawnPos>(actor);
    if (spawnPos != nullptr) {
        pos = spawnPos->pos;
    }
    GetCoopSpawnPos(actor, &pos);
    return { actor->id, (s16)pos.x, (s16)pos.y, (s16)pos.z, IsCoopBoss(actor) ? actor->params : (s16)0 };
}

bool IsRoomLoaded(s8 roomNum) {
    return roomNum == gPlayState->roomCtx.curRoom.num || roomNum == gPlayState->roomCtx.prevRoom.num;
}

Actor* FindCoopActor(const CoopActorIdentity& identity) {
    for (s32 i = 0; i < ACTORCAT_MAX; i++) {
        for (Actor* actor = gPlayState->actorCtx.actorLists[i].head; actor != NULL; actor = actor->next) {
            if (actor->update != NULL && actor->id == identity.actorId && GetCoopActorIdentity(actor) == identity &&
                ((actor->category != ACTORCAT_ENEMY && actor->category != ACTORCAT_BOSS) || IsCoopEnemy(actor) ||
                 IsCoopBoss(actor))) {
                return actor;
            }
        }
    }

    return nullptr;
}

static bool IsLiveActor(Actor* target) {
    for (s32 i = 0; i < ACTORCAT_MAX; i++) {
        for (Actor* actor = gPlayState->actorCtx.actorLists[i].head; actor != NULL; actor = actor->next) {
            if (actor == target) {
                return actor->update != NULL;
            }
        }
    }

    return false;
}

static bool IsLiveItem(Actor* item) {
    for (Actor* actor = gPlayState->actorCtx.actorLists[ACTORCAT_MISC].head; actor != NULL; actor = actor->next) {
        if (actor == item) {
            return actor->update != NULL;
        }
    }

    return false;
}

static void KillCoopEnemy(Actor* actor) {
    Vec3f zeroVec = { 0.0f, 0.0f, 0.0f };
    EffectSsDeadDb_Spawn(gPlayState, &actor->world.pos, &zeroVec, &zeroVec, 100, 0, 255, 255, 255, 255, 0, 0, 255, 1, 9,
                         true);
    Actor_Kill(actor);
}

// Whether the actor still holds a randomizer check in our seed (a shuffled pot, crate, grass or rock). A teammate on
// another seed breaking theirs must not take it away from us.
static bool HoldsCheckForUs(Actor* actor) {
    if (!IS_RANDO) {
        return false;
    }

    switch (actor->id) {
        case ACTOR_OBJ_TSUBO:
            return ObjTsubo_RandomizerHoldsItem((ObjTsubo*)actor, gPlayState);
        case ACTOR_OBJ_KIBAKO:
            return ObjKibako_RandomizerHoldsItem((ObjKibako*)actor, gPlayState);
        case ACTOR_OBJ_KIBAKO2:
            return ObjKibako2_RandomizerHoldsItem((ObjKibako2*)actor, gPlayState);
        case ACTOR_EN_KUSA:
            return EnKusa_RandomizerHoldsItem((EnKusa*)actor, gPlayState);
        case ACTOR_EN_ISHI: {
            const CheckIdentity* identity = ObjectExtension::GetInstance().Get<CheckIdentity>(actor);
            return identity != nullptr && Rock_RandomizerHoldsItem(*identity, gPlayState, actor->params & 1);
        }
        default:
            return false;
    }
}

// Plays the actor's own break effects and removes it, without rolling a drop (the teammate who broke it shares theirs)
static void BreakCoopActor(Actor* actor) {
    if (actor->category == ACTORCAT_ENEMY) {
        KillCoopEnemy(actor);
        return;
    }

    // Leave it alone if we are carrying it ourselves
    if (Actor_HasParent(actor, gPlayState)) {
        return;
    }

    switch (actor->id) {
        case ACTOR_EN_DEKUBABA:
        case ACTOR_EN_POH:
            if (IsCoopEnemyRemains(actor)) {
                Actor_Kill(actor);
            }
            return;
        case ACTOR_EN_KUSA: {
            EnKusa* grass = (EnKusa*)actor;
            if (grass->actionFunc != EnKusa_Main) {
                // Not standing: already cut (keep the stump or let it regrow), or still loading its object
                if (StaysDestroyed(actor) && !(actor->flags & ACTOR_FLAG_GRASS_DESTROYED)) {
                    Actor_Kill(actor);
                }
                return;
            }
            EnKusa_SpawnFragments(grass, gPlayState);
            SoundSource_PlaySfxAtFixedWorldPos(gPlayState, &actor->world.pos, 20, NA_SE_EV_PLANT_BROKEN);
            if ((actor->params >> 4) & 1) {
                EnKusa_SpawnBugs(grass, gPlayState);
            }
            if ((actor->params & 3) == ENKUSA_TYPE_0) {
                Actor_Kill(actor);
            } else {
                EnKusa_SetupCut(grass);
                actor->flags |= ACTOR_FLAG_GRASS_DESTROYED;
            }
            return;
        }
        case ACTOR_EN_ISHI:
            if (((EnIshi*)actor)->actionFunc == EnIshi_Wait) {
                if ((actor->params & 1) == ROCK_SMALL) {
                    EnIshi_SpawnFragmentsSmall((EnIshi*)actor, gPlayState);
                    EnIshi_SpawnDustSmall((EnIshi*)actor, gPlayState);
                    SoundSource_PlaySfxAtFixedWorldPos(gPlayState, &actor->world.pos, 20, NA_SE_EV_ROCK_BROKEN);
                } else {
                    EnIshi_SpawnFragmentsLarge((EnIshi*)actor, gPlayState);
                    EnIshi_SpawnDustLarge((EnIshi*)actor, gPlayState);
                    SoundSource_PlaySfxAtFixedWorldPos(gPlayState, &actor->world.pos, 40, NA_SE_EV_WALL_BROKEN);
                }
            }
            Actor_Kill(actor);
            return;
        case ACTOR_OBJ_TSUBO:
            if (((ObjTsubo*)actor)->actionFunc == ObjTsubo_Idle) {
                ObjTsubo_AirBreak((ObjTsubo*)actor, gPlayState);
                SoundSource_PlaySfxAtFixedWorldPos(gPlayState, &actor->world.pos, 20, NA_SE_EV_POT_BROKEN);
            }
            Actor_Kill(actor);
            return;
        case ACTOR_OBJ_KIBAKO:
            if (((ObjKibako*)actor)->actionFunc == ObjKibako_Idle) {
                ObjKibako_AirBreak((ObjKibako*)actor, gPlayState);
                SoundSource_PlaySfxAtFixedWorldPos(gPlayState, &actor->world.pos, 20, NA_SE_EV_WOODBOX_BREAK);
            }
            Actor_Kill(actor);
            return;
        case ACTOR_OBJ_KIBAKO2:
            if (((ObjKibako2*)actor)->actionFunc == ObjKibako2_Idle) {
                ObjKibako2_Break((ObjKibako2*)actor, gPlayState);
                SoundSource_PlaySfxAtFixedWorldPos(gPlayState, &actor->world.pos, 20, NA_SE_EV_WOODBOX_BREAK);
                // Some large crates hide a Gold Skulltula, which every player gets their own copy of
                if ((actor->params & 0x8000) == 0) {
                    Actor_Spawn(&gPlayState->actorCtx, gPlayState, ACTOR_EN_SW, actor->world.pos.x, actor->world.pos.y,
                                actor->world.pos.z, 0, actor->shape.rot.y, 0, actor->params | 0x8000);
                }
            }
            Actor_Kill(actor);
            return;
    }
}

// MARK: - Hook handlers

void Anchor::OnCoopRoomChanged() {
    // Whoever is in the new room already keeps running it, we only take over if nobody does
    coopClaimsAuthority = false;
    coopRoomFrames = 0;

    // A boss we come back to starts over too (like it does after a reset), and so does who's most dangerous to it
    ResetCoopBossThreat();

    // Forget destroyed actors of rooms we unloaded, a teammate still there tells us again if we come back
    std::erase_if(coopDestroyedActors, [](const auto& entry) { return !IsRoomLoaded(entry.second); });

    SendPacket_CoopSceneSyncRequest();
}

bool Anchor::ShouldCoopActorInit(Actor* actor) {
    // The actor list index isn't known yet here, so this can't tell placed enemies apart from spawned ones. A spawned
    // enemy would need the same type and spawn position as a killed placed one to be affected.
    if (!isConnected || !roomState.coopSync) {
        return true;
    }

    // What a boss puppet spawns by running its own update (Gohma's eggs, Volvagia's rocks) comes from the authority
    // instead
    if (coopUpdatingActor != nullptr && IsCoopDrivenPuppet(coopUpdatingActor) && !IsCoopBossPart(actor)) {
        return false;
    }

    if (!(IsCoopDestructible(actor) || IsCoopEnemyType(actor))) {
        return true;
    }

    if (coopDestroyedActors.contains(GetCoopActorIdentity(actor))) {
        return false;
    }

    if (IsCoopEnemyType(actor)) {
        ObjectExtension::GetInstance().Set<CoopSpawnPos>(actor, CoopSpawnPos{ actor->home.pos });
    }
    return true;
}

// Called when a destructible actor breaks (from its drop hook) or a synced enemy dies
void Anchor::OnCoopActorBroken(Actor* actor) {
    if (!IsCoopSyncActive() || isProcessingIncomingPacket) {
        return;
    }

    if (StaysDestroyed(actor)) {
        coopDestroyedActors[GetCoopActorIdentity(actor)] = gPlayState->roomCtx.curRoom.num;
    }

    // Items spawned until the actor finishes its update are its drops
    coopDropSource = actor;
    SendPacket_CoopActorDestroyed(actor);
}

void Anchor::OnCoopItemSpawned(Actor* actor) {
    // Drops of an enemy are spawned around its death, from its update, once its health is down to 0
    bool isEnemyDrop =
        coopUpdatingActor != nullptr && IsCoopEnemy(coopUpdatingActor) && coopUpdatingActor->colChkInfo.health == 0;
    if (coopDropSource == nullptr && !isEnemyDrop) {
        return;
    }

    // Randomizer items have their own sync through the check status
    if (actor->params >= ITEM00_SOH_DUMMY) {
        return;
    }

    coopPendingDrops.push_back(actor);
}

void Anchor::OnCoopActorUpdating(Actor* actor) {
    coopUpdatingActor = actor;
}

void Anchor::OnCoopActorUpdated(Actor* actor) {
    if (actor == coopDropSource) {
        coopDropSource = nullptr;
    }
    if (actor == coopUpdatingActor) {
        coopUpdatingActor = nullptr;
    }

    if (!IsCoopSyncActive() || !(IsCoopEnemy(actor) || IsCoopBoss(actor))) {
        return;
    }

    // Share health lost to our hits. Health set from a packet updates lastHealth too, so it isn't sent back.
    CoopEnemyHealth* health = ObjectExtension::GetInstance().Get<CoopEnemyHealth>(actor);
    if (health == nullptr) {
        ObjectExtension::GetInstance().Set<CoopEnemyHealth>(actor, CoopEnemyHealth{ GetCoopHealth(actor) });
        return;
    }

    // A boss puppet's health is the authority's already
    if (GetCoopHealth(actor) < health->lastHealth && !IsCoopDrivenPuppet(actor)) {
        SendPacket_CoopEnemyHealth(actor);
    }
    if (GetCoopHealth(actor) != health->lastHealth && !IsCoopDrivenPuppet(actor)) {
        RecordCoopDamage(actor, health->lastHealth, GetCoopHealth(actor));
    }
    health->lastHealth = GetCoopHealth(actor);
}

void Anchor::OnCoopActorKilled(Actor* actor) {
    // Enemies are also killed when their room unloads, but then they still have health left. Their remains are killed
    // when they're picked up (or when they vanish after a while, then they're gone for everyone anyway).
    if ((IsCoopEnemy(actor) || IsCoopEnemyRemains(actor)) && actor->colChkInfo.health == 0) {
        OnCoopActorBroken(actor);
    }
}

// Drops are shared at the end of the frame, once the item has its starting velocity
void Anchor::FlushCoopPendingDrops() {
    if (coopPendingDrops.empty()) {
        return;
    }

    for (Actor* item : coopPendingDrops) {
        if (IsCoopSyncActive() && IsLiveItem(item)) {
            uint64_t dropId = ((uint64_t)ownClientId << 32) | coopNextDropId++;
            ObjectExtension::GetInstance().Set<CoopDropId>(item, CoopDropId{ dropId });
            SendPacket_CoopItemDrop((EnItem00*)item, dropId);
        }
    }

    coopPendingDrops.clear();
    coopDropSource = nullptr;
}

void Anchor::OnCoopItemCollected(Actor* actor) {
    if (!IsCoopSyncActive() || isProcessingIncomingPacket) {
        return;
    }

    const CoopDropId* dropId = ObjectExtension::GetInstance().Get<CoopDropId>(actor);
    if (dropId != nullptr) {
        SendPacket_CoopItemCollected(dropId->id);
    }
}

// MARK: - Room catch up (sent in COOP_SCENE_SYNC)

nlohmann::json Anchor::PrepCoopDestroyedActors(s8 roomNum) {
    nlohmann::json destroyed = nlohmann::json::array();

    for (auto& [identity, actorRoomNum] : coopDestroyedActors) {
        if (actorRoomNum == roomNum) {
            destroyed.push_back(identity);
        }
    }

    return destroyed;
}

void Anchor::ApplyCoopDestroyedActors(nlohmann::json& destroyed, bool fromSameSeed) {
    for (auto& entry : destroyed) {
        CoopActorIdentity identity = entry.get<CoopActorIdentity>();
        Actor* actor = FindCoopActor(identity);

        // From another seed, only what we can tell doesn't hold a check of ours (it may not have spawned yet)
        if (!fromSameSeed && (actor == nullptr || HoldsCheckForUs(actor))) {
            continue;
        }
        coopDestroyedActors[identity] = gPlayState->roomCtx.curRoom.num;

        // Already gone before we got here, so remove it without any effects
        if (actor != nullptr && (actor->category == ACTORCAT_ENEMY || !Actor_HasParent(actor, gPlayState))) {
            Actor_Kill(actor);
        }
    }
}

// The health of the enemies (and bosses) still alive in a room, for a teammate entering it. Without this, enemies that
// aren't mirrored would be back to full health for them.
nlohmann::json Anchor::PrepCoopEnemyHealth(s8 roomNum) {
    nlohmann::json enemies = nlohmann::json::array();

    for (s32 category : { ACTORCAT_ENEMY, ACTORCAT_BOSS }) {
        for (Actor* actor = gPlayState->actorCtx.actorLists[category].head; actor != NULL; actor = actor->next) {
            if (actor->update == NULL || (actor->room != roomNum && actor->room != -1) ||
                !(IsCoopEnemy(actor) || IsCoopBoss(actor))) {
                continue;
            }

            enemies.push_back({ { "id", GetCoopActorIdentity(actor) }, { "health", GetCoopHealth(actor) } });
        }
    }

    return enemies;
}

void Anchor::ApplyCoopEnemyHealth(nlohmann::json& enemies) {
    for (auto& enemy : enemies) {
        SetCoopEnemyHealth(enemy.at("id").get<CoopActorIdentity>(), enemy.at("health").get<s32>());
    }
}

// MARK: - Packets

void Anchor::SendPacket_CoopActorDestroyed(Actor* actor) {
    std::vector<uint32_t> peers = GetCoopPeers(false);
    if (peers.empty()) {
        return;
    }

    CoopActorIdentity identity = GetCoopActorIdentity(actor);
    nlohmann::json payload = PrepCoopPayload(COOP_ACTOR_DESTROYED);
    payload["actor"] = identity;

    for (uint32_t clientId : peers) {
        payload["targetClientId"] = clientId;
        SendJsonToRemote(payload);
    }
}

void Anchor::HandlePacket_CoopActorDestroyed(nlohmann::json payload) {
    if (!IsCoopPayloadForThisScene(payload)) {
        return;
    }

    s8 roomNum = payload.at("roomNum").get<s8>();
    if (!IsRoomLoaded(roomNum)) {
        return;
    }

    CoopActorIdentity identity = payload.at("actor").get<CoopActorIdentity>();
    bool fromSameSeed = IsFromSameSeedPeer(payload);

    Actor* actor = FindCoopActor(identity);
    // A teammate on another seed breaks only what doesn't hold a check of ours, and only what we can see is there
    if (!fromSameSeed && (actor == nullptr || HoldsCheckForUs(actor))) {
        return;
    }
    if (actor != nullptr) {
        if (StaysDestroyed(actor)) {
            coopDestroyedActors[identity] = roomNum;
        }
        BreakCoopActor(actor);
    } else if (identity.actorId != ACTOR_EN_KUSA) {
        // Not spawned for us yet, make sure it doesn't. Grass is skipped since we can't tell if it regrows.
        coopDestroyedActors[identity] = roomNum;
    }
}

void Anchor::SendPacket_CoopEnemyHealth(Actor* actor) {
    std::vector<uint32_t> peers = GetCoopPeers(true);
    if (peers.empty()) {
        return;
    }

    CoopActorIdentity identity = GetCoopActorIdentity(actor);
    nlohmann::json payload = PrepCoopPayload(COOP_ENEMY_HEALTH);
    payload["actor"] = identity;
    payload["health"] = GetCoopHealth(actor);

    for (uint32_t clientId : peers) {
        payload["targetClientId"] = clientId;
        SendJsonToRemote(payload);
    }
}

void Anchor::HandlePacket_CoopEnemyHealth(nlohmann::json payload) {
    if (!IsCoopPayloadForThisScene(payload) || !IsRoomLoaded(payload.at("roomNum").get<s8>())) {
        return;
    }

    SetCoopEnemyHealth(payload.at("actor").get<CoopActorIdentity>(), payload.at("health").get<s32>());
}

void Anchor::SetCoopEnemyHealth(const CoopActorIdentity& identity, s32 health) {
    Actor* actor = FindCoopActor(identity);
    if (actor == nullptr || (actor->category != ACTORCAT_ENEMY && actor->category != ACTORCAT_BOSS)) {
        return;
    }

    // An enemy at 0 stays alive until its death is shared (COOP_ACTOR_DESTROYED), or until it's hit again here. Bosses
    // stop at 1, so every player lands the final blow and sees their own boss defeat.
    if (actor->category == ACTORCAT_BOSS) {
        health = std::max(health, 1);
    }
    if (health >= GetCoopHealth(actor)) {
        return;
    }

    SetCoopHealth(actor, health);
    ObjectExtension::GetInstance().Set<CoopEnemyHealth>(actor, CoopEnemyHealth{ health });
}

void Anchor::SendPacket_CoopItemDrop(EnItem00* item, uint64_t dropId) {
    std::vector<uint32_t> peers = GetCoopPeers(false, true);
    if (peers.empty()) {
        return;
    }

    nlohmann::json payload = PrepCoopPayload(COOP_ITEM_DROP);
    payload["dropId"] = dropId;
    payload["itemType"] = item->actor.params;
    payload["collectibleFlag"] = item->collectibleFlag;
    payload["pos"] = item->actor.world.pos;
    payload["velocityY"] = item->actor.velocity.y;
    payload["speedXZ"] = item->actor.speedXZ;
    payload["gravity"] = item->actor.gravity;
    payload["rotY"] = item->actor.world.rot.y;

    for (uint32_t clientId : peers) {
        payload["targetClientId"] = clientId;
        SendJsonToRemote(payload);
    }
}

void Anchor::HandlePacket_CoopItemDrop(nlohmann::json payload) {
    // Drops are loot, which stays within a seed
    if (!IsCoopPayloadForThisScene(payload) || !IsFromSameSeedPeer(payload) ||
        !IsRoomLoaded(payload.at("roomNum").get<s8>())) {
        return;
    }

    s16 itemType = payload.at("itemType").get<s16>();
    s16 collectibleFlag = payload.at("collectibleFlag").get<s16>();
    if (itemType < 0 || itemType >= ITEM00_SOH_DUMMY || collectibleFlag < 0 || collectibleFlag >= 0x40) {
        return;
    }

    // Dropping it like the game does adapts it to us, e.g. arrows become rupees if we don't have a bow yet
    Vec3f pos = payload.at("pos").get<Vec3f>();
    EnItem00* item = Item_DropCollectible(gPlayState, &pos, itemType | (collectibleFlag << 8));
    if (item == nullptr || item->actor.id != ACTOR_EN_ITEM00) {
        return;
    }

    item->actor.velocity.y = payload.at("velocityY").get<f32>();
    item->actor.speedXZ = payload.at("speedXZ").get<f32>();
    item->actor.gravity = payload.at("gravity").get<f32>();
    item->actor.world.rot.y = payload.at("rotY").get<s16>();
    ObjectExtension::GetInstance().Set<CoopDropId>(&item->actor, CoopDropId{ payload.at("dropId").get<uint64_t>() });
}

void Anchor::SendPacket_CoopItemCollected(uint64_t dropId) {
    std::vector<uint32_t> peers = GetCoopPeers(false, true);
    if (peers.empty()) {
        return;
    }

    nlohmann::json payload = PrepCoopPayload(COOP_ITEM_COLLECTED);
    payload["dropId"] = dropId;

    for (uint32_t clientId : peers) {
        payload["targetClientId"] = clientId;
        SendJsonToRemote(payload);
    }
}

void Anchor::HandlePacket_CoopItemCollected(nlohmann::json payload) {
    if (!IsCoopPayloadForThisScene(payload) || !IsFromSameSeedPeer(payload)) {
        return;
    }

    uint64_t dropId = payload.at("dropId").get<uint64_t>();

    for (Actor* actor = gPlayState->actorCtx.actorLists[ACTORCAT_MISC].head; actor != NULL; actor = actor->next) {
        const CoopDropId* actorDropId = ObjectExtension::GetInstance().Get<CoopDropId>(actor);
        if (actor->id == ACTOR_EN_ITEM00 && actorDropId != nullptr && actorDropId->id == dropId) {
            Actor_Kill(actor);
            return;
        }
    }
}

// MARK: - Carrying

#define CARRIED_FOLLOW_FRAMES 60     // How long a thrown object is followed at most, until it lands or breaks
#define CARRIED_EXPLOSIVE_FRAMES 400 // Bombs are followed until they blow up
#define CARRIED_MIN_FLIGHT_FRAMES 3
#define SHOT_FOLLOW_FRAMES 300 // Shots are followed until they're gone, this is just in case
#define SHOT_FLASH_DISTANCE 600.0f

struct CoopCarriedCopy {};
static ObjectExtension::Register<CoopCarriedCopy> CoopCarriedCopyRegister;

static bool IsCoopExplosive(s16 actorId) {
    return actorId == ACTOR_EN_BOM || actorId == ACTOR_EN_BOMBF;
}

static bool IsCoopExploding(s16 actorId, s16 params) {
    return (actorId == ACTOR_EN_BOM && params == BOMB_EXPLOSION) ||
           (actorId == ACTOR_EN_BOMBF && params == BOMBFLOWER_EXPLOSION);
}

// What our player shot or threw that's in flight: arrows, slingshot seeds, Deku Nuts and the boomerang. Not the ones
// still in their hand, nor enemies' arrows or our copies of teammates' shots.
static bool IsOwnCoopShot(Actor* actor) {
    if (ObjectExtension::GetInstance().Has<CoopCarriedCopy>(actor)) {
        return false;
    }
    if (actor->id == ACTOR_EN_ARROW) {
        return actor->params >= ARROW_NORMAL_LIT && actor->params <= ARROW_NUT && actor->parent == NULL;
    }
    return actor->id == ACTOR_EN_BOOM;
}

// The look and sound of a seed or Deku Nut hitting something, where a teammate's copy was last. Only the look: the
// Deku Nut's stun happens in their game.
static void PlayCoopShotImpact(Actor* copy) {
    if (copy->id != ACTOR_EN_ARROW || copy->params < ARROW_SEED) {
        return;
    }

    EffectSsStone1_Spawn(gPlayState, &copy->world.pos, 0);
    if (copy->params == ARROW_NUT) {
        SoundSource_PlaySfxAtFixedWorldPos(gPlayState, &copy->world.pos, 20, NA_SE_IT_DEKU);
        // The white flash, if it went off close enough to blind us
        if (Actor_WorldDistXYZToPoint(&GET_PLAYER(gPlayState)->actor, &copy->world.pos) < SHOT_FLASH_DISTANCE) {
            iREG(50) = -1;
        }
    } else {
        SoundSource_PlaySfxAtFixedWorldPos(gPlayState, &copy->world.pos, 20, NA_SE_IT_SLING_REFLECT);
    }
}

// Copies of shots are drawn as if flying, with their own update never running
static void SetupCoopShotCopy(Actor* copy) {
    if (copy->id == ACTOR_EN_ARROW) {
        // Its trail is drawn while flying, and seeds and nuts only while moving
        ((EnArrow*)copy)->actionFunc = EnArrow_Fly;
        copy->speedXZ = 1.0f;
    }
    // Not a projectile in our game, so enemies that dodge or react to them leave it alone
    Actor_ChangeCategory(gPlayState, &gPlayState->actorCtx, copy, ACTORCAT_PROP);
}

// The look and sound of a bomb going off, without any of its effects (those happen in the teammate's game)
static void PlayCoopExplosion(Vec3f pos) {
    Vec3f zeroVec = { 0.0f, 0.0f, 0.0f };
    Vec3f bomb2Accel = { 0.0f, 0.1f, 0.0f };
    Vec3f effectPos = pos;

    effectPos.y += 10.0f;
    EffectSsBomb2_SpawnLayered(gPlayState, &effectPos, &zeroVec, &bomb2Accel, 100, 19);

    CollisionPoly* floorPoly;
    effectPos.y = BgCheck_EntityRaycastFloor1(&gPlayState->colCtx, &floorPoly, &pos);
    if (effectPos.y > BGCHECK_Y_MIN) {
        EffectSsBlast_SpawnWhiteShockwave(gPlayState, &effectPos, &zeroVec, &zeroVec);
    }

    SoundSource_PlaySfxAtFixedWorldPos(gPlayState, &pos, 30, NA_SE_IT_BOMB_EXPLOSION);
}

// Adds what our player carries to PLAYER_UPDATE, so teammates see it in our hands, flying when we throw it and landing
// where we put it down. Things placed in the room are their own copy of it, breaking it is shared like any other
// break. Anything else, like a bomb or a Cucco, gets a copy spawned for them, a bomb until it blows up. What we shoot
// is added too, until it's gone.
void Anchor::AddCoopCarriedObject(nlohmann::json& payload) {
    Player* player = GET_PLAYER(gPlayState);
    if (!IsCoopSyncActive()) {
        coopCarriedActors.clear();
        return;
    }

    Actor* heldActor = (player->stateFlags1 & PLAYER_STATE1_CARRYING_ACTOR) ? player->heldActor : nullptr;
    if (heldActor != nullptr && std::none_of(coopCarriedActors.begin(), coopCarriedActors.end(),
                                             [&](const CoopTrackedCarry& carry) { return carry.actor == heldActor; })) {
        coopCarriedActors.push_back({ heldActor, coopNextCarryKey++, 0 });
    }
    for (s32 category : { ACTORCAT_ITEMACTION, ACTORCAT_MISC }) {
        for (Actor* actor = gPlayState->actorCtx.actorLists[category].head; actor != NULL; actor = actor->next) {
            if (actor->update != NULL && IsOwnCoopShot(actor) &&
                std::none_of(coopCarriedActors.begin(), coopCarriedActors.end(),
                             [&](const CoopTrackedCarry& carry) { return carry.actor == actor; })) {
                coopCarriedActors.push_back({ actor, coopNextCarryKey++, 0, true });
            }
        }
    }

    nlohmann::json carried = nlohmann::json::array();
    std::erase_if(coopCarriedActors, [&](CoopTrackedCarry& carry) {
        // Actors of an unloaded room are freed without being killed first, so make sure it's still there
        if (!IsLiveActor(carry.actor)) {
            return true;
        }

        Actor* actor = carry.actor;
        bool isPlaced = GetActorListIndex(actor) >= 0;
        bool isDone = false;
        if (carry.isShot) {
            isDone = ++carry.framesSinceRelease > SHOT_FOLLOW_FRAMES;
        } else if (actor == heldActor) {
            carry.framesSinceRelease = 0;
        } else if (IsCoopExplosive(actor->id) && !isPlaced) {
            isDone = ++carry.framesSinceRelease > CARRIED_EXPLOSIVE_FRAMES;
        } else {
            carry.framesSinceRelease++;
            isDone =
                carry.framesSinceRelease > CARRIED_FOLLOW_FRAMES ||
                (carry.framesSinceRelease > CARRIED_MIN_FLIGHT_FRAMES && (actor->bgCheckFlags & BGCHECKFLAG_GROUND));
        }

        nlohmann::json entry = { { "key", carry.key }, { "pos", actor->world.pos }, { "rot", actor->shape.rot } };
        if (isPlaced) {
            entry["id"] = GetCoopActorIdentity(actor);
        } else {
            entry["actorId"] = actor->id;
            entry["params"] = actor->params;
            entry["scale"] = actor->scale.x;
        }
        if (carry.isShot) {
            entry["shot"] = true;
            entry["worldRot"] = actor->world.rot;
        }
        carried.push_back(entry);

        // Sent where it came to rest one last time, from there on it's back to normal
        return isDone;
    });

    if (!carried.empty()) {
        payload["carrying"] = carried;
    }
}

void Anchor::ReadCoopCarriedObjects(AnchorClient& client, nlohmann::json& payload) {
    client.carriedCoopObjects.clear();
    if (!payload.contains("carrying")) {
        return;
    }

    for (auto& entry : payload["carrying"]) {
        CoopCarriedObject object = {};
        object.key = entry.at("key").get<uint32_t>();
        object.isPlaced = entry.contains("id");
        if (object.isPlaced) {
            object.identity = entry["id"].get<CoopActorIdentity>();
            object.actorId = object.identity.actorId;
        } else {
            object.actorId = entry.at("actorId").get<s16>();
            object.params = entry.at("params").get<s16>();
            object.scale = entry.value("scale", -1.0f);
        }
        object.pos = entry.at("pos").get<Vec3f>();
        object.rot = entry.at("rot").get<Vec3s>();
        object.isShot = entry.value("shot", false);
        if (object.isShot) {
            object.worldRot = entry.at("worldRot").get<Vec3s>();
        }
        client.carriedCoopObjects.push_back(object);
    }
}

// Called before an actor updates. Our copy of something a teammate carries follows theirs and doesn't update by itself,
// so we can't pick it up at the same time, and a copied bomb doesn't go off by itself.
bool Anchor::UpdateCoopCarriedByTeammate(Actor* actor) {
    if (ObjectExtension::GetInstance().Has<CoopCarriedCopy>(actor)) {
        return true;
    }

    if (GetActorListIndex(actor) < 0 || Actor_HasParent(actor, gPlayState) || !IsCoopSyncActive()) {
        return false;
    }

    bool hasIdentity = false;
    CoopActorIdentity identity;
    for (auto& [clientId, client] : clients) {
        for (const CoopCarriedObject& object : client.carriedCoopObjects) {
            if (!object.isPlaced || object.actorId != actor->id || !IsCoopPeer(client)) {
                continue;
            }
            // Ours stays put if it holds a check of ours, since breaking theirs won't break it
            if (!IsSameSeedPeer(client) && HoldsCheckForUs(actor)) {
                continue;
            }

            if (!hasIdentity) {
                identity = GetCoopActorIdentity(actor);
                hasIdentity = true;
            }
            if (object.identity == identity) {
                actor->world.pos = object.pos;
                actor->shape.rot = object.rot;
                return true;
            }
        }
    }

    return false;
}

// Spawns, moves and removes our copies of what teammates carry that isn't placed in the room. Called at the end of the
// frame, after their PLAYER_UPDATE came in.
void Anchor::UpdateCoopCarriedCopies() {
    std::set<std::pair<uint32_t, uint32_t>> current;

    if (IsCoopSyncActive()) {
        for (auto& [clientId, client] : clients) {
            if (!IsCoopPeer(client)) {
                continue;
            }

            for (const CoopCarriedObject& object : client.carriedCoopObjects) {
                if (object.isPlaced) {
                    continue;
                }

                auto key = std::make_pair(clientId, object.key);
                current.insert(key);

                // A copy that's already done (blown up, or couldn't be spawned) stays done
                auto it = coopCarriedCopies.find(key);
                if (it != coopCarriedCopies.end() && (it->second == nullptr || !IsLiveActor(it->second))) {
                    it->second = nullptr;
                    continue;
                }

                bool isExploding = IsCoopExploding(object.actorId, object.params);
                Actor* copy = it != coopCarriedCopies.end() ? it->second : nullptr;
                if (copy == nullptr && !isExploding) {
                    copy = Actor_Spawn(&gPlayState->actorCtx, gPlayState, object.actorId, object.pos.x, object.pos.y,
                                       object.pos.z, object.rot.x, object.rot.y, object.rot.z, object.params);
                    if (copy != nullptr) {
                        ObjectExtension::GetInstance().Set<CoopCarriedCopy>(copy, CoopCarriedCopy{});
                        copy->room = -1;
                        if (object.isShot) {
                            SetupCoopShotCopy(copy);
                        }
                        // Not an explosive in our game, so it doesn't count towards our own bomb limit and enemies
                        // that react to bombs leave it alone
                        if (copy->category == ACTORCAT_EXPLOSIVE) {
                            Actor_ChangeCategory(gPlayState, &gPlayState->actorCtx, copy, ACTORCAT_PROP);
                        }
                    }
                }
                coopCarriedCopies[key] = copy;
                if (copy == nullptr) {
                    continue;
                }

                if (isExploding) {
                    PlayCoopExplosion(copy->world.pos);
                    Actor_Kill(copy);
                    coopCarriedCopies[key] = nullptr;
                    continue;
                }

                copy->world.pos = object.pos;
                copy->shape.rot = object.rot;
                if (object.scale >= 0.0f) {
                    Actor_SetScale(copy, object.scale);
                }
                if (object.isShot) {
                    copy->world.rot = object.worldRot;
                    if (copy->id == ACTOR_EN_BOOM) {
                        ((EnBoom*)copy)->activeTimer++; // It spins
                    }
                }
            }
        }
    }

    // What teammates stopped tracking: a bomb that was gone without blowing up (like in water) goes for us too, other
    // things land and carry on by themselves
    std::erase_if(coopCarriedCopies, [&](const auto& entry) {
        if (current.contains(entry.first)) {
            return false;
        }

        Actor* copy = entry.second;
        if (copy != nullptr && IsLiveActor(copy)) {
            if (copy->id == ACTOR_EN_ARROW || copy->id == ACTOR_EN_BOOM) {
                PlayCoopShotImpact(copy);
                Actor_Kill(copy);
            } else if (IsCoopExplosive(copy->id)) {
                Actor_Kill(copy);
            } else {
                ObjectExtension::GetInstance().Remove<CoopCarriedCopy>(copy);
            }
        }
        return true;
    });
}

// MARK: - NPCs

// Most NPCs only look at story progress when they spawn, like Mido who moves out of the way once you've shown him your
// sword and shield. When a teammate in our scene makes progress, the NPCs around us are respawned so they catch up
// right away instead of when we come back.
void Anchor::OnCoopTeammateFlagSet(uint32_t clientId, s16 flagType) {
    // Story progress only means the same thing in the same seed
    if (flagType == FLAG_EVENT_CHECK_INF && IsCoopSyncActive() && clients.contains(clientId) &&
        IsSameSeedPeer(clients[clientId])) {
        coopNpcRefreshPending = true;
    }
}

void Anchor::RefreshCoopNpcs() {
    if (!coopNpcRefreshPending) {
        return;
    }

    // Not while we're in a conversation or cutscene ourselves, the NPC could be part of it
    Player* player = GET_PLAYER(gPlayState);
    if (Message_GetState(&gPlayState->msgCtx) != TEXT_STATE_NONE || gPlayState->csCtx.state != CS_STATE_IDLE ||
        (player->stateFlags1 & (PLAYER_STATE1_TALKING | PLAYER_STATE1_IN_CUTSCENE)) ||
        gPlayState->transitionTrigger != TRANS_TRIGGER_OFF) {
        return;
    }
    coopNpcRefreshPending = false;

    struct NpcSpawn {
        s16 id;
        PosRot home;
        s16 params;
        s16 listIndex;
    };
    std::vector<NpcSpawn> npcs;

    for (Actor* actor = gPlayState->actorCtx.actorLists[ACTORCAT_NPC].head; actor != NULL; actor = actor->next) {
        // Only NPCs placed in our room, which leaves out teammates' dummy players and NPCs spawned by others
        if (actor->update == NULL || GetActorListIndex(actor) < 0 || actor->room != gPlayState->roomCtx.curRoom.num) {
            continue;
        }

        npcs.push_back({ actor->id, actor->home, actor->params, GetActorListIndex(actor) });
        Actor_Kill(actor);
    }

    for (const NpcSpawn& npc : npcs) {
        Actor* actor = Actor_Spawn(&gPlayState->actorCtx, gPlayState, npc.id, npc.home.pos.x, npc.home.pos.y,
                                   npc.home.pos.z, npc.home.rot.x, npc.home.rot.y, npc.home.rot.z, npc.params);
        if (actor != nullptr) {
            SetActorListIndex(actor, npc.listIndex);
        }
    }
}

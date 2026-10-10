#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <string>
#include <set>
#include <unordered_set>
#include <nlohmann/json.hpp>

#include "soh/Network/Anchor/Anchor.h"
#include "soh/Network/Anchor/JsonConversions.hpp"
#include "soh/ObjectExtension/ObjectExtension.h"
#include "soh/ObjectExtension/ActorListIndex.h"
#include "soh/ResourceManagerHelpers.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/Enhancements/randomizer/hook_handlers.h"

extern "C" {
#include "macros.h"
#include "functions.h"
#include "src/overlays/actors/ovl_Boss_Dodongo/z_boss_dodongo.h"
#include "src/overlays/actors/ovl_Boss_Fd/z_boss_fd.h"
#include "src/overlays/actors/ovl_Boss_Ganon/z_boss_ganon.h"
#include "src/overlays/actors/ovl_Boss_Ganon2/z_boss_ganon2.h"
#include "src/overlays/actors/ovl_Boss_Fd2/z_boss_fd2.h"
#include "src/overlays/actors/ovl_Arms_Hook/z_arms_hook.h"
#include "src/overlays/actors/ovl_Boss_Ganondrof/z_boss_ganondrof.h"
#include "src/overlays/actors/ovl_Boss_Goma/z_boss_goma.h"
#include "src/overlays/actors/ovl_En_Fhg_Fire/z_en_fhg_fire.h"
#include "src/overlays/actors/ovl_En_fHG/z_en_fhg.h"
#include "src/overlays/actors/ovl_Boss_Mo/z_boss_mo.h"
#include "src/overlays/actors/ovl_Boss_Sst/z_boss_sst.h"
#include "src/overlays/actors/ovl_Boss_Tw/z_boss_tw.h"
#include "src/overlays/actors/ovl_Boss_Va/z_boss_va.h"
#include "src/overlays/actors/ovl_En_Am/z_en_am.h"
#include "src/overlays/actors/ovl_En_Bb/z_en_bb.h"
#include "src/overlays/actors/ovl_En_Bili/z_en_bili.h"
#include "src/overlays/actors/ovl_En_Bw/z_en_bw.h"
#include "src/overlays/actors/ovl_En_Crow/z_en_crow.h"
#include "src/overlays/actors/ovl_En_Dekubaba/z_en_dekubaba.h"
#include "src/overlays/actors/ovl_En_Dekunuts/z_en_dekunuts.h"
#include "src/overlays/actors/ovl_En_Dodojr/z_en_dodojr.h"
#include "src/overlays/actors/ovl_En_Dodongo/z_en_dodongo.h"
#include "src/overlays/actors/ovl_En_Dh/z_en_dh.h"
#include "src/overlays/actors/ovl_En_Dha/z_en_dha.h"
#include "src/overlays/actors/ovl_En_Eiyer/z_en_eiyer.h"
#include "src/overlays/actors/ovl_En_Fd/z_en_fd.h"
#include "src/overlays/actors/ovl_En_Firefly/z_en_firefly.h"
#include "src/overlays/actors/ovl_En_Floormas/z_en_floormas.h"
#include "src/overlays/actors/ovl_En_Fw/z_en_fw.h"
#include "src/overlays/actors/ovl_En_Fz/z_en_fz.h"
#include "src/overlays/actors/ovl_En_GeldB/z_en_geldb.h"
#include "src/overlays/actors/ovl_En_Goma/z_en_goma.h"
#include "src/overlays/actors/ovl_En_Ik/z_en_ik.h"
#include "src/overlays/actors/ovl_En_Karebaba/z_en_karebaba.h"
#include "src/overlays/actors/ovl_En_Mb/z_en_mb.h"
#include "src/overlays/actors/ovl_En_Ny/z_en_ny.h"
#include "src/overlays/actors/ovl_En_Okuta/z_en_okuta.h"
#include "src/overlays/actors/ovl_En_Poh/z_en_poh.h"
#include "src/overlays/actors/ovl_En_Rd/z_en_rd.h"
#include "src/overlays/actors/ovl_En_Rr/z_en_rr.h"
#include "src/overlays/actors/ovl_En_Sb/z_en_sb.h"
#include "src/overlays/actors/ovl_En_Skb/z_en_skb.h"
#include "src/overlays/actors/ovl_En_St/z_en_st.h"
#include "src/overlays/actors/ovl_En_Sw/z_en_sw.h"
#include "src/overlays/actors/ovl_En_Test/z_en_test.h"
#include "src/overlays/actors/ovl_En_Tite/z_en_tite.h"
#include "src/overlays/actors/ovl_En_Vali/z_en_vali.h"
#include "src/overlays/actors/ovl_En_Vm/z_en_vm.h"
#include "src/overlays/actors/ovl_En_Wallmas/z_en_wallmas.h"
#include "src/overlays/actors/ovl_En_Wf/z_en_wf.h"
#include "src/overlays/actors/ovl_En_Zf/z_en_zf.h"
extern PlayState* gPlayState;

void BossDodongo_IntroCutscene(BossDodongo* bossDodongo, PlayState* play);
void BossDodongo_Walk(BossDodongo* bossDodongo, PlayState* play);
void BossDodongo_Inhale(BossDodongo* bossDodongo, PlayState* play);
void BossDodongo_BlowFire(BossDodongo* bossDodongo, PlayState* play);
void BossDodongo_Roll(BossDodongo* bossDodongo, PlayState* play);
void BossDodongo_Explode(BossDodongo* bossDodongo, PlayState* play);
void BossDodongo_LayDown(BossDodongo* bossDodongo, PlayState* play);
void BossDodongo_Vulnerable(BossDodongo* bossDodongo, PlayState* play);
void BossDodongo_GetUp(BossDodongo* bossDodongo, PlayState* play);
void BossDodongo_Damaged(BossDodongo* bossDodongo, PlayState* play);
void BossDodongo_DeathCutscene(BossDodongo* bossDodongo, PlayState* play);
void BossDodongo_SetupExplode(BossDodongo* bossDodongo);
void BossDodongo_SetupDeathCutscene(BossDodongo* bossDodongo);
void BossFd_Fly(BossFd* bossFd, PlayState* play);
void BossFd_Wait(BossFd* bossFd, PlayState* play);
void BossFd2_Emerge(BossFd2* bossFd2, PlayState* play);
void BossFd2_Idle(BossFd2* bossFd2, PlayState* play);
void BossFd2_Burrow(BossFd2* bossFd2, PlayState* play);
void BossFd2_BreatheFire(BossFd2* bossFd2, PlayState* play);
void BossFd2_ClawSwipe(BossFd2* bossFd2, PlayState* play);
void BossFd2_Vulnerable(BossFd2* bossFd2, PlayState* play);
void BossFd2_Damaged(BossFd2* bossFd2, PlayState* play);
void BossFd2_Death(BossFd2* bossFd2, PlayState* play);
void BossFd2_Wait(BossFd2* bossFd2, PlayState* play);
void BossFd2_SetupDeath(BossFd2* bossFd2, PlayState* play);
void BossSst_HeadLurk(BossSst* bossSst, PlayState* play);
void BossSst_HeadIntro(BossSst* bossSst, PlayState* play);
void BossSst_HeadNeutral(BossSst* bossSst, PlayState* play);
void BossSst_HeadWait(BossSst* bossSst, PlayState* play);
void BossSst_HeadDamagedHand(BossSst* bossSst, PlayState* play);
void BossSst_HeadReadyCharge(BossSst* bossSst, PlayState* play);
void BossSst_HeadCharge(BossSst* bossSst, PlayState* play);
void BossSst_HeadEndCharge(BossSst* bossSst, PlayState* play);
void BossSst_HeadFrozenHand(BossSst* bossSst, PlayState* play);
void BossSst_HeadUnfreezeHand(BossSst* bossSst, PlayState* play);
void BossSst_HeadStunned(BossSst* bossSst, PlayState* play);
void BossSst_HeadVulnerable(BossSst* bossSst, PlayState* play);
void BossSst_HeadDamage(BossSst* bossSst, PlayState* play);
void BossSst_HeadRecover(BossSst* bossSst, PlayState* play);
void BossSst_HeadDeath(BossSst* bossSst, PlayState* play);
void BossSst_HeadThrash(BossSst* bossSst, PlayState* play);
void BossSst_HeadDarken(BossSst* bossSst, PlayState* play);
void BossSst_HeadFall(BossSst* bossSst, PlayState* play);
void BossSst_HeadMelt(BossSst* bossSst, PlayState* play);
void BossSst_HeadFinish(BossSst* bossSst, PlayState* play);
void BossSst_HandWait(BossSst* bossSst, PlayState* play);
void BossSst_HandDownbeat(BossSst* bossSst, PlayState* play);
void BossSst_HandOffbeat(BossSst* bossSst, PlayState* play);
void BossSst_HandDownbeatEnd(BossSst* bossSst, PlayState* play);
void BossSst_HandOffbeatEnd(BossSst* bossSst, PlayState* play);
void BossSst_HandReadySlam(BossSst* bossSst, PlayState* play);
void BossSst_HandSlam(BossSst* bossSst, PlayState* play);
void BossSst_HandEndSlam(BossSst* bossSst, PlayState* play);
void BossSst_HandReadySweep(BossSst* bossSst, PlayState* play);
void BossSst_HandSweep(BossSst* bossSst, PlayState* play);
void BossSst_HandReadyPunch(BossSst* bossSst, PlayState* play);
void BossSst_HandPunch(BossSst* bossSst, PlayState* play);
void BossSst_HandReadyClap(BossSst* bossSst, PlayState* play);
void BossSst_HandClap(BossSst* bossSst, PlayState* play);
void BossSst_HandEndClap(BossSst* bossSst, PlayState* play);
void BossSst_HandReadyGrab(BossSst* bossSst, PlayState* play);
void BossSst_HandGrab(BossSst* bossSst, PlayState* play);
void BossSst_HandCrush(BossSst* bossSst, PlayState* play);
void BossSst_HandEndCrush(BossSst* bossSst, PlayState* play);
void BossSst_HandSwing(BossSst* bossSst, PlayState* play);
void BossSst_HandRetreat(BossSst* bossSst, PlayState* play);
void BossSst_HandReel(BossSst* bossSst, PlayState* play);
void BossSst_HandReadyShake(BossSst* bossSst, PlayState* play);
void BossSst_HandShake(BossSst* bossSst, PlayState* play);
void BossSst_HandReadyCharge(BossSst* bossSst, PlayState* play);
void BossSst_HandFrozen(BossSst* bossSst, PlayState* play);
void BossSst_HandReadyBreakIce(BossSst* bossSst, PlayState* play);
void BossSst_HandBreakIce(BossSst* bossSst, PlayState* play);
void BossSst_HandStunned(BossSst* bossSst, PlayState* play);
void BossSst_HandDamage(BossSst* bossSst, PlayState* play);
void BossSst_HandRecover(BossSst* bossSst, PlayState* play);
void BossSst_HandThrash(BossSst* bossSst, PlayState* play);
void BossSst_HandDarken(BossSst* bossSst, PlayState* play);
void BossSst_HandFall(BossSst* bossSst, PlayState* play);
void BossSst_HandMelt(BossSst* bossSst, PlayState* play);
void BossSst_HandFinish(BossSst* bossSst, PlayState* play);
void BossSst_HeadSetupDeath(BossSst* bossSst, PlayState* play);
void BossSst_HandSetupDamage(BossSst* hand);
void BossSst_HandReleasePlayer(BossSst* bossSst, PlayState* play, s32 dropPlayer);
extern s32 gBossSstHandState[];
void BossGanondrof_Intro(BossGanondrof* bossGanondrof, PlayState* play);
void BossGanondrof_Paintings(BossGanondrof* bossGanondrof, PlayState* play);
void BossGanondrof_Neutral(BossGanondrof* bossGanondrof, PlayState* play);
void BossGanondrof_Throw(BossGanondrof* bossGanondrof, PlayState* play);
void BossGanondrof_Block(BossGanondrof* bossGanondrof, PlayState* play);
void BossGanondrof_Return(BossGanondrof* bossGanondrof, PlayState* play);
void BossGanondrof_Charge(BossGanondrof* bossGanondrof, PlayState* play);
void BossGanondrof_Stunned(BossGanondrof* bossGanondrof, PlayState* play);
void BossGanondrof_Death(BossGanondrof* bossGanondrof, PlayState* play);
void BossGanondrof_SetupDeath(BossGanondrof* bossGanondrof, PlayState* play);
void EnfHG_Intro(EnfHG* enfHG, PlayState* play);
void EnfHG_Approach(EnfHG* enfHG, PlayState* play);
void EnfHG_Attack(EnfHG* enfHG, PlayState* play);
void EnfHG_Damage(EnfHG* enfHG, PlayState* play);
void EnfHG_Retreat(EnfHG* enfHG, PlayState* play);
void EnfHG_Done(EnfHG* enfHG, PlayState* play);
void BossGanon_IntroCutscene(BossGanon* bossGanon, PlayState* play);
void BossGanon_DeathAndTowerCutscene(BossGanon* bossGanon, PlayState* play);
void BossGanon_Wait(BossGanon* bossGanon, PlayState* play);
void BossGanon_ChargeLightBall(BossGanon* bossGanon, PlayState* play);
void BossGanon_PlayTennis(BossGanon* bossGanon, PlayState* play);
void BossGanon_PoundFloor(BossGanon* bossGanon, PlayState* play);
void BossGanon_ChargeBigMagic(BossGanon* bossGanon, PlayState* play);
void BossGanon_Block(BossGanon* bossGanon, PlayState* play);
void BossGanon_HitByLightBall(BossGanon* bossGanon, PlayState* play);
void BossGanon_Vulnerable(BossGanon* bossGanon, PlayState* play);
void BossGanon_Damaged(BossGanon* bossGanon, PlayState* play);
void BossGanon_SetupWait(BossGanon* bossGanon, PlayState* play);
void BossGanon_SetupDeathCutscene(BossGanon* bossGanon, PlayState* play);
s32 BossGanon_CheckFallingPlatforms(BossGanon* bossGanon, PlayState* play, Vec3f* checkPos);
void BossGanonEff_SpawnSparkle(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, f32 scale, s16 arg6);
void BossTw_TurnToPlayer(BossTw* bossTw, PlayState* play);
void BossTw_FlyTo(BossTw* bossTw, PlayState* play);
void BossTw_ShootBeam(BossTw* bossTw, PlayState* play);
void BossTw_FinishBeamShoot(BossTw* bossTw, PlayState* play);
void BossTw_HitByBeam(BossTw* bossTw, PlayState* play);
void BossTw_Laugh(BossTw* bossTw, PlayState* play);
void BossTw_Spin(BossTw* bossTw, PlayState* play);
void BossTw_Wait(BossTw* bossTw, PlayState* play);
void BossTw_TwinrovaFly(BossTw* bossTw, PlayState* play);
void BossTw_TwinrovaArriveAtTarget(BossTw* bossTw, PlayState* play);
void BossTw_TwinrovaChargeBlast(BossTw* bossTw, PlayState* play);
void BossTw_TwinrovaShootBlast(BossTw* bossTw, PlayState* play);
void BossTw_TwinrovaDoneBlastShoot(BossTw* bossTw, PlayState* play);
void BossTw_TwinrovaStun(BossTw* bossTw, PlayState* play);
void BossTw_TwinrovaSpin(BossTw* bossTw, PlayState* play);
void BossTw_TwinrovaLaugh(BossTw* bossTw, PlayState* play);
void BossTw_TwinrovaGetUp(BossTw* bossTw, PlayState* play);
void BossTw_TwinrovaIntroCS(BossTw* bossTw, PlayState* play);
void BossTw_TwinrovaMergeCS(BossTw* bossTw, PlayState* play);
void BossTw_TwinrovaDeathCS(BossTw* bossTw, PlayState* play);
void BossTw_SetupHitByBeam(BossTw* bossTw, PlayState* play);
void BossTw_TwinrovaSetupDeathCS(BossTw* bossTw, PlayState* play);
void BossTw_SpawnGroundBlast(BossTw* bossTw, PlayState* play, s16 blastType);
void BossGanon_SetupBlock(BossGanon* bossGanon, PlayState* play);
void BossTw_SetupSpin(BossTw* bossTw, PlayState* play);
void BossVa_BodyPhase1(BossVa* bossVa, PlayState* play);
void BossVa_BodyPhase2(BossVa* bossVa, PlayState* play);
void BossVa_BodyPhase3(BossVa* bossVa, PlayState* play);
void BossVa_BodyPhase4(BossVa* bossVa, PlayState* play);
void BossVa_SupportAttached(BossVa* bossVa, PlayState* play);
void BossVa_SupportCut(BossVa* bossVa, PlayState* play);
void BossVa_Stump(BossVa* bossVa, PlayState* play);
void BossVa_ZapperAttack(BossVa* bossVa, PlayState* play);
void BossVa_ZapperDamaged(BossVa* bossVa, PlayState* play);
void BossVa_ZapperDeath(BossVa* bossVa, PlayState* play);
void BossVa_ZapperEnraged(BossVa* bossVa, PlayState* play);
void BossVa_ZapperHold(BossVa* bossVa, PlayState* play);
void BossVa_BariPhase2Attack(BossVa* bossVa, PlayState* play);
void BossVa_BariPhase3Attack(BossVa* bossVa, PlayState* play);
void BossVa_BariPhase3Stunned(BossVa* bossVa, PlayState* play);
void BossVa_BariDeath(BossVa* bossVa, PlayState* play);
void BossVa_BodyIntro(BossVa* bossVa, PlayState* play);
void BossVa_BodyDeath(BossVa* bossVa, PlayState* play);
void BossVa_SetupBodyDeath(BossVa* bossVa, PlayState* play);
void BossVa_SetupBariDeath(BossVa* bossVa);
extern void* gBossVaCoopState[];
extern size_t gBossVaCoopStateSizes[];
extern s32 gBossVaCoopStateCount;
void func_808FFDB0(BossGanon2* bossGanon2, PlayState* play);
void func_808FFEBC(BossGanon2* bossGanon2, PlayState* play);
void func_808FFFE0(BossGanon2* bossGanon2, PlayState* play);
void func_80900104(BossGanon2* bossGanon2, PlayState* play);
void func_8090026C(BossGanon2* bossGanon2, PlayState* play);
void func_80900344(BossGanon2* bossGanon2, PlayState* play);
void func_80900650(BossGanon2* bossGanon2, PlayState* play);
void func_808FD5F4(BossGanon2* bossGanon2, PlayState* play);
void func_80900890(BossGanon2* bossGanon2, PlayState* play);
void func_8090120C(BossGanon2* bossGanon2, PlayState* play);
void func_80900818(BossGanon2* bossGanon2, PlayState* play); // Ganon downed
void func_80901020(BossGanon2* bossGanon2, PlayState* play); // Ganon defeated
void BossGoma_SetupDefeated(BossGoma* bossGoma, PlayState* play);
void BossGoma_Defeated(BossGoma* bossGoma, PlayState* play);
void BossGoma_FloorAttackPosture(BossGoma* bossGoma, PlayState* play);
void BossGoma_FloorPrepareAttack(BossGoma* bossGoma, PlayState* play);
void BossGoma_FloorAttack(BossGoma* bossGoma, PlayState* play);
void BossGoma_FloorDamaged(BossGoma* bossGoma, PlayState* play);
void BossGoma_FloorLandStruckDown(BossGoma* bossGoma, PlayState* play);
void BossGoma_FloorLand(BossGoma* bossGoma, PlayState* play);
void BossGoma_FloorStunned(BossGoma* bossGoma, PlayState* play);
void BossGoma_FallJump(BossGoma* bossGoma, PlayState* play);
void BossGoma_FallStruckDown(BossGoma* bossGoma, PlayState* play);
void BossGoma_CeilingSpawnGohmas(BossGoma* bossGoma, PlayState* play);
void BossGoma_CeilingPrepareSpawnGohmas(BossGoma* bossGoma, PlayState* play);
void BossGoma_FloorIdle(BossGoma* bossGoma, PlayState* play);
void BossGoma_CeilingIdle(BossGoma* bossGoma, PlayState* play);
void BossGoma_FloorMain(BossGoma* bossGoma, PlayState* play);
void BossGoma_WallClimb(BossGoma* bossGoma, PlayState* play);
void BossGoma_CeilingMoveToCenter(BossGoma* bossGoma, PlayState* play);
void EnDekubaba_Wait(EnDekubaba* enDekubaba, PlayState* play);
void EnDekubaba_Lunge(EnDekubaba* enDekubaba, PlayState* play);
void EnDekubaba_PrunedSomersault(EnDekubaba* enDekubaba, PlayState* play);
void EnDekubaba_SetupDeadStickDrop(EnDekubaba* enDekubaba, PlayState* play);
void EnFd_Reappear(EnFd* enFd, PlayState* play);
void EnFd_Run(EnFd* enFd, PlayState* play);
void EnFloormas_Draw(Actor* thisx, PlayState* play);
void EnFloormas_DrawHighlighted(Actor* thisx, PlayState* play);
void EnPoh_SetupDeath(EnPoh* enPoh, PlayState* play);
void func_80ADF15C(EnPoh* enPoh, PlayState* play); // Poe dying
void EnPoh_Idle(EnPoh* enPoh, PlayState* play);
void EnVali_FloatIdle(EnVali* enVali, PlayState* play);
void EnVali_Lurk(EnVali* enVali, PlayState* play);
}

/**
 * COOP_ENEMY_SNAPSHOT, COOP_ENEMY_HIT, COOP_ENEMY_ATTACK, COOP_ENEMY_PROJECTILE
 *
 * Mirrored enemies are run by one player per room, the authority (the co-op peer in the room with the lowest client
 * id). Everyone else sees a puppet: its AI doesn't run, it just follows the position, rotation, health and animation
 * the authority sends ~10 times a second.
 *
 * - Hitting a puppet sends the hit to the authority (COOP_ENEMY_HIT), which applies it to the real enemy exactly like a
 *   collision would, so its own damage table, hit reaction and death all play out normally.
 * - Puppets can't hurt anyone. When the real enemy hits a teammate's dummy player on the authority's screen, that
 *   teammate takes the hit (COOP_ENEMY_ATTACK).
 * - When a puppet hasn't received a snapshot for a while (the authority left the room), its AI simply resumes.
 * - On the authority, enemies go after the nearest player in the room, not just the authority's own.
 * - What they shoot is spawned for everyone (COOP_ENEMY_PROJECTILE), but only the authority's copy can hurt anyone;
 *   it hits teammates the same way the enemy itself does.
 *
 * Bosses are mirrored too, but only during the fight itself: their intro and defeat are cutscenes that drive the
 * player and camera, so every player runs those with their own boss. A puppet boss joins the fight once both its own
 * intro and the authority's are over, and when the authority's boss is defeated, ours is defeated right away too, so
 * every player gets their own defeat cutscene, heart container and blue warp.
 *
 * Unlike enemy puppets, boss puppets keep running their own update (driven puppets): much of what a boss looks like
 * comes from it (Volvagia's body trails behind her head, her mane, fire and embers, dust, sounds). Every snapshot sets
 * what it's doing to the authority's (action, timers, targets, animation), and its position is nudged towards the
 * authority's after each update. Hits on it still go to the authority, its health always comes from the authority,
 * and what it spawns comes from the authority too. Its attacks hurt whoever they hit in their own game, so the
 * authority's boss hitting a teammate's dummy player doesn't count.
 *
 * In the randomizer, a boss whose soul a player hasn't found yet doesn't appear in their game at all, so they take no
 * part in its fight: they never run the room for the others, get no puppet of the boss, and the boss doesn't go after
 * them or hit them. Finding the soul while in the room changes nothing until the room is entered again, like without
 * co-op.
 *
 * A few moving platforms and traps are mirrored too, so they move in step for everyone. Unlike enemies they keep
 * running their own update (so they move smoothly and still hurt you), and are just nudged towards the authority's
 * position and rotation. Every player reports which of them they stand on, so platforms that start moving when stood
 * on also start in the authority's game when a teammate steps on.
 */

#define NO_SKELETON SIZE_MAX // For enemies animated without a skeleton, of which only the position is mirrored

// Where a mirrored boss is in its fight
typedef enum {
    COOP_BOSS_WAITING, // Intro (or anything else before the fight), run by every player for themselves
    COOP_BOSS_FIGHTING,
    COOP_BOSS_DEFEATED,
    COOP_BOSS_CUTSCENE, // During the fight, run by every player for themselves (Ganon downed, waiting for the Master
                        // Sword)
} CoopBossFight;

typedef enum {
    COOP_PUPPET_SPAWN_NONE,
    COOP_PUPPET_SPAWN,
    COOP_PUPPET_SPAWN_AS_CHILD, // Of the same enemy as for the authority, which it needs once it runs by itself
} CoopPuppetSpawn;

// Plain data (no pointers) in the enemy's struct that its draw depends on, copied to puppets as is
struct CoopStateField {
    size_t offset;
    size_t size;
};

struct CoopEnemyAdapter {
    s16 actorId;
    bool canTargetTeammates; // Only for enemies whose AI just reads the player, never changes it (no grabs, freezes)
    size_t skelAnimeOffset;
    std::vector<size_t> hurtColliderOffsets;   // Colliders the enemy registers as AC (can be hit)
    bool (*isVariant)(Actor* actor) = nullptr; // Whether this variant of the actor is a mirrored enemy (default: all)
    std::vector<CoopStateField> stateFields = {};
    // For enemies whose draw depends on what they're doing: the authority sends a small phase number, and puppets set
    // their action to one that draws the same way (it's never run while they're a puppet)
    u16 (*getPhase)(Actor* actor) = nullptr;
    void (*setPhase)(Actor* actor, u16 phase) = nullptr;
    // When the enemy is gone from the authority's game without dying in a way that's shared. Returns true if the puppet
    // stays and runs its own update from now on, otherwise it's removed.
    bool (*onAuthorityGone)(Actor* actor) = nullptr;
    // For enemies another one spawns during the fight (a Bari splitting into Biris): we spawn our own puppet of one
    // when the authority has it and we don't
    CoopPuppetSpawn puppetSpawn = COOP_PUPPET_SPAWN_NONE;
    // For bosses: where it is in the fight, and how to defeat ours when the authority's is
    CoopBossFight (*getBossFight)(Actor* actor) = nullptr;
    void (*defeat)(Actor* actor) = nullptr;
    // Something a teammate did to the boss in their game that it reacts to by itself rather than through a hit (King
    // Dodongo swallowing their bomb). Checked after a puppet ran its own update, given its phase from before: returns
    // the event to send to the authority, if any, and the authority's boss then does the same.
    u8 (*getLocalEvent)(Actor* actor, u16 phaseBefore) = nullptr;
    void (*applyEvent)(Actor* actor, u8 event, Vec3f pos) = nullptr; // pos: where the teammate's puppet is
    // Undoes what in the authority's state only makes sense for the authority's player (like holding them), after a
    // puppet boss took it over
    void (*sanitizeState)(Actor* actor) = nullptr;
    // State of the fight kept outside the actor (Barinade's), sent and applied along with stateFields
    std::vector<u8> (*getExtraState)(Actor* actor) = nullptr;
    void (*setExtraState)(Actor* actor, const std::vector<u8>& state) = nullptr;
    // Starts the cutscene the authority's boss is in (COOP_BOSS_CUTSCENE) for ours
    void (*enterCutscene)(Actor* actor) = nullptr;
};

#define STATE_RANGE(type, first, end)                                      \
    CoopStateField {                                                       \
        offsetof(type, first), offsetof(type, end) - offsetof(type, first) \
    }

#define STATE_FIELD(type, field)                   \
    CoopStateField {                               \
        offsetof(type, field), sizeof(type::field) \
    }

static u16 GetDekuBabaPhase(Actor* actor) {
    EnDekubabaActionFunc actionFunc = ((EnDekubaba*)actor)->actionFunc;
    if (actionFunc == EnDekubaba_Wait) {
        return 0;
    }
    return actionFunc == EnDekubaba_PrunedSomersault ? 2 : 1;
}

static void SetDekuBabaPhase(Actor* actor, u16 phase) {
    static const EnDekubabaActionFunc sPhaseActions[] = { EnDekubaba_Wait, EnDekubaba_Lunge,
                                                          EnDekubaba_PrunedSomersault };
    ((EnDekubaba*)actor)->actionFunc = sPhaseActions[phase < ARRAY_COUNT(sPhaseActions) ? phase : 1];
}

// A Deku Baba cut off at the stem turns into a Deku Stick to pick up, which isn't an enemy anymore. Everyone gets the
// stick, and once one player takes it, it's gone for everyone (see CoopActors.cpp).
static bool OnDekuBabaAuthorityGone(Actor* actor) {
    if (actor->colChkInfo.health != 0 || GetDekuBabaPhase(actor) != 2) {
        return false;
    }

    EnDekubaba_SetupDeadStickDrop((EnDekubaba*)actor, gPlayState);
    return true;
}

// Small Floormasters aren't drawn until the big one splits up
static u16 GetFloormasterPhase(Actor* actor) {
    if (actor->draw == NULL) {
        return 0;
    }
    return actor->draw == EnFloormas_DrawHighlighted ? 2 : 1;
}

static void SetFloormasterPhase(Actor* actor, u16 phase) {
    static const ActorFunc sPhaseDraws[] = { NULL, EnFloormas_Draw, EnFloormas_DrawHighlighted };
    actor->draw = sPhaseDraws[phase < ARRAY_COUNT(sPhaseDraws) ? phase : 1];
}

// A Flare Dancer's fire is drawn differently while it comes back
static u16 GetFlareDancerPhase(Actor* actor) {
    return ((EnFd*)actor)->actionFunc == EnFd_Reappear ? 1 : 0;
}

static void SetFlareDancerPhase(Actor* actor, u16 phase) {
    ((EnFd*)actor)->actionFunc = phase == 1 ? EnFd_Reappear : EnFd_Run;
}

// A Bari only shows its lightning while floating around
static u16 GetBariPhase(Actor* actor) {
    return ((EnVali*)actor)->actionFunc == EnVali_FloatIdle ? 1 : 0;
}

static void SetBariPhase(Actor* actor, u16 phase) {
    ((EnVali*)actor)->actionFunc = phase == 1 ? EnVali_FloatIdle : EnVali_Lurk;
}

// A Poe is drawn differently while dying, and then turns into a soul to catch in a bottle, which isn't an enemy
// anymore. Like a Deku Baba's stick, everyone gets the soul, and once one player catches it it's gone for everyone.
static u16 GetPoePhase(Actor* actor) {
    return ((EnPoh*)actor)->actionFunc == func_80ADF15C ? 1 : 0;
}

static void SetPoePhase(Actor* actor, u16 phase) {
    EnPoh* poe = (EnPoh*)actor;
    if (phase == 1) {
        poe->actionFunc = func_80ADF15C;
    } else if (poe->actionFunc == func_80ADF15C) {
        // Any other action is drawn the same way
        poe->actionFunc = EnPoh_Idle;
    }
}

static bool OnPoeAuthorityGone(Actor* actor) {
    if (actor->colChkInfo.health != 0 || GetPoePhase(actor) != 1) {
        return false;
    }

    EnPoh_SetupDeath((EnPoh*)actor, gPlayState);
    return true;
}

// Queen Gohma. Her actions are sent as the phase, so she carries on from the authority's when she runs by herself
// again.
static const BossGomaActionFunc sBossGomaFightActions[] = {
    BossGoma_FloorMain,
    BossGoma_FloorIdle,
    BossGoma_FloorAttackPosture,
    BossGoma_FloorPrepareAttack,
    BossGoma_FloorAttack,
    BossGoma_FloorDamaged,
    BossGoma_FloorLandStruckDown,
    BossGoma_FloorLand,
    BossGoma_FloorStunned,
    BossGoma_FallJump,
    BossGoma_FallStruckDown,
    BossGoma_CeilingSpawnGohmas,
    BossGoma_CeilingPrepareSpawnGohmas,
    BossGoma_CeilingIdle,
    BossGoma_WallClimb,
    BossGoma_CeilingMoveToCenter,
};

static CoopBossFight GetBossGomaFight(Actor* actor) {
    BossGoma* gohma = (BossGoma*)actor;
    if (gohma->actionFunc == BossGoma_Defeated) {
        return COOP_BOSS_DEFEATED;
    }
    // Her intro ends by letting her loose
    return gohma->disableGameplayLogic ? COOP_BOSS_WAITING : COOP_BOSS_FIGHTING;
}

static u16 GetBossGomaPhase(Actor* actor) {
    BossGomaActionFunc actionFunc = ((BossGoma*)actor)->actionFunc;
    for (u8 i = 0; i < ARRAY_COUNT(sBossGomaFightActions); i++) {
        if (sBossGomaFightActions[i] == actionFunc) {
            return i;
        }
    }
    return 0;
}

static void SetBossGomaPhase(Actor* actor, u16 phase) {
    ((BossGoma*)actor)->actionFunc = sBossGomaFightActions[phase < ARRAY_COUNT(sBossGomaFightActions) ? phase : 0];
}

// The same as her own death in BossGoma_UpdateHit
static void DefeatBossGoma(Actor* actor) {
    actor->colChkInfo.health = 0;
    BossGoma_SetupDefeated((BossGoma*)actor, gPlayState);
    Enemy_StartFinishingBlow(gPlayState, actor);
    GameInteractor_ExecuteOnBossDefeat(actor);
}

// For bosses whose actions are sent as the phase
template <typename T, size_t N, typename F> static u16 GetActionPhase(T (&actions)[N], F actionFunc) {
    for (u8 i = 0; i < N; i++) {
        if (actions[i] == actionFunc) {
            return i;
        }
    }
    return 0;
}

template <typename T, size_t N> static T GetPhaseAction(T (&actions)[N], u16 phase) {
    return actions[phase < N ? phase : 0];
}

// King Dodongo, whose health is a field of his own (see GetCoopHealth)
static const BossDodongoActionFunc sKingDodongoActions[] = {
    BossDodongo_Walk,    BossDodongo_Inhale,     BossDodongo_BlowFire, BossDodongo_Roll,    BossDodongo_Explode,
    BossDodongo_LayDown, BossDodongo_Vulnerable, BossDodongo_GetUp,    BossDodongo_Damaged,
};

#define KING_DODONGO_ATE_BOMB 1

static CoopBossFight GetKingDodongoFight(Actor* actor) {
    BossDodongoActionFunc actionFunc = ((BossDodongo*)actor)->actionFunc;
    if (actionFunc == BossDodongo_IntroCutscene) {
        return COOP_BOSS_WAITING;
    }
    return actionFunc == BossDodongo_DeathCutscene ? COOP_BOSS_DEFEATED : COOP_BOSS_FIGHTING;
}

// He swallows the bombs thrown into his mouth while inhaling. A teammate's bomb is only a copy for show in the
// authority's game, so the teammate's own puppet swallows it, and the authority's King Dodongo then does too.
static u8 GetKingDodongoLocalEvent(Actor* actor, u16 phaseBefore) {
    BossDodongoActionFunc before = GetPhaseAction(sKingDodongoActions, phaseBefore);
    bool ateBomb = before == BossDodongo_Inhale && ((BossDodongo*)actor)->actionFunc == BossDodongo_Explode;
    return ateBomb ? KING_DODONGO_ATE_BOMB : 0;
}

static void ApplyKingDodongoEvent(Actor* actor, u8 event, Vec3f pos) {
    BossDodongo* kingDodongo = (BossDodongo*)actor;
    if (event == KING_DODONGO_ATE_BOMB && kingDodongo->actionFunc == BossDodongo_Inhale) {
        Audio_PlayActorSound2(actor, NA_SE_EN_DODO_K_DRINK);
        BossDodongo_SetupExplode(kingDodongo);
    }
}

// The same as his own death in BossDodongo_UpdateDamage
static void DefeatKingDodongo(Actor* actor) {
    ((BossDodongo*)actor)->health = 0;
    BossDodongo_SetupDeathCutscene((BossDodongo*)actor);
    Enemy_StartFinishingBlow(gPlayState, actor);
}

// Morpha's core, which keeps her health, and her tentacles (the first spawned with the core, the second once she's been
// hit three times), which are the same actor. Their actions are all in a state number of their own (work[0]).
#define MO_BATTLE 0
#define MO_DEATH_START 100
#define MO_TENT_READY 0
#define MO_TENT_GRAB 4
#define MO_TENT_SHAKE 5
#define MO_TENT_DEATH_START 200
#define MORPHA_PULLED 1

static BossMo* FindMorphaCore() {
    for (Actor* actor = gPlayState->actorCtx.actorLists[ACTORCAT_BOSS].head; actor != NULL; actor = actor->next) {
        if (actor->id == ACTOR_BOSS_MO && actor->params != BOSSMO_TENTACLE && actor->update != NULL) {
            return (BossMo*)actor;
        }
    }
    return nullptr;
}

// The tentacles go by the core
static CoopBossFight GetMorphaFight(Actor* actor) {
    BossMo* core = actor->params == BOSSMO_TENTACLE ? FindMorphaCore() : (BossMo*)actor;
    if (core == nullptr || core->csState >= MO_DEATH_START ||
        ((BossMo*)actor)->work[MO_TENT_ACTION_STATE] >= MO_TENT_DEATH_START) {
        return COOP_BOSS_DEFEATED;
    }
    return core->csState == MO_BATTLE ? COOP_BOSS_FIGHTING : COOP_BOSS_WAITING;
}

static bool IsPulledByOurHookshot(Actor* actor) {
    for (Actor* hook = gPlayState->actorCtx.actorLists[ACTORCAT_ITEMACTION].head; hook != NULL; hook = hook->next) {
        if (hook->id == ACTOR_ARMS_HOOK && hook->update != NULL && ((ArmsHook*)hook)->grabbed == actor) {
            return true;
        }
    }
    return false;
}

// The authority's core doesn't feel a teammate's hookshot pulling it, so the teammate's puppet tells it where it is
static u8 GetMorphaLocalEvent(Actor* actor, u16 phaseBefore) {
    return actor->params != BOSSMO_TENTACLE && IsPulledByOurHookshot(actor) ? MORPHA_PULLED : 0;
}

static void ApplyMorphaEvent(Actor* actor, u8 event, Vec3f pos) {
    if (event == MORPHA_PULLED && actor->params != BOSSMO_TENTACLE) {
        actor->world.pos = pos;
    }
}

// A tentacle holding the authority's player would pull ours in too, its grab acts on whoever is the player
static void SanitizeMorphaState(Actor* actor) {
    BossMo* part = (BossMo*)actor;
    s16* state = &part->work[MO_TENT_ACTION_STATE];
    if (actor->params == BOSSMO_TENTACLE && (*state == MO_TENT_GRAB || *state == MO_TENT_SHAKE) &&
        GET_PLAYER(gPlayState)->actor.parent != actor) {
        *state = MO_TENT_READY;
    }
}

// The same as her own death in BossMo_CoreCollisionCheck
static void DefeatMorpha(Actor* actor) {
    if (actor->params == BOSSMO_TENTACLE) {
        return;
    }

    BossMo* core = (BossMo*)actor;
    actor->colChkInfo.health = 0;
    Enemy_StartFinishingBlow(gPlayState, actor);
    GameInteractor_ExecuteOnBossDefeat(actor);
    Audio_QueueSeqCmd(0x1 << 28 | SEQ_PLAYER_BGM_MAIN << 24 | 0x100FF);
    core->csState = MO_DEATH_START;
    for (Actor* tent = gPlayState->actorCtx.actorLists[ACTORCAT_BOSS].head; tent != NULL; tent = tent->next) {
        if (tent->id != ACTOR_BOSS_MO || tent->params != BOSSMO_TENTACLE || tent->update == NULL) {
            continue;
        }
        if (tent->parent == actor) {
            ((BossMo*)tent)->drawActor = false;
            ((BossMo*)tent)->work[MO_TENT_ACTION_STATE] = MO_TENT_DEATH_START;
            ((BossMo*)tent)->baseAlpha = 0.0f;
        } else {
            ((BossMo*)tent)->tent2KillTimer = 1;
        }
    }
    Player* player = GET_PLAYER(gPlayState);
    if (player->actor.parent != NULL) {
        player->av2.actionVar2 = 0x65;
        player->actor.parent = NULL;
        player->csAction = 0;
    }
}

// Phantom Ganon, his fakes in the paintings (the same actor) and his horse, which runs the painting phase of the fight.
// The fakes are spawned at random, so they come from the authority's game, as children of the horse.
static const BossGanondrofActionFunc sPhantomGanonActions[] = {
    BossGanondrof_Paintings, BossGanondrof_Neutral, BossGanondrof_Throw,   BossGanondrof_Block,
    BossGanondrof_Return,    BossGanondrof_Charge,  BossGanondrof_Stunned,
};
static const EnfHGActionFunc sPhantomHorseActions[] = {
    EnfHG_Approach, EnfHG_Attack, EnfHG_Damage, EnfHG_Retreat, EnfHG_Done,
};

static BossGanondrof* FindPhantomGanon() {
    for (Actor* actor = gPlayState->actorCtx.actorLists[ACTORCAT_BOSS].head; actor != NULL; actor = actor->next) {
        if (actor->id == ACTOR_BOSS_GANONDROF && actor->params == GND_REAL_BOSS && actor->update != NULL) {
            return (BossGanondrof*)actor;
        }
    }
    return nullptr;
}

static CoopBossFight GetPhantomGanonFight(BossGanondrof* phantomGanon) {
    if (phantomGanon == nullptr || phantomGanon->actionFunc == BossGanondrof_Death) {
        return COOP_BOSS_DEFEATED;
    }
    return phantomGanon->actionFunc == BossGanondrof_Intro ? COOP_BOSS_WAITING : COOP_BOSS_FIGHTING;
}

// His fakes and horse go by him
static CoopBossFight GetPhantomGanonPartFight(Actor* actor) {
    if (actor->id == ACTOR_BOSS_GANONDROF && actor->params == GND_REAL_BOSS) {
        return GetPhantomGanonFight((BossGanondrof*)actor);
    }
    return GetPhantomGanonFight(FindPhantomGanon());
}

// The same as his own death in BossGanondrof_CollisionCheck
static void DefeatPhantomGanon(Actor* actor) {
    if (actor->params != GND_REAL_BOSS) {
        return;
    }
    actor->colChkInfo.health = 0;
    BossGanondrof_SetupDeath((BossGanondrof*)actor, gPlayState);
    Enemy_StartFinishingBlow(gPlayState, actor);
    GameInteractor_ExecuteOnBossDefeat(actor);
}

// Ganondorf. His light balls are the same actor (see the shots below).
static const BossGanonActionFunc sGanondorfActions[] = {
    BossGanon_Wait,  BossGanon_ChargeLightBall, BossGanon_PlayTennis, BossGanon_PoundFloor, BossGanon_ChargeBigMagic,
    BossGanon_Block, BossGanon_HitByLightBall,  BossGanon_Vulnerable, BossGanon_Damaged,
};

static CoopBossFight GetGanondorfFight(Actor* actor) {
    BossGanonActionFunc actionFunc = ((BossGanon*)actor)->actionFunc;
    if (actionFunc == BossGanon_IntroCutscene) {
        return COOP_BOSS_WAITING;
    }
    return actionFunc == BossGanon_DeathAndTowerCutscene ? COOP_BOSS_DEFEATED : COOP_BOSS_FIGHTING;
}

// The same as his own death in BossGanon_UpdateDamage
static void DefeatGanondorf(Actor* actor) {
    BossGanon* ganondorf = (BossGanon*)actor;
    actor->colChkInfo.health = 0;
    BossGanon_SetupDeathCutscene(ganondorf, gPlayState);
    Audio_PlayActorSound2(actor, NA_SE_EN_GANON_DEAD);
    Audio_PlayActorSound2(actor, NA_SE_EN_GANON_DD_THUNDER);
    static Vec3f sZeroVec = { 0.0f, 0.0f, 0.0f };
    Sfx_PlaySfxAtPos(&sZeroVec, NA_SE_EN_LAST_DAMAGE);
    Audio_QueueSeqCmd(0x100100FF);
    ganondorf->screenFlashTimer = 4;
    GameInteractor_ExecuteOnBossDefeat(actor);
}

// Twinrova: Kotake and Koume, who are beaten by reflecting one's beam onto the other, then the two merged. All three
// are the same actor (and so are the blasts, see the shots below).
#define TW_KOTAKE 0
#define TW_KOUME 1
#define TW_TWINROVA 2
#define TW_FIRE_BLAST 0x64
#define TW_ICE_BLAST 0x66
#define TWINROVA_SISTER_HIT 1 // By the other's beam, reflected by a teammate's Mirror Shield
#define TWINROVA_STUNNED 2    // By a teammate's charged Mirror Shield blast

static const BossTwActionFunc sTwinrovaActions[] = {
    BossTw_TurnToPlayer,
    BossTw_FlyTo,
    BossTw_ShootBeam,
    BossTw_FinishBeamShoot,
    BossTw_HitByBeam,
    BossTw_Laugh,
    BossTw_Spin,
    BossTw_Wait,
    BossTw_TwinrovaFly,
    BossTw_TwinrovaArriveAtTarget,
    BossTw_TwinrovaChargeBlast,
    BossTw_TwinrovaShootBlast,
    BossTw_TwinrovaDoneBlastShoot,
    BossTw_TwinrovaStun,
    BossTw_TwinrovaSpin,
    BossTw_TwinrovaLaugh,
    BossTw_TwinrovaGetUp,
};

static BossTw* FindTwinrova() {
    for (Actor* actor = gPlayState->actorCtx.actorLists[ACTORCAT_BOSS].head; actor != NULL; actor = actor->next) {
        if (actor->id == ACTOR_BOSS_TW && actor->params == TW_TWINROVA && actor->update != NULL) {
            return (BossTw*)actor;
        }
    }
    return nullptr;
}

// The sisters go by Twinrova (who waits out of sight while they fight). Merging them is a cutscene, played by every
// player.
static CoopBossFight GetTwinrovaFight(Actor* actor) {
    BossTw* twinrova = FindTwinrova();
    if (twinrova == nullptr || twinrova->actionFunc == BossTw_TwinrovaDeathCS) {
        return COOP_BOSS_DEFEATED;
    }
    BossTwActionFunc actionFunc = twinrova->actionFunc;
    bool isCutscene = actionFunc == BossTw_TwinrovaIntroCS || actionFunc == BossTw_TwinrovaMergeCS;
    return isCutscene ? COOP_BOSS_WAITING : COOP_BOSS_FIGHTING;
}

static u8 GetTwinrovaLocalEvent(Actor* actor, u16 phaseBefore) {
    BossTwActionFunc before = GetPhaseAction(sTwinrovaActions, phaseBefore);
    BossTwActionFunc now = ((BossTw*)actor)->actionFunc;
    if (actor->params != TW_TWINROVA && now == BossTw_HitByBeam && before != BossTw_HitByBeam) {
        return TWINROVA_SISTER_HIT;
    }
    if (actor->params == TW_TWINROVA && now == BossTw_TwinrovaStun && before != BossTw_TwinrovaStun) {
        return TWINROVA_STUNNED;
    }
    return 0;
}

// The same as what the reflected beam (BossTw_ShootBeam) and the charged blast (BossTw_UpdateEffects) do
static void ApplyTwinrovaEvent(Actor* actor, u8 event, Vec3f pos) {
    BossTw* part = (BossTw*)actor;
    if (event == TWINROVA_SISTER_HIT && actor->params != TW_TWINROVA && part->actionFunc != BossTw_HitByBeam) {
        BossTw_SetupHitByBeam(part, gPlayState);
        Audio_PlayActorSound2(actor, NA_SE_EN_TWINROBA_DAMAGE_VOICE);
        actor->colChkInfo.health++;
    } else if (event == TWINROVA_STUNNED && actor->params == TW_TWINROVA && part->actionFunc != BossTw_TwinrovaStun) {
        part->twinrovaStun = 1;
    }
}

// The same as her own death in BossTw_TwinrovaDamage
static void DefeatTwinrova(Actor* actor) {
    if (actor->params != TW_TWINROVA) {
        return;
    }
    actor->colChkInfo.health = 0;
    BossTw_TwinrovaSetupDeathCS((BossTw*)actor, gPlayState);
    Enemy_StartFinishingBlow(gPlayState, actor);
    Audio_PlayActorSound2(actor, NA_SE_EN_TWINROBA_YOUNG_DEAD);
    GameInteractor_ExecuteOnBossDefeat(actor);
}

// Barinade: her body, the supports holding her to the ceiling (and their stumps once cut), the zappers and the jellies
// (Bari) around her are all the same actor. How far the fight is (supports cut, jellies killed, hits on her body) is
// kept outside of them, and sent along with her body. The jellies she spawns during the fight, and the stumps, come
// from the authority's game.
static const BossVaActionFunc sBarinadeActions[] = {
    BossVa_BodyPhase1,    BossVa_BodyPhase2, BossVa_BodyPhase3,       BossVa_BodyPhase4,       BossVa_SupportAttached,
    BossVa_SupportCut,    BossVa_Stump,      BossVa_ZapperAttack,     BossVa_ZapperDamaged,    BossVa_ZapperDeath,
    BossVa_ZapperEnraged, BossVa_ZapperHold, BossVa_BariPhase2Attack, BossVa_BariPhase3Attack, BossVa_BariPhase3Stunned,
    BossVa_BariDeath,
};

static BossVa* FindBarinade() {
    for (Actor* actor = gPlayState->actorCtx.actorLists[ACTORCAT_BOSS].head; actor != NULL; actor = actor->next) {
        if (actor->id == ACTOR_BOSS_VA && actor->params == BOSSVA_BODY && actor->update != NULL) {
            return (BossVa*)actor;
        }
    }
    return nullptr;
}

// Her parts go by her body
static CoopBossFight GetBarinadeFight(Actor* actor) {
    BossVa* body = actor->params == BOSSVA_BODY ? (BossVa*)actor : FindBarinade();
    if (body == nullptr || body->actionFunc == BossVa_BodyDeath) {
        return COOP_BOSS_DEFEATED;
    }
    return body->actionFunc == BossVa_BodyIntro ? COOP_BOSS_WAITING : COOP_BOSS_FIGHTING;
}

static std::vector<u8> GetBarinadeExtraState(Actor* actor) {
    std::vector<u8> state;
    for (s32 i = 0; i < gBossVaCoopStateCount && actor->params == BOSSVA_BODY; i++) {
        u8* data = (u8*)gBossVaCoopState[i];
        state.insert(state.end(), data, data + gBossVaCoopStateSizes[i]);
    }
    return state;
}

static void SetBarinadeExtraState(Actor* actor, const std::vector<u8>& state) {
    size_t offset = 0;
    for (s32 i = 0; i < gBossVaCoopStateCount; i++) {
        if (offset + gBossVaCoopStateSizes[i] <= state.size()) {
            memcpy(gBossVaCoopState[i], state.data() + offset, gBossVaCoopStateSizes[i]);
        }
        offset += gBossVaCoopStateSizes[i];
    }
}

// A jelly gone from the authority's game was killed there, the other parts stay
static bool OnBarinadePartGone(Actor* actor) {
    if (actor->params >= BOSSVA_BARI_UPPER_1 && actor->params <= BOSSVA_BARI_LOWER_5) {
        BossVa_SetupBariDeath((BossVa*)actor);
    }
    return true;
}

// The same as her own death in BossVa_BodyPhase4
static void DefeatBarinade(Actor* actor) {
    if (actor->params != BOSSVA_BODY) {
        return;
    }
    BossVa_SetupBodyDeath((BossVa*)actor, gPlayState);
    Enemy_StartFinishingBlow(gPlayState, actor);
    GameInteractor_ExecuteOnBossDefeat(actor);
}

// Ganon. Partway through, he's downed and every player gets their Master Sword back in their own game, which is a
// cutscene for each of them (COOP_BOSS_CUTSCENE).
static const BossGanon2ActionFunc sGanonActions[] = {
    func_808FFDB0, func_808FFEBC, func_808FFFE0, func_80900104, func_8090026C, func_80900344, func_80900650,
};

static CoopBossFight GetGanonFight(Actor* actor) {
    BossGanon2ActionFunc actionFunc = ((BossGanon2*)actor)->actionFunc;
    if (actionFunc == func_808FD5F4) {
        return COOP_BOSS_WAITING;
    }
    if (actionFunc == func_80900890) {
        return COOP_BOSS_CUTSCENE;
    }
    return actionFunc == func_8090120C ? COOP_BOSS_DEFEATED : COOP_BOSS_FIGHTING;
}

// The same as his own death in BossGanon2_CollisionCheck
static void DefeatGanon(Actor* actor) {
    actor->colChkInfo.health = 0;
    func_80901020((BossGanon2*)actor, gPlayState);
}

// Volvagia flying around (BossFd), which keeps her health, and her head in the holes (BossFd2), which is where she's
// beaten. They hand the fight over to each other, and their actions are sent as the phase.
static const BossFdActionFunc sVolvagiaActions[] = { BossFd_Fly, BossFd_Wait };
static const BossFd2ActionFunc sVolvagiaHoleActions[] = {
    BossFd2_Wait,        BossFd2_Emerge,    BossFd2_Idle,       BossFd2_Burrow,
    BossFd2_BreatheFire, BossFd2_ClawSwipe, BossFd2_Vulnerable, BossFd2_Damaged,
};

static CoopBossFight GetVolvagiaFight(Actor* actor) {
    BossFd* volvagia = (BossFd*)actor;
    if (volvagia->introState != BFD_CS_NONE) {
        return COOP_BOSS_WAITING;
    }
    // Her death is played out by her head (see DefeatVolvagia), then she flies off to burn up
    if (actor->colChkInfo.health == 0 || volvagia->work[BFD_ACTION_STATE] >= BOSSFD_DEATH_START) {
        return COOP_BOSS_DEFEATED;
    }
    return COOP_BOSS_FIGHTING;
}

static CoopBossFight GetVolvagiaHoleFight(Actor* actor) {
    BossFd2* head = (BossFd2*)actor;
    if (head->actionFunc == BossFd2_Death) {
        return COOP_BOSS_DEFEATED;
    }
    BossFd* volvagia = (BossFd*)actor->parent;
    return (volvagia == nullptr || volvagia->introState != BFD_CS_NONE) ? COOP_BOSS_WAITING : COOP_BOSS_FIGHTING;
}

// The same as her own death in BossFd2_CollisionCheck
static void DefeatVolvagia(Actor* actor) {
    BossFd2* head = (BossFd2*)actor;
    if (actor->parent != nullptr) {
        actor->parent->colChkInfo.health = 0;
    }
    BossFd2_SetupDeath(head, gPlayState);
    head->work[FD2_DAMAGE_FLASH_TIMER] = 10;
    head->work[FD2_INVINC_TIMER] = 30000;
    Audio_QueueSeqCmd(0x1 << 28 | SEQ_PLAYER_BGM_MAIN << 24 | 0x100FF);
    Audio_PlayActorSound2(actor, NA_SE_EN_VALVAISA_DEAD);
    Enemy_StartFinishingBlow(gPlayState, actor);
    GameInteractor_ExecuteOnBossDefeat(actor);
}

// Bongo Bongo's head and his two hands, which are the same actor. The head, which keeps his health, is beaten once both
// hands are. A hand's phase is its action and, in the high byte, its state that the head and other hand go by.
static const BossSstActionFunc sBongoHeadActions[] = {
    BossSst_HeadNeutral, BossSst_HeadWait,       BossSst_HeadDamagedHand, BossSst_HeadReadyCharge,
    BossSst_HeadCharge,  BossSst_HeadEndCharge,  BossSst_HeadFrozenHand,  BossSst_HeadUnfreezeHand,
    BossSst_HeadStunned, BossSst_HeadVulnerable, BossSst_HeadDamage,      BossSst_HeadRecover,
};
static const BossSstActionFunc sBongoHandActions[] = {
    BossSst_HandWait,        BossSst_HandDownbeat,  BossSst_HandOffbeat,       BossSst_HandDownbeatEnd,
    BossSst_HandOffbeatEnd,  BossSst_HandReadySlam, BossSst_HandSlam,          BossSst_HandEndSlam,
    BossSst_HandReadySweep,  BossSst_HandSweep,     BossSst_HandReadyPunch,    BossSst_HandPunch,
    BossSst_HandReadyClap,   BossSst_HandClap,      BossSst_HandEndClap,       BossSst_HandReadyGrab,
    BossSst_HandGrab,        BossSst_HandCrush,     BossSst_HandEndCrush,      BossSst_HandSwing,
    BossSst_HandRetreat,     BossSst_HandReel,      BossSst_HandReadyShake,    BossSst_HandShake,
    BossSst_HandReadyCharge, BossSst_HandFrozen,    BossSst_HandReadyBreakIce, BossSst_HandBreakIce,
    BossSst_HandStunned,     BossSst_HandDamage,    BossSst_HandRecover,
};

static BossSst* FindBongoPart(s16 params) {
    for (Actor* actor = gPlayState->actorCtx.actorLists[ACTORCAT_BOSS].head; actor != NULL; actor = actor->next) {
        if (actor->id == ACTOR_BOSS_SST && actor->params == params && actor->update != NULL) {
            return (BossSst*)actor;
        }
    }
    return nullptr;
}

static CoopBossFight GetBongoHeadFight(Actor* actor) {
    BossSstActionFunc actionFunc = ((BossSst*)actor)->actionFunc;
    if (actionFunc == BossSst_HeadLurk || actionFunc == BossSst_HeadIntro) {
        return COOP_BOSS_WAITING;
    }
    for (BossSstActionFunc fightAction : sBongoHeadActions) {
        if (actionFunc == fightAction) {
            return COOP_BOSS_FIGHTING;
        }
    }
    return COOP_BOSS_DEFEATED;
}

// The hands go by the head: they're part of the fight while it is
static CoopBossFight GetBongoFight(Actor* actor) {
    if (actor->params == BONGO_HEAD) {
        return GetBongoHeadFight(actor);
    }
    BossSst* head = FindBongoPart(BONGO_HEAD);
    return head != nullptr ? GetBongoHeadFight(&head->actor) : COOP_BOSS_DEFEATED;
}

static u16 GetBongoPhase(Actor* actor) {
    BossSst* part = (BossSst*)actor;
    if (actor->params == BONGO_HEAD) {
        return GetActionPhase(sBongoHeadActions, part->actionFunc);
    }
    return GetActionPhase(sBongoHandActions, part->actionFunc) | (gBossSstHandState[actor->params] << 8);
}

static void SetBongoPhase(Actor* actor, u16 phase) {
    BossSst* part = (BossSst*)actor;
    if (actor->params == BONGO_HEAD) {
        part->actionFunc = GetPhaseAction(sBongoHeadActions, phase);
    } else {
        part->actionFunc = GetPhaseAction(sBongoHandActions, phase & 0xFF);
        gBossSstHandState[actor->params] = phase >> 8;
    }
}

// The same as his own death in BossSst_HeadCollisionCheck
static void DefeatBongo(Actor* actor) {
    if (actor->params != BONGO_HEAD) {
        return;
    }

    BossSst* hands[] = { FindBongoPart(BONGO_LEFT_HAND), FindBongoPart(BONGO_RIGHT_HAND) };
    for (BossSst* hand : hands) {
        if (hand != nullptr) {
            BossSst_HandReleasePlayer(hand, gPlayState, true);
        }
    }
    actor->colChkInfo.health = 0;
    Enemy_StartFinishingBlow(gPlayState, actor);
    BossSst_HeadSetupDeath((BossSst*)actor, gPlayState);
    GameInteractor_ExecuteOnBossDefeat(actor);
    for (BossSst* hand : hands) {
        if (hand != nullptr) {
            BossSst_HandSetupDamage(hand);
        }
    }
}

static const std::vector<CoopEnemyAdapter> sCoopEnemyAdapters = {
    // Armos (not the statues you push around)
    { ACTOR_EN_AM,
      true,
      offsetof(EnAm, skelAnime),
      { offsetof(EnAm, hurtCollider) },
      [](Actor* actor) { return actor->params != ARMOS_STATUE; } },
    // Bubble
    { ACTOR_EN_BB, true, offsetof(EnBb, skelAnime), { offsetof(EnBb, collider) } },
    // Biri, also the ones a Bari splits into
    { ACTOR_EN_BILI,
      true,
      offsetof(EnBili, skelAnime),
      { offsetof(EnBili, collider) },
      nullptr,
      {},
      nullptr,
      nullptr,
      nullptr,
      COOP_PUPPET_SPAWN },
    // Torch Slug
    { ACTOR_EN_BW, true, offsetof(EnBw, skelAnime), { offsetof(EnBw, collider2) } },
    // Guay
    { ACTOR_EN_CROW, true, offsetof(EnCrow, skelAnime), { offsetof(EnCrow, collider) } },
    // Deku Baba
    { ACTOR_EN_DEKUBABA,
      true,
      offsetof(EnDekubaba, skelAnime),
      { offsetof(EnDekubaba, collider) },
      nullptr,
      { STATE_FIELD(EnDekubaba, stemSectionAngle) },
      GetDekuBabaPhase,
      SetDekuBabaPhase,
      OnDekuBabaAuthorityGone },
    // Mad Scrub
    { ACTOR_EN_DEKUNUTS, true, offsetof(EnDekunuts, skelAnime), { offsetof(EnDekunuts, collider) } },
    // Baby Dodongo
    { ACTOR_EN_DODOJR, true, offsetof(EnDodojr, skelAnime), { offsetof(EnDodojr, collider) } },
    // Dodongo (the hard parts of its body make swords bounce off)
    { ACTOR_EN_DODONGO,
      true,
      offsetof(EnDodongo, skelAnime),
      { offsetof(EnDodongo, colliderBody), offsetof(EnDodongo, colliderHard) } },
    // Stinger
    { ACTOR_EN_EIYER, true, offsetof(EnEiyer, skelanime), { offsetof(EnEiyer, collider) } },
    // Keese
    { ACTOR_EN_FIREFLY, true, offsetof(EnFirefly, skelAnime), { offsetof(EnFirefly, collider) } },
    // Floormaster, and the small ones it splits into (they grab the player they target)
    { ACTOR_EN_FLOORMAS,
      false,
      offsetof(EnFloormas, skelAnime),
      { offsetof(EnFloormas, collider) },
      nullptr,
      {},
      GetFloormasterPhase,
      SetFloormasterPhase },
    // Moblin (they grab the player they target)
    { ACTOR_EN_MB, false, offsetof(EnMb, skelAnime), { offsetof(EnMb, hitbox) } },
    // Withered Deku Baba
    { ACTOR_EN_KAREBABA, true, offsetof(EnKarebaba, skelAnime), { offsetof(EnKarebaba, bodyCollider) } },
    // Octorok
    { ACTOR_EN_OKUTA, true, offsetof(EnOkuta, skelAnime), { offsetof(EnOkuta, collider) } },
    // ReDead, Gibdo (their scream freezes and their grab holds the player they target)
    { ACTOR_EN_RD, false, offsetof(EnRd, skelAnime), { offsetof(EnRd, collider) } },
    // Like Like (it swallows the player it targets, and wobbles without a skeleton)
    { ACTOR_EN_RR, false, NO_SKELETON, { offsetof(EnRr, collider1), offsetof(EnRr, collider2) } },
    // Shell Blade
    { ACTOR_EN_SB, true, offsetof(EnSb, skelAnime), { offsetof(EnSb, collider) } },
    // Stalchild (only the ones placed in a room, not the ones that dig up at night)
    { ACTOR_EN_SKB, true, offsetof(EnSkb, skelAnime), { offsetof(EnSkb, collider) } },
    // Skulltula (its spin attack damages the player it targets directly)
    { ACTOR_EN_ST,
      false,
      offsetof(EnSt, skelAnime),
      { offsetof(EnSt, colCylinder), offsetof(EnSt, colCylinder) + sizeof(ColliderCylinder),
        offsetof(EnSt, colCylinder) + 2 * sizeof(ColliderCylinder) } },
    // Skullwalltula (not the Gold Skulltulas, which are the same actor)
    { ACTOR_EN_SW,
      true,
      offsetof(EnSw, skelAnime),
      { offsetof(EnSw, collider) },
      [](Actor* actor) { return (actor->params & 0xE000) == 0; } },
    // Tektite
    { ACTOR_EN_TITE, true, offsetof(EnTite, skelAnime), { offsetof(EnTite, collider) } },
    // Wallmaster (it grabs the player it targets)
    { ACTOR_EN_WALLMAS, false, offsetof(EnWallmas, skelAnime), { offsetof(EnWallmas, collider) } },
    // Wolfos
    { ACTOR_EN_WF,
      true,
      offsetof(EnWf, skelAnime),
      { offsetof(EnWf, colliderCylinderBody), offsetof(EnWf, colliderCylinderTail), offsetof(EnWf, colliderSpheres) } },
    // Lizalfos, Dinolfos (they fade in when they drop from the ceiling)
    { ACTOR_EN_ZF,
      true,
      offsetof(EnZf, skelAnime),
      { offsetof(EnZf, bodyCollider) },
      nullptr,
      { STATE_FIELD(EnZf, alpha) } },
    // Gohma Larva, and the egg it hatches from (also the ones Queen Gohma lays)
    { ACTOR_EN_GOMA,
      true,
      offsetof(EnGoma, skelanime),
      { offsetof(EnGoma, colCyl2) },
      [](Actor* actor) { return actor->params < 10; }, // Not the debris of a hatching egg, or parts of Queen Gohma
      { STATE_RANGE(EnGoma, slopePitch, playerDetectionTimer), STATE_RANGE(EnGoma, eggScale, unk_2F4) },
      nullptr,
      nullptr,
      nullptr,
      COOP_PUPPET_SPAWN_AS_CHILD },
    // Queen Gohma
    { ACTOR_BOSS_GOMA,
      true,
      offsetof(BossGoma, skelanime),
      { offsetof(BossGoma, collider) },
      nullptr,
      { STATE_RANGE(BossGoma, frameCount, subCameraId), STATE_RANGE(BossGoma, decayingProgress, unk_1C8),
        STATE_RANGE(BossGoma, actionState, unk_1D8), STATE_RANGE(BossGoma, tailLimbsScale, unusedInitX),
        STATE_RANGE(BossGoma, mainEnvColor, currentAnimFrameCount), STATE_FIELD(BossGoma, eyeIrisScaleY) },
      GetBossGomaPhase,
      SetBossGomaPhase,
      nullptr,
      COOP_PUPPET_SPAWN_NONE,
      GetBossGomaFight,
      DefeatBossGoma },
    // King Dodongo. His fire breath is spawned for everyone like an enemy projectile.
    { ACTOR_BOSS_DODONGO,
      true,
      offsetof(BossDodongo, skelAnime),
      { offsetof(BossDodongo, collider) },
      nullptr,
      { STATE_RANGE(BossDodongo, unk_196, cutsceneCamera), STATE_RANGE(BossDodongo, unk_1B6, unk_25C) },
      [](Actor* actor) { return GetActionPhase(sKingDodongoActions, ((BossDodongo*)actor)->actionFunc); },
      [](Actor* actor, u16 phase) { ((BossDodongo*)actor)->actionFunc = GetPhaseAction(sKingDodongoActions, phase); },
      nullptr,
      COOP_PUPPET_SPAWN_NONE,
      GetKingDodongoFight,
      DefeatKingDodongo,
      GetKingDodongoLocalEvent,
      ApplyKingDodongoEvent,
      nullptr },
    // Phantom Ganon and his fakes. What's left out of his state is whether he's gone (killActor).
    { ACTOR_BOSS_GANONDROF,
      true,
      offsetof(BossGanondrof, skelAnime),
      { offsetof(BossGanondrof, colliderBody) },
      nullptr,
      { STATE_RANGE(BossGanondrof, work, killActor), STATE_RANGE(BossGanondrof, returnCount, spearTip) },
      [](Actor* actor) { return GetActionPhase(sPhantomGanonActions, ((BossGanondrof*)actor)->actionFunc); },
      [](Actor* actor, u16 phase) {
          ((BossGanondrof*)actor)->actionFunc = GetPhaseAction(sPhantomGanonActions, phase);
      },
      nullptr,
      COOP_PUPPET_SPAWN_AS_CHILD,
      GetPhantomGanonPartFight,
      DefeatPhantomGanon },
    // Phantom Ganon's horse (not the ones of his fakes, which ride out of the painting they're spawned for)
    { ACTOR_EN_FHG,
      true,
      offsetof(EnfHG, skin) + offsetof(Skin, skelAnime),
      {},
      [](Actor* actor) { return actor->params == GND_REAL_BOSS; },
      { STATE_RANGE(EnfHG, bossGndSignal, cameraEye), STATE_RANGE(EnfHG, hoofSfxPos, cameraSpeedMod),
        STATE_RANGE(EnfHG, gallopTimer, cutsceneState), STATE_RANGE(EnfHG, timers, unk_1E2),
        STATE_RANGE(EnfHG, warpColorFilterR, actionFunc) },
      [](Actor* actor) { return GetActionPhase(sPhantomHorseActions, ((EnfHG*)actor)->actionFunc); },
      [](Actor* actor, u16 phase) { ((EnfHG*)actor)->actionFunc = GetPhaseAction(sPhantomHorseActions, phase); },
      nullptr,
      COOP_PUPPET_SPAWN_NONE,
      GetPhantomGanonPartFight,
      nullptr },
    // Ganondorf (not his light balls, the same actor). His body part positions are left out of his state, his draw
    // works them out.
    { ACTOR_BOSS_GANON,
      true,
      offsetof(BossGanon, skelAnime),
      { offsetof(BossGanon, collider) },
      [](Actor* actor) { return actor->params < 0x64; },
      { STATE_RANGE(BossGanon, unk_198, envLightMode), STATE_RANGE(BossGanon, unk_1A2, unk_1F0),
        STATE_RANGE(BossGanon, handLightBallScale, unk_260), STATE_RANGE(BossGanon, unk_26C, unk_278),
        STATE_RANGE(BossGanon, unk_284, unk_2D4), STATE_RANGE(BossGanon, triforcePos, unk_2EC),
        STATE_FIELD(BossGanon, unk_508) },
      [](Actor* actor) { return GetActionPhase(sGanondorfActions, ((BossGanon*)actor)->actionFunc); },
      [](Actor* actor, u16 phase) { ((BossGanon*)actor)->actionFunc = GetPhaseAction(sGanondorfActions, phase); },
      nullptr,
      COOP_PUPPET_SPAWN_NONE,
      GetGanondorfFight,
      DefeatGanondorf },
    // Twinrova and the sisters. Their beams are worked out by every game for itself (at the player the authority's
    // goes after), so what's left out of their state is about the beam, and whether the charged blast stunned Twinrova.
    { ACTOR_BOSS_TW,
      true,
      offsetof(BossTw, skelAnime),
      { offsetof(BossTw, collider) },
      [](Actor* actor) { return actor->params < TW_FIRE_BLAST; },
      { STATE_RANGE(BossTw, work, fogR), STATE_FIELD(BossTw, csState1), STATE_FIELD(BossTw, targetPos),
        STATE_RANGE(BossTw, rotateSpeed, twinrovaStun), STATE_RANGE(BossTw, visible, skelAnime) },
      [](Actor* actor) { return GetActionPhase(sTwinrovaActions, ((BossTw*)actor)->actionFunc); },
      [](Actor* actor, u16 phase) { ((BossTw*)actor)->actionFunc = GetPhaseAction(sTwinrovaActions, phase); },
      nullptr,
      COOP_PUPPET_SPAWN_NONE,
      GetTwinrovaFight,
      DefeatTwinrova,
      GetTwinrovaLocalEvent,
      ApplyTwinrovaEvent,
      nullptr },
    // Barinade (not the door to her room, the same actor)
    { ACTOR_BOSS_VA,
      true,
      offsetof(BossVa, skelAnime),
      { offsetof(BossVa, colliderBody), offsetof(BossVa, colliderSph) },
      [](Actor* actor) { return actor->params != BOSSVA_DOOR; },
      { STATE_RANGE(BossVa, onCeiling, armTip), STATE_RANGE(BossVa, unk_1E4, effectPos) },
      [](Actor* actor) { return GetActionPhase(sBarinadeActions, ((BossVa*)actor)->actionFunc); },
      [](Actor* actor, u16 phase) { ((BossVa*)actor)->actionFunc = GetPhaseAction(sBarinadeActions, phase); },
      OnBarinadePartGone,
      COOP_PUPPET_SPAWN_AS_CHILD,
      GetBarinadeFight,
      DefeatBarinade,
      nullptr,
      nullptr,
      nullptr,
      GetBarinadeExtraState,
      SetBarinadeExtraState },
    // Ganon. What's left out of his state is about the cutscenes, and whether our player got the Master Sword back
    // (unk_334 to unk_337).
    { ACTOR_BOSS_GANON2,
      true,
      offsetof(BossGanon2, skelAnime),
      { offsetof(BossGanon2, unk_424) },
      nullptr,
      { STATE_RANGE(BossGanon2, unk_194, unk_1B8), STATE_RANGE(BossGanon2, unk_224, unk_22C),
        STATE_RANGE(BossGanon2, unk_310, unk_314), STATE_RANGE(BossGanon2, unk_316, unk_334),
        STATE_RANGE(BossGanon2, unk_338, unk_35C) },
      [](Actor* actor) { return GetActionPhase(sGanonActions, ((BossGanon2*)actor)->actionFunc); },
      [](Actor* actor, u16 phase) { ((BossGanon2*)actor)->actionFunc = GetPhaseAction(sGanonActions, phase); },
      nullptr,
      COOP_PUPPET_SPAWN_NONE,
      GetGanonFight,
      DefeatGanon,
      nullptr,
      nullptr,
      nullptr,
      nullptr,
      nullptr,
      [](Actor* actor) { func_80900818((BossGanon2*)actor, gPlayState); } },
    // Morpha's core and tentacles. What's left out of their state is about the player a tentacle holds.
    { ACTOR_BOSS_MO,
      true,
      NO_SKELETON,
      { offsetof(BossMo, coreCollider), offsetof(BossMo, tentCollider) },
      nullptr,
      { STATE_RANGE(BossMo, tent2KillTimer, linkToLeft), STATE_RANGE(BossMo, noBubbles, linkHitTimer),
        STATE_RANGE(BossMo, targetPos, grabPosRot), STATE_RANGE(BossMo, tentMaxAngle, tentPos) },
      nullptr,
      nullptr,
      nullptr,
      COOP_PUPPET_SPAWN_NONE,
      GetMorphaFight,
      DefeatMorpha,
      GetMorphaLocalEvent,
      ApplyMorphaEvent,
      SanitizeMorphaState },
    // Volvagia flying around. Her body trails behind her head, which needs every frame of her own movement, so the
    // ring buffer positions of the body and mane (work[3] and work[4]) are left out.
    { ACTOR_BOSS_FD,
      true,
      offsetof(BossFd, skelAnimeHead),
      { offsetof(BossFd, collider) },
      nullptr,
      { STATE_RANGE(BossFd, fireBreathTimer, fogMode), CoopStateField{ offsetof(BossFd, work), 3 * sizeof(s16) },
        CoopStateField{ offsetof(BossFd, work) + 5 * sizeof(s16), (BFD_SHORT_COUNT - 5) * sizeof(s16) },
        STATE_RANGE(BossFd, timers, targetPosition), STATE_RANGE(BossFd, targetPosition, bodySegsRot) },
      [](Actor* actor) { return GetActionPhase(sVolvagiaActions, ((BossFd*)actor)->actionFunc); },
      [](Actor* actor, u16 phase) { ((BossFd*)actor)->actionFunc = GetPhaseAction(sVolvagiaActions, phase); },
      nullptr,
      COOP_PUPPET_SPAWN_NONE,
      GetVolvagiaFight,
      nullptr },
    // Volvagia's head in the holes
    { ACTOR_BOSS_FD2,
      true,
      offsetof(BossFd2, skelAnime),
      { offsetof(BossFd2, collider) },
      nullptr,
      { STATE_RANGE(BossFd2, work, headPos) },
      [](Actor* actor) { return GetActionPhase(sVolvagiaHoleActions, ((BossFd2*)actor)->actionFunc); },
      [](Actor* actor, u16 phase) { ((BossFd2*)actor)->actionFunc = GetPhaseAction(sVolvagiaHoleActions, phase); },
      nullptr,
      COOP_PUPPET_SPAWN_NONE,
      GetVolvagiaHoleFight,
      DefeatVolvagia },
    // Bongo Bongo's head (hit by its cylinder) and hands (hit by their spheres)
    { ACTOR_BOSS_SST,
      true,
      offsetof(BossSst, skelAnime),
      { offsetof(BossSst, colliderCyl), offsetof(BossSst, colliderJntSph) },
      nullptr,
      { STATE_RANGE(BossSst, actionVar, jointTable), STATE_RANGE(BossSst, radius, colliderJntSph) },
      GetBongoPhase,
      SetBongoPhase,
      nullptr,
      COOP_PUPPET_SPAWN_NONE,
      GetBongoFight,
      DefeatBongo },
    // Stalfos
    { ACTOR_EN_TEST,
      true,
      offsetof(EnTest, skelAnime),
      { offsetof(EnTest, bodyCollider), offsetof(EnTest, shieldCollider) },
      nullptr,
      { STATE_FIELD(EnTest, unk_7C8), STATE_FIELD(EnTest, headRot), STATE_FIELD(EnTest, unk_7DE),
        STATE_FIELD(EnTest, iceTimer) } },
    // Beamos (its beam is drawn from where the authority's points)
    { ACTOR_EN_VM,
      true,
      offsetof(EnVm, skelAnime),
      { offsetof(EnVm, colliderCylinder) },
      nullptr,
      { STATE_FIELD(EnVm, unk_21C), STATE_RANGE(EnVm, beamPos1, unk_25E), STATE_FIELD(EnVm, unk_260) } },
    // Iron Knuckle (not Nabooru's, whose fight is part of a cutscene)
    { ACTOR_EN_IK,
      true,
      offsetof(EnIk, skelAnime),
      { offsetof(EnIk, bodyCollider), offsetof(EnIk, shieldCollider) },
      [](Actor* actor) { return (actor->params & 0xFF) != 0; },
      { STATE_RANGE(EnIk, unk_2F8, isBreakingProp), STATE_FIELD(EnIk, unk_2FE), STATE_FIELD(EnIk, drawMode) } },
    // Gerudo fighter (losing to one gets the player they target thrown in jail)
    { ACTOR_EN_GELDB,
      false,
      offsetof(EnGeldB, skelAnime),
      { offsetof(EnGeldB, bodyCollider), offsetof(EnGeldB, blockCollider) },
      nullptr,
      { STATE_FIELD(EnGeldB, action), STATE_FIELD(EnGeldB, timer), STATE_RANGE(EnGeldB, meleeWeaponState, blureIndex),
        STATE_FIELD(EnGeldB, headRot) } },
    // Freezard
    { ACTOR_EN_FZ,
      true,
      NO_SKELETON,
      { offsetof(EnFz, collider1), offsetof(EnFz, collider2) },
      nullptr,
      { STATE_FIELD(EnFz, envAlpha), STATE_RANGE(EnFz, state, unusedTimer2) } },
    // Spike
    { ACTOR_EN_NY,
      true,
      NO_SKELETON,
      { offsetof(EnNy, collider) },
      nullptr,
      { STATE_RANGE(EnNy, unk_1CA, hitPlayer), STATE_FIELD(EnNy, unk_1D4), STATE_FIELD(EnNy, unk_1D8),
        STATE_FIELD(EnNy, unk_1E0) } },
    // Dead Hand
    { ACTOR_EN_DH,
      true,
      offsetof(EnDh, skelAnime),
      { offsetof(EnDh, collider1), offsetof(EnDh, collider2) },
      nullptr,
      { STATE_FIELD(EnDh, drawDirtWave), STATE_FIELD(EnDh, alpha), STATE_FIELD(EnDh, dirtWaveSpread),
        STATE_FIELD(EnDh, dirtWaveHeight), STATE_FIELD(EnDh, dirtWaveAlpha) } },
    // Dead Hand's hands (they grab the player they target)
    { ACTOR_EN_DHA,
      false,
      offsetof(EnDha, skelAnime),
      { offsetof(EnDha, collider) },
      nullptr,
      { STATE_RANGE(EnDha, limbAngleX, handPos) } },
    // Flare Dancer
    { ACTOR_EN_FD,
      true,
      offsetof(EnFd, skelAnime),
      { offsetof(EnFd, collider) },
      nullptr,
      { STATE_FIELD(EnFd, coreActive), STATE_FIELD(EnFd, invincibilityTimer), STATE_FIELD(EnFd, fadeAlpha),
        STATE_FIELD(EnFd, corePos) },
      GetFlareDancerPhase,
      SetFlareDancerPhase },
    // The core a Flare Dancer leaves running around when its fire is put out
    { ACTOR_EN_FW,
      true,
      offsetof(EnFw, skelAnime),
      { offsetof(EnFw, collider) },
      nullptr,
      {},
      nullptr,
      nullptr,
      nullptr,
      COOP_PUPPET_SPAWN_AS_CHILD },
    // Poe
    { ACTOR_EN_POH,
      true,
      offsetof(EnPoh, skelAnime),
      { offsetof(EnPoh, colliderCyl), offsetof(EnPoh, colliderSph) },
      nullptr,
      { STATE_FIELD(EnPoh, unk_198), STATE_FIELD(EnPoh, visibilityTimer), STATE_FIELD(EnPoh, lightColor),
        STATE_FIELD(EnPoh, envColor) },
      GetPoePhase,
      SetPoePhase,
      OnPoeAuthorityGone },
    // Bari (it splits into Biris, see above)
    { ACTOR_EN_VALI,
      true,
      offsetof(EnVali, skelAnime),
      { offsetof(EnVali, bodyCollider) },
      nullptr,
      { STATE_FIELD(EnVali, lightningTimer), STATE_FIELD(EnVali, armScale) },
      GetBariPhase,
      SetBariPhase },
};

static const s16 sCoopMirroredObjects[] = {
    ACTOR_BG_DDAN_JD,         // Dodongo's Cavern rising platform
    ACTOR_BG_HAKA_SGAMI,      // Shadow Temple spinning scythe
    ACTOR_BG_HIDAN_FSLIFT,    // Fire Temple lift
    ACTOR_BG_HIDAN_RSEKIZOU,  // Fire Temple rotating flame statue
    ACTOR_BG_HIDAN_SYOKU,     // Fire Temple moving platform
    ACTOR_BG_JYA_LIFT,        // Spirit Temple elevator
    ACTOR_BG_MORI_ELEVATOR,   // Forest Temple elevator
    ACTOR_BG_MORI_RAKKATENJO, // Forest Temple falling ceiling
    ACTOR_OBJ_LIFT,           // Collapsing platform
};

#define OBJECT_CORRECTION_RATE 0.2f

#define PUPPET_TIMEOUT_FRAMES 20 // ~1 second without snapshots and the puppet's AI takes over again
// A weapon touching a puppet over several frames in a row is a single hit, like a sword swing. Touching it again after
// this many frames without contact is a new one.
#define PUPPET_HIT_CONTACT_GAP_FRAMES 2
// How long a hit on a puppet waits on the authority for its enemy to be hittable (e.g. when it arrives in the same
// frame as another hit)
#define HIT_RETRY_FRAMES 4
#define SNAPSHOT_INTERVAL_FRAMES 2
#define AUTHORITY_UNRESPONSIVE_TIME std::chrono::seconds(1)

struct CoopPuppet {
    u32 lastSnapshotFrame = 0;
    Vec3f targetPos;
    Vec3s shapeRot;
    s16 worldRotY;
    u8 health;
    Vec3f focusOffset;
    const char* animation = nullptr;
    f32 frame;
    f32 playSpeed;
    f32 endFrame;
    u8 mode;
    bool hasNewFrame = false;
    std::vector<std::pair<Actor*, u32>> recentAttackers; // Our weapons touching it, and the last frame they did
    bool hasColliderOffsets = false;
    std::vector<Vec3f> colliderOffsets; // Per hurt collider element, relative to the actor position
    std::vector<u8> state;              // The adapter's state fields, one after the other
    std::vector<u8> extraState;         // See getExtraState
    u16 phase = 0;
    bool isReleased = false; // Runs its own update for good (see onAuthorityGone)
    Vec3f scale;
    CoopBossFight bossFight = COOP_BOSS_WAITING;
    // Bosses only (see UpdateCoopPuppet)
    Vec3s worldRot;
    u32 drivenFrame = UINT32_MAX; // When it last ran its own update as a puppet
    bool isHeldLocally = false;   // See IsCoopHeldLocally
    uint32_t targetClientId = 0;  // Who the authority's goes after
    u16 phaseBefore = 0;          // Before its own update, for getLocalEvent
    u32 holdUntilFrame = 0;       // After a local event, until the authority's boss has caught up with it
    bool hasCorrected = false;
    Vec3f correctedPos; // Where it was left after the last correction, to tell how far it moved by itself since
    Vec3s correctedShapeRot;
    Vec3s correctedWorldRot;
};

struct CoopChildCount {
    s16 count;
};
static ObjectExtension::Register<CoopChildCount> CoopChildCountRegister;

// Actors our player spawned, directly or through something they spawned (see OnCoopActorSpawned)
struct CoopOwnedByUs {};
static ObjectExtension::Register<CoopOwnedByUs> CoopOwnedByUsRegister;

static ObjectExtension::Register<CoopPuppet> CoopPuppetRegister;

struct CoopMirroredObject {
    u32 lastSnapshotFrame;
    Vec3f targetPos;
    Vec3s targetRot;
    bool hasCorrected = false;
    Vec3f correctedPos; // Where it was left after the last correction, to tell how far it moved by itself since
    Vec3s correctedRot;
};
static ObjectExtension::Register<CoopMirroredObject> CoopMirroredObjectRegister;

struct CoopProjectile {
    bool isCopy; // Spawned from a teammate's packet, just for show
};
static ObjectExtension::Register<CoopProjectile> CoopProjectileRegister;

struct CoopEnemyTarget {
    uint32_t clientId; // 0 for our own player
};
static ObjectExtension::Register<CoopEnemyTarget> CoopEnemyTargetRegister;

#define TARGET_SWITCH_DISTANCE 100.0f // How much closer another player has to be to draw the enemy's attention

static const CoopEnemyAdapter* FindCoopEnemyAdapter(Actor* actor) {
    for (const CoopEnemyAdapter& adapter : sCoopEnemyAdapters) {
        if (adapter.actorId == actor->id) {
            return (adapter.isVariant == nullptr || adapter.isVariant(actor)) ? &adapter : nullptr;
        }
    }

    return nullptr;
}

static SkelAnime* GetSkelAnime(Actor* actor, const CoopEnemyAdapter* adapter) {
    if (adapter->skelAnimeOffset == NO_SKELETON) {
        return nullptr;
    }

    return (SkelAnime*)((uintptr_t)actor + adapter->skelAnimeOffset);
}

static Collider* GetHurtCollider(Actor* actor, const CoopEnemyAdapter* adapter, size_t index) {
    return (Collider*)((uintptr_t)actor + adapter->hurtColliderOffsets[index]);
}

// Animations are referenced by their resource path, which is the same for everyone. Animation_Change keeps the pointer,
// so received paths are kept alive here.
static const char* InternAnimationPath(const std::string& path) {
    static std::unordered_set<std::string> sAnimationPaths;
    return sAnimationPaths.insert(path).first->c_str();
}

static const char* GetAnimationPath(SkelAnime* skelAnime) {
    char* animation = (char*)skelAnime->animation;
    return (animation != nullptr && ResourceMgr_OTRSigCheck(animation)) ? animation : nullptr;
}

// Calls func on the position of every element of a hurt collider (cylinders have one, joint spheres one per sphere)
template <typename F> static void ForEachColliderElement(Collider* collider, F func) {
    if (collider->shape == COLSHAPE_CYLINDER) {
        ColliderCylinder* cylinder = (ColliderCylinder*)collider;
        func(&cylinder->info, &cylinder->dim.pos);
    } else if (collider->shape == COLSHAPE_JNTSPH) {
        ColliderJntSph* jntSph = (ColliderJntSph*)collider;
        for (s32 i = 0; i < jntSph->count; i++) {
            func(&jntSph->elements[i].info, &jntSph->elements[i].dim.worldSphere.center);
        }
    }
}

static bool IsMirroredType(Actor* actor) {
    return FindCoopEnemyAdapter(actor) != nullptr && (IsCoopEnemy(actor) || IsCoopBoss(actor));
}

// Bosses only during the fight, they run their intro and defeat for every player by themselves
static bool IsMirroredNow(Actor* actor) {
    const CoopEnemyAdapter* adapter = FindCoopEnemyAdapter(actor);
    return adapter != nullptr &&
           (adapter->getBossFight == nullptr || adapter->getBossFight(actor) == COOP_BOSS_FIGHTING);
}

bool Anchor::IsCoopMirroredEnemy(Actor* actor) {
    return IsCoopSyncActive() && IsMirroredType(actor);
}

// A mirrored enemy, or something one of them shot (in the game running them)
bool Anchor::IsCoopMirroredAttacker(Actor* actor) {
    if (actor == nullptr) {
        return false;
    }

    // Not a boss itself, its puppets hit everyone in their own game
    const CoopEnemyAdapter* adapter = FindCoopEnemyAdapter(actor);
    if (adapter != nullptr && adapter->getBossFight != nullptr) {
        return false;
    }

    const CoopProjectile* projectile = ObjectExtension::GetInstance().Get<CoopProjectile>(actor);
    return IsCoopMirroredEnemy(actor) || (projectile != nullptr && !projectile->isCopy && IsCoopSyncActive());
}

static bool IsCoopMirroredObject(Actor* actor) {
    for (s16 actorId : sCoopMirroredObjects) {
        if (actor->id == actorId) {
            return GetActorListIndex(actor) >= 0;
        }
    }

    return false;
}

// The authority is whoever was in the room first: a player entering a room where a fight is going on joins it, rather
// than everyone's enemies starting over from the newcomer's. Every player says in PLAYER_UPDATE whether they claim to
// be the authority of their room, and a claim is only made when nobody else in the room has one:
// - Right away when entering a room nobody else is in.
// - Otherwise after a short while, if none of the players in it claims it (they all just arrived, or are paused).
// Peers whose game stopped sending updates (paused, in a menu) are passed over, so enemies don't freeze for everyone
// else in the meantime. Coming back from a pause, we drop our claim, since someone else may have taken over and our
// enemies stood still. When two claims meet (like when entering a room together), the lowest client id keeps it.
#define AUTHORITY_CLAIM_DELAY_FRAMES 30

void Anchor::UpdateCoopRoomAuthority() {
    auto now = std::chrono::steady_clock::now();
    if (now - coopLastActiveTime > AUTHORITY_UNRESPONSIVE_TIME) {
        coopClaimsAuthority = false;
        coopRoomFrames = 0;
    }
    coopLastActiveTime = now;
    coopRoomFrames++;

    if (!IsCoopSyncActive()) {
        coopClaimsAuthority = false;
        coopRoomAuthority = ownClientId;
        return;
    }

    bool hasPeers = false;
    uint32_t authority = UINT32_MAX;
    for (uint32_t clientId : GetCoopPeers(true)) {
        AnchorClient& client = clients[clientId];
        if (now - client.lastPlayerUpdate >= AUTHORITY_UNRESPONSIVE_TIME) {
            continue;
        }

        hasPeers = true;
        if (client.claimsCoopAuthority) {
            authority = std::min(authority, clientId);
        }
    }

    // Without the room's boss, we have nothing to run for those who have it
    bool canClaim = !IsOwnBossLeftOut();
    if (canClaim && authority == UINT32_MAX && (!hasPeers || coopRoomFrames > AUTHORITY_CLAIM_DELAY_FRAMES)) {
        coopClaimsAuthority = true;
    }
    if (coopClaimsAuthority && canClaim) {
        authority = std::min(authority, ownClientId);
    }

    coopClaimsAuthority = authority == ownClientId;
    coopRoomAuthority = authority == UINT32_MAX ? 0 : authority;
}

// Without the soul of the scene's boss, our game left it out (see Randomizer_IsCurrentBossSoulMissing)
bool Anchor::IsOwnBossLeftOut() {
    return IS_RANDO && Randomizer_IsCurrentBossSoulMissing();
}

// 0 while nobody runs the room yet (we just entered it), then everyone's enemies run by themselves
uint32_t Anchor::GetCoopRoomAuthority() {
    return IsCoopSyncActive() ? coopRoomAuthority : ownClientId;
}

bool Anchor::IsCoopRoomAuthority() {
    return GetCoopRoomAuthority() == ownClientId;
}

// MARK: - Threat

/**
 * Enemies and bosses go after the most dangerous player in the room: whoever dealt them the most damage. The authority
 * keeps count, from the health its enemies lose, and who caused it: its own player, unless a teammate's hit on a
 * puppet, something a teammate did to a boss in their game (see getLocalEvent), or a shot a teammate sent back at it
 * (see the shots) came just before.
 *
 * A boss counts the damage to any of its parts together (Bongo Bongo's head for his hands, Volvagia flying and in the
 * holes). Twinrova's sisters count the hits they take up instead of losing health.
 */

#define THREAT_HIT_FRAMES 3    // A teammate's hit lands in the enemy's next update
#define THREAT_EVENT_FRAMES 60 // What a teammate did to a boss (King Dodongo chokes on a bomb ~2 seconds later)
#define THREAT_SHOT_FRAMES 150 // A shot a teammate sent back, until it hits the boss

struct CoopThreat {
    std::map<uint32_t, s32> damage; // By client id
    uint32_t lastAttacker = 0;
    u32 lastAttackUntil = 0;
};
static ObjectExtension::Register<CoopThreat> CoopThreatRegister;
static CoopThreat sCoopBossThreat; // One boss per room

void ResetCoopBossThreat() {
    sCoopBossThreat = {};
}

static CoopThreat* GetCoopThreat(Actor* actor) {
    if (IsCoopBoss(actor)) {
        return &sCoopBossThreat;
    }
    CoopThreat* threat = ObjectExtension::GetInstance().Get<CoopThreat>(actor);
    if (threat == nullptr) {
        ObjectExtension::GetInstance().Set<CoopThreat>(actor, CoopThreat{});
        threat = ObjectExtension::GetInstance().Get<CoopThreat>(actor);
    }
    return threat;
}

// A teammate did something to it that may cost it health in a moment
static void RecordCoopAttacker(Actor* actor, uint32_t clientId, u32 frames) {
    CoopThreat* threat = GetCoopThreat(actor);
    threat->lastAttacker = clientId;
    threat->lastAttackUntil = gPlayState->gameplayFrames + frames;
}

// Called in the authority's game when an enemy's health changed in its update
void RecordCoopDamage(Actor* actor, s32 healthBefore, s32 healthAfter) {
    s32 damage = healthBefore - healthAfter;
    if (actor->id == ACTOR_BOSS_TW && actor->params != TW_TWINROVA) {
        damage = -damage; // The sisters count up the hits they take
    }
    if (damage <= 0 || !Anchor::Instance->IsCoopRoomAuthority()) {
        return;
    }

    CoopThreat* threat = GetCoopThreat(actor);
    uint32_t attacker =
        gPlayState->gameplayFrames <= threat->lastAttackUntil ? threat->lastAttacker : Anchor::Instance->ownClientId;
    threat->damage[attacker] += damage;
}

// MARK: - Authority

void Anchor::SendCoopEnemySnapshots() {
    if (!IsCoopSyncActive() || gPlayState->gameplayFrames % SNAPSHOT_INTERVAL_FRAMES != 0) {
        return;
    }

    // Not while our game is paused, a teammate takes over then
    std::vector<uint32_t> peers = GetCoopPeers(true);
    if (peers.empty() || !IsCoopRoomAuthority() ||
        std::chrono::steady_clock::now() - coopLastActiveTime > AUTHORITY_UNRESPONSIVE_TIME) {
        return;
    }

    nlohmann::json enemies = nlohmann::json::array();
    // Background too, for Phantom Ganon's horse
    for (s32 category : { ACTORCAT_ENEMY, ACTORCAT_BOSS, ACTORCAT_BG }) {
        for (Actor* actor = gPlayState->actorCtx.actorLists[category].head; actor != NULL; actor = actor->next) {
            const CoopEnemyAdapter* adapter = FindCoopEnemyAdapter(actor);
            if (adapter == nullptr || actor->update == NULL || !IsMirroredType(actor)) {
                continue;
            }

            SkelAnime* skelAnime = GetSkelAnime(actor, adapter);
            const char* animation = skelAnime != nullptr ? GetAnimationPath(skelAnime) : nullptr;

            nlohmann::json enemy;
            enemy["id"] = GetCoopActorIdentity(actor);
            enemy["pos"] = actor->world.pos;
            enemy["rot"] = actor->shape.rot;
            enemy["rotY"] = actor->world.rot.y;
            enemy["health"] = GetCoopHealth(actor);
            enemy["focus"] = Vec3f{ actor->focus.pos.x - actor->world.pos.x, actor->focus.pos.y - actor->world.pos.y,
                                    actor->focus.pos.z - actor->world.pos.z };
            enemy["anim"] = animation != nullptr ? animation : "";
            enemy["frame"] = skelAnime != nullptr ? skelAnime->curFrame : 0.0f;
            enemy["speed"] = skelAnime != nullptr ? skelAnime->playSpeed : 0.0f;
            enemy["end"] = skelAnime != nullptr ? skelAnime->endFrame : 0.0f;
            enemy["mode"] = skelAnime != nullptr ? skelAnime->mode : 0;
            enemy["scale"] = actor->scale;
            if (adapter->puppetSpawn != COOP_PUPPET_SPAWN_NONE) {
                enemy["params"] = actor->params;
                if (actor->parent != nullptr && (IsCoopEnemy(actor->parent) || IsCoopBoss(actor->parent))) {
                    enemy["parent"] = GetCoopActorIdentity(actor->parent);
                }
                if (actor->parent == nullptr) {
                    enemy["noParent"] = true;
                }
            }
            if (!adapter->stateFields.empty()) {
                std::vector<u8> state;
                for (const CoopStateField& field : adapter->stateFields) {
                    u8* data = (u8*)actor + field.offset;
                    state.insert(state.end(), data, data + field.size);
                }
                enemy["state"] = state;
            }
            if (adapter->getPhase != nullptr) {
                enemy["phase"] = adapter->getPhase(actor);
            }
            if (adapter->getExtraState != nullptr) {
                enemy["extra"] = adapter->getExtraState(actor);
            }
            if (adapter->getBossFight != nullptr) {
                enemy["boss"] = (u8)adapter->getBossFight(actor);
                enemy["worldRot"] = actor->world.rot;
                const CoopEnemyTarget* target = ObjectExtension::GetInstance().Get<CoopEnemyTarget>(actor);
                enemy["target"] = (target != nullptr && target->clientId != 0) ? target->clientId : ownClientId;
            }
            enemies.push_back(enemy);
        }
    }

    nlohmann::json objects = nlohmann::json::array();
    for (s32 i = 0; i < ACTORCAT_MAX; i++) {
        for (Actor* actor = gPlayState->actorCtx.actorLists[i].head; actor != NULL; actor = actor->next) {
            if (actor->update != NULL && IsCoopMirroredObject(actor)) {
                objects.push_back({ { "id", GetCoopActorIdentity(actor) },
                                    { "pos", actor->world.pos },
                                    { "rot", actor->shape.rot } });
            }
        }
    }

    if (enemies.empty() && objects.empty()) {
        return;
    }

    nlohmann::json payload = PrepCoopPayload(COOP_ENEMY_SNAPSHOT);
    payload["enemies"] = enemies;
    payload["objects"] = objects;
    payload["quiet"] = true;

    for (uint32_t clientId : peers) {
        payload["targetClientId"] = clientId;
        SendJsonToRemote(payload);
    }
}

void Anchor::HandlePacket_CoopEnemyHit(nlohmann::json payload) {
    if (!IsCoopPayloadForThisScene(payload) || !IsCoopRoomAuthority()) {
        return;
    }

    coopPendingHits.push_back({ payload, 0 });
}

// Hits on teammates' puppets are applied at the end of the frame (after the incoming packets), one per collider and
// frame since a collider only holds one hit. A hit that can't land yet waits a few frames rather than being lost, like
// when two players hit the same enemy at once, or it arrives just as the enemy turns hittable again.
void Anchor::ApplyCoopPendingHits() {
    if (!IsCoopRoomAuthority()) {
        coopPendingHits.clear();
        return;
    }

    std::erase_if(coopPendingHits, [&](CoopPendingHit& hit) {
        return ApplyCoopEnemyHit(hit.payload) || ++hit.frames > HIT_RETRY_FRAMES;
    });
}

// Applies a hit on a teammate's puppet to the real enemy, the same way CollisionCheck_SetATvsAC does for a real hit.
// This runs at the end of the frame, after the enemy registered its colliders and before the next collision check,
// which then computes the damage from the enemy's own damage table. Returns false if it can't be hit right now.
bool Anchor::ApplyCoopEnemyHit(nlohmann::json& payload) {
    Actor* actor = FindCoopActor(payload.at("actor").get<CoopActorIdentity>());
    const CoopEnemyAdapter* adapter = actor != nullptr ? FindCoopEnemyAdapter(actor) : nullptr;
    size_t colliderIndex = payload.at("collider").get<size_t>();
    if (adapter == nullptr || colliderIndex >= adapter->hurtColliderOffsets.size()) {
        return true; // Gone, nothing to wait for
    }

    // Only if it can be hit right now (e.g. not while it's still reeling from the previous hit), and isn't already
    // being hit this frame
    Collider* collider = GetHurtCollider(actor, adapter, colliderIndex);
    bool isHittable = false;
    for (s32 i = 0; i < gPlayState->colChkCtx.colACCount; i++) {
        isHittable |= gPlayState->colChkCtx.colAC[i] == collider;
    }
    if (!isHittable || (collider->acFlags & AC_HIT)) {
        return false;
    }

    ColliderInfo* hitInfo = nullptr;
    s32 elementIndex = payload.at("element").get<s32>();
    s32 currentIndex = 0;
    ForEachColliderElement(collider, [&](ColliderInfo* info, Vec3s* pos) {
        if (currentIndex++ == elementIndex) {
            hitInfo = info;
        }
    });
    if (hitInfo == nullptr) {
        return true;
    }

    // The game keeps pointers to the attacker's collider until the enemy has handled the hit, so they need to outlive
    // this function. A few slots are enough since hits are applied within a frame.
    static Collider sAttackColliders[8];
    static ColliderInfo sAttackInfos[8];
    static size_t sNextAttackSlot = 0;
    Collider* attack = &sAttackColliders[sNextAttackSlot];
    ColliderInfo* attackInfo = &sAttackInfos[sNextAttackSlot];
    sNextAttackSlot = (sNextAttackSlot + 1) % ARRAY_COUNT(sAttackColliders);

    uint32_t clientId = payload.at("clientId").get<uint32_t>();
    Actor* attacker = &GET_PLAYER(gPlayState)->actor;
    if (clients.contains(clientId) && clients[clientId].player != nullptr) {
        attacker = &clients[clientId].player->actor;
    }

    *attack = {};
    attack->actor = attacker;
    attack->atFlags = AT_ON | AT_TYPE_PLAYER;
    *attackInfo = {};
    attackInfo->toucherFlags = TOUCH_ON;
    attackInfo->toucher.dmgFlags = payload.at("dmgFlags").get<u32>();
    attackInfo->toucher.effect = payload.at("effect").get<u8>();
    attackInfo->toucher.damage = payload.at("damage").get<u8>();

    Vec3s hitPos = payload.at("hitPos").get<Vec3s>();
    collider->acFlags |= AC_HIT;
    collider->ac = attacker;
    hitInfo->acHit = attack;
    hitInfo->acHitInfo = attackInfo;
    hitInfo->bumperFlags |= BUMP_HIT;
    hitInfo->bumper.hitPos = hitPos;
    actor->colChkInfo.acHitEffect = attackInfo->toucher.effect;
    RecordCoopAttacker(actor, clientId, THREAT_HIT_FRAMES);
    return true;
}

// MARK: - Puppets

void Anchor::HandlePacket_CoopEnemySnapshot(nlohmann::json payload) {
    if (!IsCoopPayloadForThisScene(payload) || payload.at("roomNum").get<s8>() != gPlayState->roomCtx.curRoom.num ||
        IsCoopRoomAuthority()) {
        return;
    }

    // Only from the authority, or from whoever claims it while we're still finding out who that is
    uint32_t authority = GetCoopRoomAuthority();
    if (authority != 0 && payload.at("clientId").get<uint32_t>() != authority) {
        return;
    }

    std::set<Actor*> mirroredActors;
    for (auto& enemy : payload.at("enemies")) {
        CoopActorIdentity identity = enemy.at("id").get<CoopActorIdentity>();
        Actor* actor = FindCoopActor(identity);
        if (actor == nullptr) {
            actor = SpawnCoopPuppet(identity, enemy);
        }
        const CoopEnemyAdapter* adapter = actor != nullptr ? FindCoopEnemyAdapter(actor) : nullptr;
        if (adapter == nullptr) {
            continue;
        }
        mirroredActors.insert(actor);

        CoopPuppet* puppet = ObjectExtension::GetInstance().Get<CoopPuppet>(actor);
        if (puppet == nullptr) {
            ObjectExtension::GetInstance().Set<CoopPuppet>(actor, CoopPuppet{});
            puppet = ObjectExtension::GetInstance().Get<CoopPuppet>(actor);
        }

        std::string animation = enemy.at("anim").get<std::string>();
        puppet->lastSnapshotFrame = gPlayState->gameplayFrames;
        puppet->targetPos = enemy.at("pos").get<Vec3f>();
        puppet->shapeRot = enemy.at("rot").get<Vec3s>();
        puppet->worldRotY = enemy.at("rotY").get<s16>();
        puppet->health = enemy.at("health").get<u8>();
        puppet->focusOffset = enemy.at("focus").get<Vec3f>();
        puppet->animation = animation.empty() ? nullptr : InternAnimationPath(animation);
        puppet->frame = enemy.at("frame").get<f32>();
        puppet->playSpeed = enemy.at("speed").get<f32>();
        puppet->endFrame = enemy.at("end").get<f32>();
        puppet->mode = enemy.at("mode").get<u8>();
        puppet->hasNewFrame = true;
        puppet->state = enemy.value("state", std::vector<u8>{});
        puppet->phase = enemy.value("phase", (u16)0);
        puppet->extraState = enemy.value("extra", std::vector<u8>{});
        puppet->scale = enemy.value("scale", actor->scale);
        puppet->bossFight = (CoopBossFight)enemy.value("boss", (u8)COOP_BOSS_WAITING);
        puppet->worldRot = enemy.value("worldRot", actor->world.rot);
        puppet->targetClientId = enemy.value("target", (uint32_t)0);
    }

    // Puppets whose enemy is gone from the authority's game without dying (like a small Floormaster merging back into
    // the big one) are removed too, rather than going on with their own AI. Boss parts only if they say what happens
    // then (a Barinade jelly killed by a teammate), the others stay for their own defeat.
    for (s32 category : { ACTORCAT_ENEMY, ACTORCAT_BOSS }) {
        for (Actor* actor = gPlayState->actorCtx.actorLists[category].head; actor != NULL; actor = actor->next) {
            CoopPuppet* puppet = ObjectExtension::GetInstance().Get<CoopPuppet>(actor);
            if (puppet == nullptr || puppet->isReleased || actor->update == NULL || mirroredActors.contains(actor) ||
                (actor->room != gPlayState->roomCtx.curRoom.num && actor->room != -1) ||
                gPlayState->gameplayFrames - puppet->lastSnapshotFrame > PUPPET_TIMEOUT_FRAMES) {
                continue;
            }

            const CoopEnemyAdapter* adapter = FindCoopEnemyAdapter(actor);
            if (category == ACTORCAT_BOSS && (adapter == nullptr || adapter->onAuthorityGone == nullptr ||
                                              !IsMirroredNow(actor) || puppet->bossFight != COOP_BOSS_FIGHTING)) {
                continue;
            }
            if (adapter != nullptr && adapter->onAuthorityGone != nullptr && adapter->onAuthorityGone(actor)) {
                puppet->isReleased = true;
            } else {
                Actor_Kill(actor);
            }
        }
    }

    if (!payload.contains("objects")) {
        return;
    }

    for (auto& object : payload.at("objects")) {
        Actor* actor = FindCoopActor(object.at("id").get<CoopActorIdentity>());
        if (actor == nullptr || !IsCoopMirroredObject(actor)) {
            continue;
        }

        CoopMirroredObject* mirror = ObjectExtension::GetInstance().Get<CoopMirroredObject>(actor);
        if (mirror == nullptr) {
            ObjectExtension::GetInstance().Set<CoopMirroredObject>(actor, CoopMirroredObject{});
            mirror = ObjectExtension::GetInstance().Get<CoopMirroredObject>(actor);
        }
        mirror->lastSnapshotFrame = gPlayState->gameplayFrames;
        mirror->targetPos = object.at("pos").get<Vec3f>();
        mirror->targetRot = object.at("rot").get<Vec3s>();
    }
}

// An enemy the authority has, spawned during the fight by another one, that we don't have yet
Actor* Anchor::SpawnCoopPuppet(const CoopActorIdentity& identity, nlohmann::json& enemy) {
    // Parts of a boss our game left out
    if (identity.childIndex == 0 || !enemy.contains("params") || IsOwnBossLeftOut()) {
        return nullptr;
    }

    const CoopEnemyAdapter* adapter = nullptr;
    for (const CoopEnemyAdapter& candidate : sCoopEnemyAdapters) {
        if (candidate.actorId == identity.actorId) {
            adapter = &candidate;
        }
    }
    if (adapter == nullptr || adapter->puppetSpawn == COOP_PUPPET_SPAWN_NONE) {
        return nullptr;
    }

    Vec3f pos = enemy.at("pos").get<Vec3f>();
    Vec3s rot = enemy.at("rot").get<Vec3s>();
    s16 params = enemy.at("params").get<s16>();
    Actor* actor;
    // As a child, unless the authority's has no parent (Barinade's jellies have her, the stumps of her supports don't)
    if (adapter->puppetSpawn == COOP_PUPPET_SPAWN_AS_CHILD && !enemy.value("noParent", false)) {
        Actor* parent = enemy.contains("parent") ? FindCoopActor(enemy["parent"].get<CoopActorIdentity>()) : nullptr;
        if (parent == nullptr) {
            return nullptr;
        }
        actor = Actor_SpawnAsChild(&gPlayState->actorCtx, parent, gPlayState, identity.actorId, pos.x, pos.y, pos.z,
                                   rot.x, rot.y, rot.z, params);
    } else {
        actor = Actor_Spawn(&gPlayState->actorCtx, gPlayState, identity.actorId, pos.x, pos.y, pos.z, rot.x, rot.y,
                            rot.z, params);
    }

    if (actor != nullptr) {
        SetCoopChildIdentity(actor, identity);
    }
    return actor;
}

// Called after a mirrored platform or trap ran its own update, to nudge it towards where the authority's copy is
void Anchor::CorrectCoopMirroredObject(Actor* actor) {
    CoopMirroredObject* mirror = ObjectExtension::GetInstance().Get<CoopMirroredObject>(actor);
    if (mirror == nullptr || gPlayState->gameplayFrames - mirror->lastSnapshotFrame > PUPPET_TIMEOUT_FRAMES ||
        IsCoopRoomAuthority()) {
        return;
    }

    // The authority's copy kept moving since the snapshot, the same way ours did, so move the target along with ours.
    // Otherwise the correction would keep dragging it back between snapshots.
    if (mirror->hasCorrected) {
        mirror->targetPos.x += actor->world.pos.x - mirror->correctedPos.x;
        mirror->targetPos.y += actor->world.pos.y - mirror->correctedPos.y;
        mirror->targetPos.z += actor->world.pos.z - mirror->correctedPos.z;
        mirror->targetRot.x += (s16)(actor->shape.rot.x - mirror->correctedRot.x);
        mirror->targetRot.y += (s16)(actor->shape.rot.y - mirror->correctedRot.y);
        mirror->targetRot.z += (s16)(actor->shape.rot.z - mirror->correctedRot.z);
    }

    actor->world.pos.x += (mirror->targetPos.x - actor->world.pos.x) * OBJECT_CORRECTION_RATE;
    actor->world.pos.y += (mirror->targetPos.y - actor->world.pos.y) * OBJECT_CORRECTION_RATE;
    actor->world.pos.z += (mirror->targetPos.z - actor->world.pos.z) * OBJECT_CORRECTION_RATE;
    actor->shape.rot.x += (s16)((s16)(mirror->targetRot.x - actor->shape.rot.x) * OBJECT_CORRECTION_RATE);
    actor->shape.rot.y += (s16)((s16)(mirror->targetRot.y - actor->shape.rot.y) * OBJECT_CORRECTION_RATE);
    actor->shape.rot.z += (s16)((s16)(mirror->targetRot.z - actor->shape.rot.z) * OBJECT_CORRECTION_RATE);

    mirror->hasCorrected = true;
    mirror->correctedPos = actor->world.pos;
    mirror->correctedRot = actor->shape.rot;
}

// Sets the animation the authority's enemy plays, keeping ours where it is if it's close enough
static void ApplyCoopPuppetAnimation(SkelAnime* skelAnime, CoopPuppet* puppet) {
    if (skelAnime == nullptr || puppet->animation == nullptr) {
        return;
    }

    const char* currentAnimation = GetAnimationPath(skelAnime);
    if (currentAnimation == nullptr || strcmp(currentAnimation, puppet->animation) != 0) {
        Animation_Change(skelAnime, (AnimationHeader*)puppet->animation, puppet->playSpeed, puppet->frame,
                         puppet->endFrame, puppet->mode, -4.0f);
    } else if (puppet->hasNewFrame) {
        if (fabsf(skelAnime->curFrame - puppet->frame) > 2.0f) {
            skelAnime->curFrame = puppet->frame;
        }
        skelAnime->playSpeed = puppet->playSpeed;
        skelAnime->endFrame = puppet->endFrame;
        skelAnime->mode = puppet->mode;
    }
}

static void ApplyCoopPuppetState(Actor* actor, const CoopEnemyAdapter* adapter, CoopPuppet* puppet) {
    size_t stateOffset = 0;
    for (const CoopStateField& field : adapter->stateFields) {
        if (stateOffset + field.size <= puppet->state.size()) {
            memcpy((u8*)actor + field.offset, puppet->state.data() + stateOffset, field.size);
        }
        stateOffset += field.size;
    }
    if (adapter->setPhase != nullptr) {
        adapter->setPhase(actor, puppet->phase);
    }
    if (adapter->setExtraState != nullptr && !puppet->extraState.empty()) {
        adapter->setExtraState(actor, puppet->extraState);
    }
}

static Player* GetCoopClientPlayer(uint32_t clientId) {
    if (clientId == Anchor::Instance->ownClientId || !Anchor::Instance->clients.contains(clientId)) {
        return nullptr;
    }
    return Anchor::Instance->clients[clientId].player;
}

// A dummy player can't be grabbed (by Bongo Bongo's hands, Morpha's tentacles), it would stop following the teammate.
// The grab misses, and in the teammate's own game, where they're the player, it can catch them for real.
static s32 MissGrabbingDummy(PlayState* play, Player* player) {
    return false;
}

// Runs func with a teammate's dummy player standing in for our player
template <typename F> static void WithPlayer(Player* standIn, F func) {
    // GET_PLAYER is the head of the player list. Put it back right away, nothing else may see the dummy there.
    ActorListEntry* playerList = &gPlayState->actorCtx.actorLists[ACTORCAT_PLAYER];
    Actor* player = playerList->head;
    s32 (*grabPlayer)(PlayState*, Player*) = gPlayState->grabPlayer;
    playerList->head = &standIn->actor;
    gPlayState->grabPlayer = MissGrabbingDummy;
    func();
    playerList->head = player;
    gPlayState->grabPlayer = grabPlayer;
}

static void UpdateWithPlayer(Actor* actor, Player* target) {
    WithPlayer(target, [&]() { actor->update(actor, gPlayState); });
}

// Bosses only go after a teammate in their action (where they aim and decide what to do). What else their update does
// to the player (like the flames of Volvagia's breath, or Ganondorf's shockwaves) still goes for whoever is in the way.
// What a few check before their action runs (which then sees the action standing in for theirs, see UpdateWithTarget)
// is checked by beforeUpdate instead, with our own player like in vanilla.
struct CoopBossAction {
    s16 actorId;
    size_t actionFuncOffset;
    bool (*goesAfterTarget)(Actor* actor) = nullptr; // Which of its parts (default: all)
    void (*beforeUpdate)(Actor* actor) = nullptr; // In the authority's game, before it updates going after a teammate
};

// Ganondorf blocks when the player aims at him
static void BeforeGanondorfUpdate(Actor* actor) {
    BossGanon* ganondorf = (BossGanon*)actor;
    if ((ganondorf->actionFunc == BossGanon_Wait || ganondorf->actionFunc == BossGanon_Block) &&
        GET_PLAYER(gPlayState)->unk_A73 != 0) {
        BossGanon_SetupBlock(ganondorf, gPlayState);
    }
}

// The sisters dodge when the player aims at them while facing them
static void BeforeTwinrovaUpdate(Actor* actor) {
    BossTw* sister = (BossTw*)actor;
    Player* player = GET_PLAYER(gPlayState);
    s16 facing = (s16)(player->actor.shape.rot.y - Actor_WorldYawTowardActor(actor, &player->actor) + 0x8000);
    if (actor->params != TW_TWINROVA &&
        (sister->actionFunc == BossTw_FlyTo || sister->actionFunc == BossTw_Spin ||
         sister->actionFunc == BossTw_TurnToPlayer) &&
        facing < 0x1000 && facing > -0x1000 && player->unk_A73) {
        BossTw_SetupSpin(sister, gPlayState);
    }
}

static const CoopBossAction sCoopBossActions[] = {
    { ACTOR_BOSS_GOMA, offsetof(BossGoma, actionFunc) },
    { ACTOR_BOSS_DODONGO, offsetof(BossDodongo, actionFunc) },
    { ACTOR_BOSS_GANON2, offsetof(BossGanon2, actionFunc) },
    { ACTOR_BOSS_VA, offsetof(BossVa, actionFunc) },
    { ACTOR_BOSS_FD, offsetof(BossFd, actionFunc) },
    { ACTOR_BOSS_FD2, offsetof(BossFd2, actionFunc) },
    { ACTOR_BOSS_GANONDROF, offsetof(BossGanondrof, actionFunc) },
    { ACTOR_EN_FHG, offsetof(EnfHG, actionFunc) },
    { ACTOR_BOSS_GANON, offsetof(BossGanon, actionFunc), nullptr, BeforeGanondorfUpdate },
    { ACTOR_BOSS_TW, offsetof(BossTw, actionFunc), nullptr, BeforeTwinrovaUpdate },
    // Bongo Bongo's hands (his head checks whether it can be hurt before its action, and doesn't go after anyone)
    { ACTOR_BOSS_SST, offsetof(BossSst, actionFunc), [](Actor* actor) { return actor->params != BONGO_HEAD; } },
    // Morpha's tentacles (her core moves by itself, not in an action)
    { ACTOR_BOSS_MO, offsetof(BossMo, actionFunc), [](Actor* actor) { return actor->params == BOSSMO_TENTACLE; } },
};

typedef void (*CoopActionFunc)(Actor*, PlayState*);
static CoopActionFunc sTargetedAction;
static Player* sTarget;
static size_t sTargetedActionOffset;

// Takes the place of the boss's action while it runs, see UpdateWithTarget
static void RunTargetedAction(Actor* actor, PlayState* play) {
    CoopActionFunc* actionFunc = (CoopActionFunc*)((uintptr_t)actor + sTargetedActionOffset);
    *actionFunc = sTargetedAction;

    WithPlayer(sTarget, [&]() { sTargetedAction(actor, play); });
}

static const CoopBossAction* FindCoopBossAction(Actor* actor) {
    for (const CoopBossAction& bossAction : sCoopBossActions) {
        if (bossAction.actorId == actor->id) {
            return &bossAction;
        }
    }
    return nullptr;
}

// Whether it goes after a teammate right now, when that's who it targets (see UpdateWithTarget)
static bool CanGoAfterTarget(Actor* actor) {
    const CoopBossAction* bossAction = FindCoopBossAction(actor);
    return bossAction == nullptr || bossAction->goesAfterTarget == nullptr || bossAction->goesAfterTarget(actor);
}

// Runs the actor's update going after a teammate: their dummy player stands in for our player, and the actor gets what
// Actor_UpdateAll works out about the player before each update. For bosses, only in their action.
static void UpdateWithTarget(Actor* actor, Player* target) {
    actor->xzDistToPlayer = Actor_WorldDistXZToActor(actor, &target->actor);
    actor->yDistToPlayer = Actor_HeightDiff(actor, &target->actor);
    actor->xyzDistToPlayerSq = SQ(actor->xzDistToPlayer) + SQ(actor->yDistToPlayer);
    actor->yawTowardsPlayer = Actor_WorldYawTowardActor(actor, &target->actor);

    const CoopBossAction* bossAction = FindCoopBossAction(actor);
    if (bossAction == nullptr) {
        UpdateWithPlayer(actor, target);
        return;
    }

    if (bossAction->beforeUpdate != nullptr && Anchor::Instance->IsCoopRoomAuthority()) {
        bossAction->beforeUpdate(actor);
    }

    // The action puts itself back as soon as it runs. If the update didn't run it, it's put back here.
    CoopActionFunc* actionFunc = (CoopActionFunc*)((uintptr_t)actor + bossAction->actionFuncOffset);
    sTargetedAction = *actionFunc;
    sTarget = target;
    sTargetedActionOffset = bossAction->actionFuncOffset;
    *actionFunc = RunTargetedAction;
    actor->update(actor, gPlayState);
    if (*actionFunc == RunTargetedAction) {
        *actionFunc = sTargetedAction;
    }
}

// While a part of the boss holds our player (a hand, a tentacle), or our hookshot holds a part of it, all of it runs by
// itself in our game, hits included (they still go to the authority too). What it does to our player, and how they get
// away (cutting the tentacle by hitting the core, the hookshot pulling the core out), is in its own actions.
static bool IsCoopHeldLocally(Actor* actor) {
    Actor* holder = GET_PLAYER(gPlayState)->actor.parent;
    if (holder == actor) {
        return true;
    }
    if (holder != nullptr && holder->id == actor->id) {
        const CoopEnemyAdapter* adapter = FindCoopEnemyAdapter(holder);
        if (adapter != nullptr && adapter->getBossFight != nullptr) {
            return true;
        }
    }
    return IsPulledByOurHookshot(actor);
}

// Runs before the enemy's update while it's a puppet. Returns true if its update is skipped, which is the case for
// enemies. A boss runs its own update (see the top of the file), unless it should run its own AI.
bool Anchor::UpdateCoopPuppet(Actor* actor) {
    const CoopEnemyAdapter* adapter = FindCoopEnemyAdapter(actor);
    if (adapter == nullptr) {
        return false;
    }

    CoopPuppet* puppet = ObjectExtension::GetInstance().Get<CoopPuppet>(actor);
    if (puppet == nullptr || puppet->isReleased || !IsCoopSyncActive() ||
        gPlayState->gameplayFrames - puppet->lastSnapshotFrame > PUPPET_TIMEOUT_FRAMES || IsCoopRoomAuthority()) {
        return false;
    }

    // A boss is only a puppet while both ours and the authority's are fighting. Once the authority's is defeated, ours
    // is too, and plays its own defeat from there.
    bool isBoss = adapter->getBossFight != nullptr;
    if (isBoss) {
        if (adapter->getBossFight(actor) != COOP_BOSS_FIGHTING) {
            return false;
        }
        // Not while it holds our player, it lets go of them first (like Morpha's own death waits for that)
        if (puppet->bossFight == COOP_BOSS_DEFEATED && !IsCoopHeldLocally(actor)) {
            puppet->isReleased = true;
            if (adapter->defeat != nullptr) {
                adapter->defeat(actor);
            }
            return false;
        }
        // Into the cutscene the authority's is in, played by ours from there (it joins the fight again after)
        if (puppet->bossFight == COOP_BOSS_CUTSCENE && adapter->enterCutscene != nullptr && !IsCoopHeldLocally(actor)) {
            adapter->enterCutscene(actor);
            return false;
        }
        if (puppet->bossFight != COOP_BOSS_FIGHTING) {
            return false;
        }
    }

    // Remember where the hurt colliders sit relative to the enemy, to carry them along as it moves
    if (!puppet->hasColliderOffsets) {
        puppet->hasColliderOffsets = true;
        for (size_t i = 0; i < adapter->hurtColliderOffsets.size(); i++) {
            ForEachColliderElement(GetHurtCollider(actor, adapter, i), [&](ColliderInfo* info, Vec3s* pos) {
                puppet->colliderOffsets.push_back(
                    { pos->x - actor->world.pos.x, pos->y - actor->world.pos.y, pos->z - actor->world.pos.z });
            });
        }
    }

    // Hits from us since the last frame go to the authority. The puppet never reacts to them, so a weapon keeps
    // touching it for as long as it overlaps, which is all one hit.
    u32 frame = gPlayState->gameplayFrames;
    std::erase_if(puppet->recentAttackers,
                  [&](const auto& entry) { return frame - entry.second > PUPPET_HIT_CONTACT_GAP_FRAMES; });
    std::vector<Actor*> touchingAttackers;
    Player* player = GET_PLAYER(gPlayState);
    for (size_t i = 0; i < adapter->hurtColliderOffsets.size(); i++) {
        Collider* collider = GetHurtCollider(actor, adapter, i);
        if (!(collider->acFlags & AC_HIT)) {
            continue;
        }

        // Our sword, or something we shot or threw
        Actor* attacker = collider->ac;
        if (attacker == nullptr || (attacker != &player->actor && attacker->parent != &player->actor &&
                                    !ObjectExtension::GetInstance().Has<CoopOwnedByUs>(attacker))) {
            continue;
        }

        // Not if it was already touching it, then this is still the same hit
        bool isNewHit =
            std::none_of(puppet->recentAttackers.begin(), puppet->recentAttackers.end(),
                         [&](const auto& entry) { return entry.first == attacker; }) &&
            std::find(touchingAttackers.begin(), touchingAttackers.end(), attacker) == touchingAttackers.end();
        touchingAttackers.push_back(attacker);
        if (!isNewHit) {
            continue;
        }

        s32 elementIndex = 0;
        s32 currentIndex = 0;
        ColliderInfo* hitInfo = nullptr;
        ForEachColliderElement(collider, [&](ColliderInfo* info, Vec3s* pos) {
            if (hitInfo == nullptr && (info->bumperFlags & BUMP_HIT) && info->acHitInfo != nullptr) {
                hitInfo = info;
                elementIndex = currentIndex;
            }
            currentIndex++;
        });
        if (hitInfo == nullptr) {
            continue;
        }

        uint32_t authority = GetCoopRoomAuthority();
        if (authority != ownClientId && authority != 0) {
            nlohmann::json payload = PrepCoopPayload(COOP_ENEMY_HIT);
            payload["targetClientId"] = authority;
            payload["actor"] = GetCoopActorIdentity(actor);
            payload["collider"] = i;
            payload["element"] = elementIndex;
            payload["dmgFlags"] = hitInfo->acHitInfo->toucher.dmgFlags;
            payload["effect"] = hitInfo->acHitInfo->toucher.effect;
            payload["damage"] = hitInfo->acHitInfo->toucher.damage;
            payload["hitPos"] = hitInfo->bumper.hitPos;
            SendJsonToRemote(payload);
        }
    }
    for (Actor* attacker : touchingAttackers) {
        std::erase_if(puppet->recentAttackers, [&](const auto& entry) { return entry.first == attacker; });
        puppet->recentAttackers.push_back({ attacker, frame });
    }

    // A boss runs its own update from the authority's latest state, without reacting to any hit itself (the hits
    // above reach it through the authority)
    if (isBoss) {
        puppet->isHeldLocally = IsCoopHeldLocally(actor);
        for (size_t i = 0; i < adapter->hurtColliderOffsets.size() && !puppet->isHeldLocally; i++) {
            Collider* collider = GetHurtCollider(actor, adapter, i);
            collider->acFlags &= ~AC_HIT;
            ForEachColliderElement(collider, [&](ColliderInfo* info, Vec3s* pos) { info->bumperFlags &= ~BUMP_HIT; });
        }
        if (puppet->hasNewFrame && !puppet->isHeldLocally && frame >= puppet->holdUntilFrame) {
            ApplyCoopPuppetAnimation(GetSkelAnime(actor, adapter), puppet);
            ApplyCoopPuppetState(actor, adapter, puppet);
            if (adapter->sanitizeState != nullptr) {
                adapter->sanitizeState(actor);
            }
            puppet->hasNewFrame = false;
            if (adapter->getPhase != nullptr) {
                puppet->phaseBefore = adapter->getPhase(actor);
            }
        }
        puppet->drivenFrame = frame;

        // It goes after the player the authority's goes after: in their game they're our dummy player of them. Not for
        // the ones that grab who they go after (that would grab the dummy player), they go after our own player.
        Player* target = GetCoopClientPlayer(puppet->targetClientId);
        if (adapter->canTargetTeammates && !puppet->isHeldLocally && target != nullptr && CanGoAfterTarget(actor)) {
            UpdateWithTarget(actor, target);
            GameInteractor_ExecuteOnActorUpdate(actor);
            return true;
        }
        return false;
    }

    // Follow the authority, snapping if it's far off (e.g. the first snapshot)
    Vec3f diff = { puppet->targetPos.x - actor->world.pos.x, puppet->targetPos.y - actor->world.pos.y,
                   puppet->targetPos.z - actor->world.pos.z };
    f32 step = (SQ(diff.x) + SQ(diff.y) + SQ(diff.z) > SQ(300.0f)) ? 1.0f : 0.5f;
    actor->world.pos.x += diff.x * step;
    actor->world.pos.y += diff.y * step;
    actor->world.pos.z += diff.z * step;
    actor->shape.rot = puppet->shapeRot;
    actor->world.rot.y = puppet->worldRotY;
    SetCoopHealth(actor, puppet->health);
    actor->scale = puppet->scale;
    actor->focus.pos = { actor->world.pos.x + puppet->focusOffset.x, actor->world.pos.y + puppet->focusOffset.y,
                         actor->world.pos.z + puppet->focusOffset.z };

    SkelAnime* skelAnime = GetSkelAnime(actor, adapter);
    ApplyCoopPuppetAnimation(skelAnime, puppet);
    puppet->hasNewFrame = false;
    if (skelAnime != nullptr) {
        SkelAnime_Update(skelAnime);
    }
    ApplyCoopPuppetState(actor, adapter, puppet);

    // Can be hit and pushed, but never attacks (only the authority's real enemy does)
    size_t offsetIndex = 0;
    for (size_t i = 0; i < adapter->hurtColliderOffsets.size(); i++) {
        Collider* collider = GetHurtCollider(actor, adapter, i);
        ForEachColliderElement(collider, [&](ColliderInfo* info, Vec3s* pos) {
            if (offsetIndex < puppet->colliderOffsets.size()) {
                Vec3f& offset = puppet->colliderOffsets[offsetIndex];
                pos->x = actor->world.pos.x + offset.x;
                pos->y = actor->world.pos.y + offset.y;
                pos->z = actor->world.pos.z + offset.z;
            }
            offsetIndex++;
        });

        if (collider->acFlags & AC_ON) {
            CollisionCheck_SetAC(gPlayState, &gPlayState->colChkCtx, collider);
        }
        if (collider->ocFlags1 & OC1_ON) {
            CollisionCheck_SetOC(gPlayState, &gPlayState->colChkCtx, collider);
        }
    }

    return true;
}

// Spawned by a boss as part of itself (Morpha's second tentacle): it's matched up with the authority's, not left out
bool IsCoopBossPart(Actor* actor) {
    const CoopEnemyAdapter* adapter = FindCoopEnemyAdapter(actor);
    return adapter != nullptr && adapter->getBossFight != nullptr && adapter->puppetSpawn == COOP_PUPPET_SPAWN_NONE;
}

// A boss puppet that just ran its own update this frame
bool IsCoopDrivenPuppet(Actor* actor) {
    const CoopPuppet* puppet = ObjectExtension::GetInstance().Get<CoopPuppet>(actor);
    return puppet != nullptr && puppet->drivenFrame == gPlayState->gameplayFrames;
}

#define BOSS_EVENT_HOLD_FRAMES 10
#define DRIVEN_SNAP_DISTANCE 150.0f // Further off than this (like popping out of another hole), it jumps there

static s16 NudgeAngle(s16 angle, s16 target) {
    return angle + (s16)((s16)(target - angle) * OBJECT_CORRECTION_RATE);
}

// Called after a boss puppet ran its own update, to nudge it towards where the authority's is, and keep the health
// the authority's has
void Anchor::CorrectCoopDrivenPuppet(Actor* actor) {
    if (!IsCoopDrivenPuppet(actor)) {
        return;
    }
    CoopPuppet* puppet = ObjectExtension::GetInstance().Get<CoopPuppet>(actor);
    SetCoopHealth(actor, puppet->health);

    // What it spawned as its child was left out (see ShouldCoopActorInit), and is gone next frame
    if (actor->child != nullptr && actor->child->update == NULL && actor->child->init == NULL &&
        actor->child->parent == actor) {
        actor->child = nullptr;
    }

    // What changed since the authority's state was last applied (or since the last update), whether in its own update
    // or another part's (one Twinrova sister's beam hitting the other)
    const CoopEnemyAdapter* adapter = FindCoopEnemyAdapter(actor);
    u8 event = adapter->getLocalEvent != nullptr ? adapter->getLocalEvent(actor, puppet->phaseBefore) : 0;
    if (adapter->getPhase != nullptr) {
        puppet->phaseBefore = adapter->getPhase(actor);
    }
    uint32_t authority = GetCoopRoomAuthority();
    if (event != 0 && authority != 0 && authority != ownClientId) {
        nlohmann::json payload = PrepCoopPayload(COOP_BOSS_EVENT);
        payload["targetClientId"] = authority;
        payload["actor"] = GetCoopActorIdentity(actor);
        payload["event"] = event;
        payload["pos"] = actor->world.pos;
        SendJsonToRemote(payload);
        puppet->holdUntilFrame = gPlayState->gameplayFrames + BOSS_EVENT_HOLD_FRAMES;
    }

    if (puppet->isHeldLocally) {
        puppet->hasCorrected = false;
        return;
    }

    // The authority's kept moving since the snapshot, the same way ours did, so move the target along with ours.
    // Otherwise the correction would keep dragging it back between snapshots.
    if (puppet->hasCorrected) {
        puppet->targetPos.x += actor->world.pos.x - puppet->correctedPos.x;
        puppet->targetPos.y += actor->world.pos.y - puppet->correctedPos.y;
        puppet->targetPos.z += actor->world.pos.z - puppet->correctedPos.z;
        for (s32 i = 0; i < 3; i++) {
            (&puppet->shapeRot.x)[i] += (s16)((&actor->shape.rot.x)[i] - (&puppet->correctedShapeRot.x)[i]);
            (&puppet->worldRot.x)[i] += (s16)((&actor->world.rot.x)[i] - (&puppet->correctedWorldRot.x)[i]);
        }
    }

    Vec3f diff = { puppet->targetPos.x - actor->world.pos.x, puppet->targetPos.y - actor->world.pos.y,
                   puppet->targetPos.z - actor->world.pos.z };
    if (SQ(diff.x) + SQ(diff.y) + SQ(diff.z) > SQ(DRIVEN_SNAP_DISTANCE)) {
        actor->world.pos = puppet->targetPos;
        actor->shape.rot = puppet->shapeRot;
        actor->world.rot = puppet->worldRot;
    } else {
        actor->world.pos.x += diff.x * OBJECT_CORRECTION_RATE;
        actor->world.pos.y += diff.y * OBJECT_CORRECTION_RATE;
        actor->world.pos.z += diff.z * OBJECT_CORRECTION_RATE;
        for (s32 i = 0; i < 3; i++) {
            (&actor->shape.rot.x)[i] = NudgeAngle((&actor->shape.rot.x)[i], (&puppet->shapeRot.x)[i]);
            (&actor->world.rot.x)[i] = NudgeAngle((&actor->world.rot.x)[i], (&puppet->worldRot.x)[i]);
        }
    }

    puppet->hasCorrected = true;
    puppet->correctedPos = actor->world.pos;
    puppet->correctedShapeRot = actor->shape.rot;
    puppet->correctedWorldRot = actor->world.rot;
}

void Anchor::HandlePacket_CoopBossEvent(nlohmann::json payload) {
    if (!IsCoopPayloadForThisScene(payload) || !IsCoopRoomAuthority()) {
        return;
    }

    Actor* actor = FindCoopActor(payload.at("actor").get<CoopActorIdentity>());
    const CoopEnemyAdapter* adapter = actor != nullptr ? FindCoopEnemyAdapter(actor) : nullptr;
    if (adapter != nullptr && adapter->applyEvent != nullptr && IsMirroredNow(actor)) {
        RecordCoopAttacker(actor, payload.at("clientId").get<uint32_t>(), THREAT_EVENT_FRAMES);
        adapter->applyEvent(actor, payload.at("event").get<u8>(), payload.at("pos").get<Vec3f>());
    }
}

// MARK: - Aim

/**
 * COOP_BOSS_AIM
 *
 * What a boss does at the player it goes after that depends on that player's own game, like a Twinrova sister's beam
 * being reflected by their Mirror Shield (which needs them to hold up their shield, facing the beam). Their game sends
 * it while it lasts, and everyone else's boss shows the same.
 */

struct CoopBossAim {
    s16 actorId;
    bool (*isActive)(Actor* actor);
    std::vector<CoopStateField> fields;
};

static const CoopBossAim sCoopBossAims[] = {
    // A Twinrova sister's beam, and its reflection
    { ACTOR_BOSS_TW,
      [](Actor* actor) { return actor->params != TW_TWINROVA && ((BossTw*)actor)->actionFunc == BossTw_ShootBeam; },
      { STATE_FIELD(BossTw, targetPos), STATE_FIELD(BossTw, beamShootState),
        STATE_RANGE(BossTw, beamReflectionOrigin, unk_54C) } },
};

#define BOSS_AIM_TIMEOUT_FRAMES 4

struct CoopBossAimState {
    u32 frame;
    std::vector<u8> state;
};
static ObjectExtension::Register<CoopBossAimState> CoopBossAimStateRegister;

static const CoopBossAim* FindCoopBossAim(Actor* actor) {
    for (const CoopBossAim& aim : sCoopBossAims) {
        if (aim.actorId == actor->id && aim.isActive(actor)) {
            return &aim;
        }
    }
    return nullptr;
}

// Whether our player is who it goes after (in the authority's game, or as a puppet of it)
static bool IsGoingAfterUs(Actor* actor) {
    if (Anchor::Instance->IsCoopRoomAuthority()) {
        const CoopEnemyTarget* target = ObjectExtension::GetInstance().Get<CoopEnemyTarget>(actor);
        return target == nullptr || target->clientId == 0;
    }
    const CoopPuppet* puppet = ObjectExtension::GetInstance().Get<CoopPuppet>(actor);
    return IsCoopDrivenPuppet(actor) && puppet->targetClientId == Anchor::Instance->ownClientId;
}

// Called after a boss part updated: sends its aim if it goes after us, or shows the aim of the teammate it goes after
void Anchor::UpdateCoopBossAim(Actor* actor) {
    const CoopBossAim* aim = IsCoopSyncActive() ? FindCoopBossAim(actor) : nullptr;
    if (aim == nullptr) {
        return;
    }

    if (IsGoingAfterUs(actor)) {
        std::vector<uint32_t> peers = GetCoopPeers(true);
        if (peers.empty()) {
            return;
        }

        std::vector<u8> state;
        for (const CoopStateField& field : aim->fields) {
            u8* data = (u8*)actor + field.offset;
            state.insert(state.end(), data, data + field.size);
        }
        nlohmann::json payload = PrepCoopPayload(COOP_BOSS_AIM);
        payload["actor"] = GetCoopActorIdentity(actor);
        payload["state"] = state;
        payload["quiet"] = true;
        for (uint32_t clientId : peers) {
            payload["targetClientId"] = clientId;
            SendJsonToRemote(payload);
        }
        return;
    }

    CoopBossAimState* aimState = ObjectExtension::GetInstance().Get<CoopBossAimState>(actor);
    if (aimState == nullptr || gPlayState->gameplayFrames - aimState->frame > BOSS_AIM_TIMEOUT_FRAMES) {
        return;
    }
    size_t offset = 0;
    for (const CoopStateField& field : aim->fields) {
        if (offset + field.size <= aimState->state.size()) {
            memcpy((u8*)actor + field.offset, aimState->state.data() + offset, field.size);
        }
        offset += field.size;
    }
}

void Anchor::HandlePacket_CoopBossAim(nlohmann::json payload) {
    if (!IsCoopPayloadForThisScene(payload)) {
        return;
    }

    Actor* actor = FindCoopActor(payload.at("actor").get<CoopActorIdentity>());
    if (actor != nullptr) {
        ObjectExtension::GetInstance().Set<CoopBossAimState>(
            actor, CoopBossAimState{ gPlayState->gameplayFrames, payload.at("state").get<std::vector<u8>>() });
    }
}

// MARK: - Targeting

// Picks the player the enemy goes after: the one in the room who dealt it the most damage, or while nobody did yet, the
// nearest one, sticking with the current one unless another is clearly closer
static Player* ChooseCoopEnemyTarget(Actor* actor, const std::vector<uint32_t>& peers, uint32_t& targetClientId) {
    const CoopThreat* threat = GetCoopThreat(actor);
    Player* mostDangerous = nullptr;
    uint32_t mostDangerousClientId = 0;
    s32 mostDamage = 0;
    f32 mostDangerousDist = FLT_MAX;
    auto considerThreat = [&](uint32_t clientId, Player* player) {
        auto it = threat->damage.find(clientId);
        s32 damage = it != threat->damage.end() ? it->second : 0;
        f32 dist = Math_Vec3f_DistXYZ(&actor->world.pos, &player->actor.world.pos);
        if (damage > mostDamage || (damage == mostDamage && damage > 0 && dist < mostDangerousDist)) {
            mostDangerous = player;
            mostDangerousClientId = clientId == Anchor::Instance->ownClientId ? 0 : clientId;
            mostDamage = damage;
            mostDangerousDist = dist;
        }
    };
    considerThreat(Anchor::Instance->ownClientId, GET_PLAYER(gPlayState));
    for (uint32_t clientId : peers) {
        AnchorClient& client = Anchor::Instance->clients[clientId];
        if (client.player != nullptr && !(client.player->stateFlags1 & PLAYER_STATE1_DEAD) && !client.isBossLeftOut) {
            considerThreat(clientId, client.player);
        }
    }
    if (mostDangerous != nullptr) {
        targetClientId = mostDangerousClientId;
        return mostDangerous;
    }

    CoopEnemyTarget* currentTarget = ObjectExtension::GetInstance().Get<CoopEnemyTarget>(actor);
    uint32_t currentClientId = currentTarget != nullptr ? currentTarget->clientId : 0;

    Player* best = GET_PLAYER(gPlayState);
    uint32_t bestClientId = 0;
    f32 bestDist = Math_Vec3f_DistXYZ(&actor->world.pos, &best->actor.world.pos);
    f32 currentDist = currentClientId == 0 ? bestDist : FLT_MAX;

    for (uint32_t clientId : peers) {
        AnchorClient& client = Anchor::Instance->clients[clientId];
        if (client.player == nullptr || (client.player->stateFlags1 & PLAYER_STATE1_DEAD) || client.isBossLeftOut) {
            continue;
        }

        f32 dist = Math_Vec3f_DistXYZ(&actor->world.pos, &client.player->actor.world.pos);
        if (clientId == currentClientId) {
            currentDist = dist;
        }
        if (dist < bestDist) {
            best = client.player;
            bestClientId = clientId;
            bestDist = dist;
        }
    }

    // Stay on the current target (if it's still around) unless the nearest player is clearly closer
    if (bestClientId != currentClientId && currentDist != FLT_MAX && currentDist < bestDist + TARGET_SWITCH_DISTANCE) {
        bestClientId = currentClientId;
        best = currentClientId == 0 ? GET_PLAYER(gPlayState) : Anchor::Instance->clients[currentClientId].player;
    }

    targetClientId = bestClientId;
    return best;
}

// Runs the enemy's update with a teammate's dummy player standing in for our player, if that's who it goes after.
// Returns false if it should just update normally.
bool Anchor::UpdateCoopEnemyWithTarget(Actor* actor) {
    const CoopEnemyAdapter* adapter = FindCoopEnemyAdapter(actor);
    if (adapter == nullptr || !adapter->canTargetTeammates || actor->update == NULL || !IsCoopSyncActive() ||
        !IsMirroredType(actor) || !IsMirroredNow(actor)) {
        return false;
    }

    std::vector<uint32_t> peers = GetCoopPeers(true);
    if (peers.empty() || !IsCoopRoomAuthority()) {
        return false;
    }

    uint32_t targetClientId;
    Player* target = ChooseCoopEnemyTarget(actor, peers, targetClientId);
    ObjectExtension::GetInstance().Set<CoopEnemyTarget>(actor, CoopEnemyTarget{ targetClientId });
    if (targetClientId == 0 || !CanGoAfterTarget(actor)) {
        return false;
    }

    UpdateWithTarget(actor, target);
    GameInteractor_ExecuteOnActorUpdate(actor);
    return true;
}

// MARK: - Platforms

// The mirrored platform our player is standing on, sent along with PLAYER_UPDATE
bool Anchor::GetCoopStandingOn(CoopActorIdentity* identity) {
    Player* player = GET_PLAYER(gPlayState);
    if (!IsCoopSyncActive() || !(player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) ||
        player->actor.floorBgId == BGCHECK_SCENE) {
        return false;
    }

    DynaPolyActor* floor = DynaPoly_GetActor(&gPlayState->colCtx, player->actor.floorBgId);
    if (floor == nullptr || !IsCoopMirroredObject(&floor->actor)) {
        return false;
    }

    *identity = GetCoopActorIdentity(&floor->actor);
    return true;
}

// Called before a mirrored platform updates in our game, if we run it: teammates standing on it count as standing on
// it in our game too, so it starts moving for them like it does for us
void Anchor::ApplyCoopTeammatesStandingOn(Actor* actor) {
    if (!IsCoopMirroredObject(actor) || !IsCoopSyncActive() || !IsCoopRoomAuthority()) {
        return;
    }

    CoopActorIdentity identity = GetCoopActorIdentity(actor);
    for (uint32_t clientId : GetCoopPeers(true)) {
        AnchorClient& client = clients[clientId];
        if (client.isStandingOnCoopObject && client.standingOnCoopObject == identity) {
            ((DynaPolyActor*)actor)->interactFlags |= DYNA_INTERACT_PLAYER_ON_TOP | DYNA_INTERACT_PLAYER_ABOVE;
        }
    }
}

// MARK: - Floormaster

// A Floormaster spawns the two small ones it splits into in the same spot as itself, so they're told apart by which
// of its two slots they're in
void Anchor::OnCoopFloormasterInit(Actor* actor) {
    if (actor->params != 0x10 /* SPAWN_SMALL */) {
        SetCoopChildSlot(actor->parent, actor, 1);
        SetCoopChildSlot(actor->child, actor, 2);
    }
}

// MARK: - Shots

/**
 * COOP_SHOT_STATE
 *
 * Shots that a player plays back at the boss (Phantom Ganon's energy balls) are run by the game of the player they fly
 * at: only there can that player hit them back, or be hit by them. Once one flies back at the boss (or the boss returns
 * it), the authority's game runs it, since that's where the boss is. Everyone else's copy doesn't run by itself, it
 * follows the state the game running it sends every frame. When the shot ends there, it ends for everyone, along with
 * what that does to the boss.
 */

struct CoopShotAdapter {
    s16 actorId;
    bool (*isShot)(Actor* actor);
    std::vector<CoopStateField> stateFields; // Sent along with its position, direction and speed
    bool (*isFlyingAtPlayer)(Actor* actor);  // Run by the player it flies at while true, otherwise by the authority
    bool (*hasEnded)(Actor* actor);
    void (*onFollow)(Actor* actor); // Its look while it follows another game's (its own update doesn't run)
    void (*onEnded)(Actor* actor, bool isAuthority); // When it ended in the game running it, for the others
};

static bool IsEnergyBall(Actor* actor) {
    return actor->params == FHGFIRE_ENERGY_BALL;
}

static void FollowEnergyBall(Actor* actor) {
    EnFhgFire* ball = (EnFhgFire*)actor;
    ball->work[FHGFIRE_VARIANCE_TIMER]++;
    Actor_SetScale(actor, (ball->work[FHGFIRE_VARIANCE_TIMER] & 1) ? 6.0f : 5.25f);
    actor->shape.rot.z += (s16)(Rand_ZeroOne() * 0x4E20) + 0x4000;
    Lights_PointNoGlowSetInfo(&ball->lightInfo, (s16)actor->world.pos.x, (s16)actor->world.pos.y,
                              (s16)actor->world.pos.z, 255, 255, 255, 200);
}

// The same as the end of the ball in EnFhgFire_EnergyBall, for Phantom Ganon
static void EndEnergyBall(Actor* actor, bool isAuthority) {
    if (isAuthority && actor->parent != nullptr && actor->parent->id == ACTOR_BOSS_GANONDROF) {
        ((BossGanondrof*)actor->parent)->flyMode = GND_FLY_NEUTRAL;
    }
}

// Ganondorf's light ball, which flies at a player in state 0 (unk_1C2), and is done once unk_1A8 is set
static bool IsGanondorfLightBall(Actor* actor) {
    return actor->params >= 0x64 && actor->params < 0xC8;
}

static void FollowGanondorfLightBall(Actor* actor) {
    BossGanon* ball = (BossGanon*)actor;
    Vec3f zero = { 0.0f, 0.0f, 0.0f };
    Vec3f accel = { 0.0f, 0.2f, 0.0f };

    ball->unk_1A2++;
    Actor_SetScale(actor, (ball->unk_1A2 % 2) != 0 ? 6.0f : 5.25f);
    actor->shape.rot.z += (s16)(Rand_ZeroOne() * 20000.0f) + 0x4000;
    if (actor->parent != nullptr) {
        ((BossGanon*)actor->parent)->envLightMode = 1;
    }
    for (s32 i = 0; i < 2; i++) {
        Vec3f pos = { Rand_CenteredFloat(30.0f) + actor->world.pos.x, Rand_CenteredFloat(30.0f) + actor->world.pos.y,
                      Rand_CenteredFloat(30.0f) + actor->world.pos.z };
        BossGanonEff_SpawnSparkle(gPlayState, &pos, &zero, &accel, Rand_ZeroFloat(500.0f) + 700.0f, 0x1E);
    }
    Audio_PlayActorSound2(actor, NA_SE_EN_GANON_FIRE - SFX_FLAG);
}

// A ball that ended on the floor breaks the platform there. Ganondorf stops playing tennis once it's gone without
// hitting him (that only happens in the authority's game, which runs it then).
static void EndGanondorfLightBall(Actor* actor, bool isAuthority) {
    BossGanon* ganondorf = actor->parent != nullptr ? (BossGanon*)actor->parent : nullptr;
    if (ganondorf == nullptr) {
        return;
    }
    if (actor->world.pos.y < 20.0f) {
        BossGanon_CheckFallingPlatforms((BossGanon*)actor, gPlayState, &actor->world.pos);
    }
    if (isAuthority && ganondorf->actionFunc == BossGanon_PlayTennis) {
        BossGanon_SetupWait(ganondorf, gPlayState);
    }
}

// The balls of his big magic attack, which home in on a player in states 10 and 11, and are done in state 1
static bool IsGanondorfBigMagicBall(Actor* actor) {
    return actor->params >= 0x104 && actor->params < 0x12C;
}

static void FollowGanondorfBigMagicBall(Actor* actor) {
    BossGanon* ball = (BossGanon*)actor;
    ball->unk_1A2++;
    ball->unk_1A6 = (ball->unk_1A6 + 1) % 15;
    ball->unk_2EC[ball->unk_1A6] = actor->world.pos;
    ball->unk_3C4[ball->unk_1A6].x = BINANG_TO_RAD(actor->world.rot.x);
    ball->unk_3C4[ball->unk_1A6].y = BINANG_TO_RAD(actor->world.rot.y);
}

// One that flew off out of the room breaks the platform it ended up at
static void EndGanondorfBigMagicBall(Actor* actor, bool isAuthority) {
    if (fabsf(actor->world.pos.x) > 400.0f || fabsf(actor->world.pos.z) > 400.0f || actor->world.pos.y < 0.0f) {
        BossGanon_CheckFallingPlatforms((BossGanon*)actor, gPlayState, &actor->world.pos);
    }
}

// Twinrova's fire and ice blasts, which fly at a player until they're done (state 2). A player absorbing three in a
// row with the Mirror Shield keeps the third one (state 10) and blasts it back at her from their shield.
static bool IsTwinrovaBlast(Actor* actor) {
    return actor->params == TW_FIRE_BLAST || actor->params == TW_ICE_BLAST;
}

static void FollowTwinrovaBlast(Actor* actor) {
    BossTw* blast = (BossTw*)actor;
    blast->work[TAIL_IDX] = (blast->work[TAIL_IDX] + 1) % 30;
    blast->blastTailPos[blast->work[TAIL_IDX]] = actor->world.pos;
}

// One that ended on the floor (not on a shield) sets it on fire or freezes it
static void EndTwinrovaBlast(Actor* actor, bool isAuthority) {
    BossTw* blast = (BossTw*)actor;
    if (blast->work[INVINC_TIMER] == 0 && blast->groundBlastPos.y >= 0.0f && blast->groundBlastPos.y != 35.0f) {
        BossTw_SpawnGroundBlast(blast, gPlayState, actor->params == TW_FIRE_BLAST ? 1 : 0);
    }
}

static const std::vector<CoopShotAdapter> sCoopShotAdapters = {
    // Phantom Ganon's energy ball, green while flying at a player and blue while flying back at him
    { ACTOR_EN_FHG_FIRE,
      IsEnergyBall,
      { STATE_RANGE(EnFhgFire, work, lightNode) },
      [](Actor* actor) { return ((EnFhgFire*)actor)->work[FHGFIRE_FIRE_MODE] == FHGFIRE_LIGHT_GREEN; },
      [](Actor* actor) { return ((EnFhgFire*)actor)->work[FHGFIRE_KILL_TIMER] != 0; },
      FollowEnergyBall,
      EndEnergyBall },
    // Ganondorf's light ball, and the balls of his big magic attack, which are the same actor (and the same actor as
    // Ganondorf himself)
    { ACTOR_BOSS_GANON,
      IsGanondorfLightBall,
      { STATE_RANGE(BossGanon, unk_1A2, unk_1F0) },
      [](Actor* actor) { return ((BossGanon*)actor)->unk_1A8 == 0 && ((BossGanon*)actor)->unk_1C2 == 0; },
      [](Actor* actor) { return ((BossGanon*)actor)->unk_1A8 != 0; },
      FollowGanondorfLightBall,
      EndGanondorfLightBall },
    { ACTOR_BOSS_TW,
      IsTwinrovaBlast,
      { STATE_RANGE(BossTw, work, fogR), STATE_FIELD(BossTw, csState1), STATE_FIELD(BossTw, magicDir),
        STATE_RANGE(BossTw, visible, skelAnime), STATE_FIELD(BossTw, groundBlastPos) },
      [](Actor* actor) { return ((BossTw*)actor)->csState1 != 2; },
      [](Actor* actor) { return ((BossTw*)actor)->csState1 == 2; },
      FollowTwinrovaBlast,
      EndTwinrovaBlast },
    { ACTOR_BOSS_GANON,
      IsGanondorfBigMagicBall,
      { STATE_RANGE(BossGanon, unk_1A2, unk_1FC), STATE_FIELD(BossGanon, csCamMaxStepScale) },
      [](Actor* actor) { return ((BossGanon*)actor)->unk_1C2 == 10 || ((BossGanon*)actor)->unk_1C2 == 11; },
      [](Actor* actor) { return ((BossGanon*)actor)->unk_1C2 == 1; },
      FollowGanondorfBigMagicBall,
      EndGanondorfBigMagicBall },
};

#define SHOT_TAKEOVER_FRAMES 20 // Without any state from the game running it for this long, the authority takes over

struct CoopShot {
    uint32_t id;
    uint32_t owner;  // Whose game runs it
    uint32_t target; // Who it flies at (when it does)
    u32 lastStateFrame;
    bool hasEnded = false;
};
static ObjectExtension::Register<CoopShot> CoopShotRegister;

static const CoopShotAdapter* FindCoopShotAdapter(Actor* actor) {
    for (const CoopShotAdapter& adapter : sCoopShotAdapters) {
        if (adapter.actorId == actor->id && adapter.isShot(actor)) {
            return &adapter;
        }
    }
    return nullptr;
}

// Called before a shot updates. Returns true if its own update is skipped (or was run here already).
bool Anchor::UpdateCoopShot(Actor* actor) {
    CoopShot* shot = ObjectExtension::GetInstance().Get<CoopShot>(actor);
    const CoopShotAdapter* adapter = shot != nullptr ? FindCoopShotAdapter(actor) : nullptr;
    if (adapter == nullptr || !IsCoopSyncActive()) {
        return false;
    }

    // The game running it went quiet (left, paused), so the authority takes over
    if (shot->owner != ownClientId && IsCoopRoomAuthority() &&
        gPlayState->gameplayFrames - shot->lastStateFrame > SHOT_TAKEOVER_FRAMES) {
        shot->owner = ownClientId;
    }

    if (shot->owner != ownClientId) {
        // Keeps flying the way it did, until the next state comes in
        if (!shot->hasEnded) {
            Actor_UpdateVelocityXYZ(actor);
            Actor_UpdatePos(actor);
            adapter->onFollow(actor);
        }
        return true;
    }

    // The authority runs it while it isn't flying at a player, and the boss returning it sends it at the player the
    // boss goes after
    if (IsCoopRoomAuthority()) {
        const CoopEnemyTarget* bossTarget =
            actor->parent != nullptr ? ObjectExtension::GetInstance().Get<CoopEnemyTarget>(actor->parent) : nullptr;
        shot->target = (bossTarget != nullptr && bossTarget->clientId != 0) ? bossTarget->clientId : ownClientId;
        Player* target = GetCoopClientPlayer(shot->target);
        if (target != nullptr) {
            UpdateWithTarget(actor, target);
            GameInteractor_ExecuteOnActorUpdate(actor);
            return true;
        }
    }
    return false;
}

// Called after a shot we run updated: tells the others where it is, and hands it over if it's someone else's now. Also
// when it's removed, which ends it for the others too (like a blast absorbed by Twinrova's shield).
void Anchor::SendCoopShotState(Actor* actor, bool isRemoved) {
    CoopShot* shot = ObjectExtension::GetInstance().Get<CoopShot>(actor);
    const CoopShotAdapter* adapter = shot != nullptr ? FindCoopShotAdapter(actor) : nullptr;
    if (adapter == nullptr || shot->owner != ownClientId || shot->hasEnded || !IsCoopSyncActive()) {
        return;
    }

    uint32_t authority = GetCoopRoomAuthority();
    bool wasFlyingAtPlayer = shot->owner == shot->target;
    shot->hasEnded = adapter->hasEnded(actor) || isRemoved;
    shot->owner = adapter->isFlyingAtPlayer(actor) ? shot->target : (authority != 0 ? authority : ownClientId);

    // Our player sent it back at the boss
    if (wasFlyingAtPlayer && !shot->hasEnded && !adapter->isFlyingAtPlayer(actor) && actor->parent != nullptr &&
        IsCoopRoomAuthority()) {
        RecordCoopAttacker(actor->parent, ownClientId, THREAT_SHOT_FRAMES);
    }
    if (shot->hasEnded) {
        shot->owner = ownClientId;
    }

    std::vector<u8> state;
    for (const CoopStateField& field : adapter->stateFields) {
        u8* data = (u8*)actor + field.offset;
        state.insert(state.end(), data, data + field.size);
    }

    nlohmann::json payload = PrepCoopPayload(COOP_SHOT_STATE);
    payload["shotId"] = shot->id;
    payload["owner"] = shot->owner;
    payload["target"] = shot->target;
    payload["pos"] = actor->world.pos;
    payload["rot"] = actor->world.rot;
    payload["speedXZ"] = actor->speedXZ;
    payload["state"] = state;
    payload["ended"] = shot->hasEnded;
    payload["quiet"] = true;
    for (uint32_t clientId : GetCoopPeers(true)) {
        payload["targetClientId"] = clientId;
        SendJsonToRemote(payload);
    }
}

void Anchor::HandlePacket_CoopShotState(nlohmann::json payload) {
    if (!IsCoopPayloadForThisScene(payload)) {
        return;
    }

    uint32_t shotId = payload.at("shotId").get<uint32_t>();
    for (s32 i = 0; i < ACTORCAT_MAX; i++) {
        for (Actor* actor = gPlayState->actorCtx.actorLists[i].head; actor != NULL; actor = actor->next) {
            CoopShot* shot = ObjectExtension::GetInstance().Get<CoopShot>(actor);
            const CoopShotAdapter* adapter = shot != nullptr ? FindCoopShotAdapter(actor) : nullptr;
            if (adapter == nullptr || shot->id != shotId || actor->update == NULL) {
                continue;
            }
            if (shot->hasEnded) {
                return;
            }

            std::vector<u8> state = payload.at("state").get<std::vector<u8>>();
            size_t stateOffset = 0;
            for (const CoopStateField& field : adapter->stateFields) {
                if (stateOffset + field.size <= state.size()) {
                    memcpy((u8*)actor + field.offset, state.data() + stateOffset, field.size);
                }
                stateOffset += field.size;
            }
            actor->world.pos = payload.at("pos").get<Vec3f>();
            actor->world.rot = payload.at("rot").get<Vec3s>();
            actor->speedXZ = payload.at("speedXZ").get<f32>();
            // A teammate sent it back at the boss, which is ours to run now
            uint32_t owner = payload.at("owner").get<uint32_t>();
            if (owner == ownClientId && shot->owner != ownClientId && actor->parent != nullptr) {
                RecordCoopAttacker(actor->parent, payload.at("clientId").get<uint32_t>(), THREAT_SHOT_FRAMES);
            }
            shot->owner = owner;
            shot->target = payload.at("target").get<uint32_t>();
            shot->lastStateFrame = gPlayState->gameplayFrames;

            if (payload.at("ended").get<bool>()) {
                shot->hasEnded = true;
                adapter->onEnded(actor, IsCoopRoomAuthority());
                Actor_Kill(actor);
            }
            return;
        }
    }
}

// The shot copies of a projectile get, see HandlePacket_CoopEnemyProjectile
static void SetCoopShot(Actor* actor, uint32_t id, uint32_t target, uint32_t owner) {
    if (FindCoopShotAdapter(actor) != nullptr) {
        ObjectExtension::GetInstance().Set<CoopShot>(actor, CoopShot{ id, owner, target, gPlayState->gameplayFrames });
    }
}

static bool IsOwnCoopShot(Actor* actor) {
    const CoopShot* shot = ObjectExtension::GetInstance().Get<CoopShot>(actor);
    return shot != nullptr && shot->owner == Anchor::Instance->ownClientId;
}

// MARK: - Projectiles

// Something spawned while a mirrored enemy we run updates, like an Octorok's rock or a Mad Scrub's nut
void Anchor::OnCoopActorSpawned(Actor* actor) {
    // What our player spawns is ours, and so is what that spawns in turn (a Deku Nut's flash, an arrow's fire). Its
    // hits on puppets go to the authority like our sword's, it no longer has our player as its parent once let go.
    if (coopUpdatingActor != nullptr && coopUpdatingActor != actor &&
        (coopUpdatingActor == &GET_PLAYER(gPlayState)->actor ||
         ObjectExtension::GetInstance().Has<CoopOwnedByUs>(coopUpdatingActor))) {
        ObjectExtension::GetInstance().Set<CoopOwnedByUs>(actor, CoopOwnedByUs{});
    }

    // Not what a boss spawns in its intro or defeat (doors, blue warp, heart container), every player has their own
    if (coopUpdatingActor == nullptr || coopUpdatingActor == actor || actor->id == ACTOR_EN_ITEM00 ||
        actor->id == ACTOR_EN_ELF || !IsCoopMirroredEnemy(coopUpdatingActor) || !IsMirroredNow(coopUpdatingActor) ||
        !IsCoopRoomAuthority()) {
        return;
    }

    // Enemies it spawns are numbered, so teammates can spawn their own puppets of them (see SpawnCoopPuppet). The ones
    // with a slot of their own already (small Floormasters) keep it.
    const CoopEnemyAdapter* spawnedAdapter = FindCoopEnemyAdapter(actor);
    if (spawnedAdapter != nullptr &&
        (actor->category == ACTORCAT_ENEMY || spawnedAdapter->puppetSpawn != COOP_PUPPET_SPAWN_NONE)) {
        if (!IsCoopEnemy(actor)) {
            CoopChildCount* childCount = ObjectExtension::GetInstance().Get<CoopChildCount>(coopUpdatingActor);
            s16 slot = (childCount != nullptr ? childCount->count : 0) + 1;
            ObjectExtension::GetInstance().Set<CoopChildCount>(coopUpdatingActor, CoopChildCount{ slot });
            SetCoopChildSlot(actor, coopUpdatingActor, slot);
        }
        return;
    }

    ObjectExtension::GetInstance().Set<CoopProjectile>(actor, CoopProjectile{ false });
    coopPendingProjectiles.push_back(actor);

    // A shot to play back is run by the player it flies at, which is who the boss goes after
    const CoopEnemyTarget* bossTarget = ObjectExtension::GetInstance().Get<CoopEnemyTarget>(coopUpdatingActor);
    uint32_t target = (bossTarget != nullptr && bossTarget->clientId != 0) ? bossTarget->clientId : ownClientId;
    SetCoopShot(actor, ((uint32_t)ownClientId << 20) | (coopNextShotId++ & 0xFFFFF), target,
                FindCoopShotAdapter(actor) != nullptr && FindCoopShotAdapter(actor)->isFlyingAtPlayer(actor)
                    ? target
                    : ownClientId);
}

// Shared at the end of the frame, once they have their starting velocity
void Anchor::FlushCoopPendingProjectiles() {
    if (coopPendingProjectiles.empty()) {
        return;
    }

    std::vector<uint32_t> peers = GetCoopPeers(true);
    for (Actor* projectile : coopPendingProjectiles) {
        bool isAlive = false;
        for (Actor* actor = gPlayState->actorCtx.actorLists[projectile->category].head; actor != NULL;
             actor = actor->next) {
            isAlive |= actor == projectile && actor->update != NULL;
        }
        if (!isAlive || peers.empty()) {
            continue;
        }

        nlohmann::json payload = PrepCoopPayload(COOP_ENEMY_PROJECTILE);
        payload["actorId"] = projectile->id;
        payload["params"] = projectile->params;
        payload["pos"] = projectile->world.pos;
        payload["rot"] = projectile->shape.rot;
        payload["worldRot"] = projectile->world.rot;
        payload["velocity"] = projectile->velocity;
        payload["speedXZ"] = projectile->speedXZ;
        payload["gravity"] = projectile->gravity;
        const CoopShot* shot = ObjectExtension::GetInstance().Get<CoopShot>(projectile);
        if (shot != nullptr) {
            payload["shotId"] = shot->id;
            payload["shotTarget"] = shot->target;
            payload["shotOwner"] = shot->owner;
        }
        // Some need the one that shot them (Volvagia's rocks)
        if (projectile->parent != nullptr && (IsCoopEnemy(projectile->parent) || IsCoopBoss(projectile->parent))) {
            payload["parent"] = GetCoopActorIdentity(projectile->parent);
        }
        for (uint32_t clientId : peers) {
            payload["targetClientId"] = clientId;
            SendJsonToRemote(payload);
        }
    }

    coopPendingProjectiles.clear();
}

void Anchor::HandlePacket_CoopEnemyProjectile(nlohmann::json payload) {
    if (!IsCoopPayloadForThisScene(payload) || payload.at("roomNum").get<s8>() != gPlayState->roomCtx.curRoom.num ||
        IsCoopRoomAuthority()) {
        return;
    }

    Vec3f pos = payload.at("pos").get<Vec3f>();
    Vec3s rot = payload.at("rot").get<Vec3s>();
    s16 actorId = payload.at("actorId").get<s16>();
    s16 params = payload.at("params").get<s16>();
    Actor* projectile;
    if (payload.contains("parent")) {
        // Not without our copy of the one that shot it, it may rely on it
        Actor* parent = FindCoopActor(payload["parent"].get<CoopActorIdentity>());
        if (parent == nullptr) {
            return;
        }
        // It doesn't take the place of the parent's own child (Phantom Ganon's horse)
        Actor* parentChild = parent->child;
        projectile = Actor_SpawnAsChild(&gPlayState->actorCtx, parent, gPlayState, actorId, pos.x, pos.y, pos.z, rot.x,
                                        rot.y, rot.z, params);
        parent->child = parentChild;
    } else {
        projectile =
            Actor_Spawn(&gPlayState->actorCtx, gPlayState, actorId, pos.x, pos.y, pos.z, rot.x, rot.y, rot.z, params);
    }
    if (projectile == nullptr) {
        return;
    }

    ObjectExtension::GetInstance().Set<CoopProjectile>(projectile, CoopProjectile{ true });
    if (payload.contains("shotId")) {
        SetCoopShot(projectile, payload["shotId"].get<uint32_t>(), payload["shotTarget"].get<uint32_t>(),
                    payload["shotOwner"].get<uint32_t>());
    }
    projectile->world.rot = payload.at("worldRot").get<Vec3s>();
    projectile->velocity = payload.at("velocity").get<Vec3f>();
    projectile->speedXZ = payload.at("speedXZ").get<f32>();
    projectile->gravity = payload.at("gravity").get<f32>();
}

// Copies of projectiles fly and break like the real one, but can't hit anything: the real one does the hitting
void Anchor::DisarmCoopProjectileCopy(Actor* actor) {
    // A shot we run hits for real (see UpdateCoopShot)
    const CoopProjectile* projectile = ObjectExtension::GetInstance().Get<CoopProjectile>(actor);
    if (projectile == nullptr || !projectile->isCopy || IsOwnCoopShot(actor)) {
        return;
    }

    CollisionCheckContext* colChkCtx = &gPlayState->colChkCtx;
    s32 count = 0;
    for (s32 i = 0; i < colChkCtx->colATCount; i++) {
        if (colChkCtx->colAT[i]->actor != actor) {
            colChkCtx->colAT[count++] = colChkCtx->colAT[i];
        }
    }
    colChkCtx->colATCount = count;
}

// MARK: - Attacks on teammates

// Called by a teammate's dummy player when it's hit by a mirrored enemy that we run
void Anchor::SendPacket_CoopEnemyAttack(uint32_t clientId, Actor* attacker, ColliderInfo* attackInfo) {
    nlohmann::json payload = PrepCoopPayload(COOP_ENEMY_ATTACK);
    payload["targetClientId"] = clientId;
    payload["damage"] = attackInfo->toucher.damage;
    payload["effect"] = attackInfo->toucher.effect;
    payload["attackerPos"] = attacker->world.pos;

    SendJsonToRemote(payload);
}

void Anchor::HandlePacket_CoopEnemyAttack(nlohmann::json payload) {
    if (!IsCoopPayloadForThisScene(payload)) {
        return;
    }

    Player* self = GET_PLAYER(gPlayState);

    // Same exceptions as an enemy hit in vanilla: not while invincible, in a cutscene or getting an item
    if (self->invincibilityTimer != 0 || Player_InBlockingCsMode(gPlayState, self) ||
        self->stateFlags1 & (PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_GETTING_ITEM)) {
        return;
    }

    u8 effect = payload.at("effect").get<u8>();
    s32 response = PLAYER_HIT_RESPONSE_KNOCKBACK_SMALL;
    if (effect == 1) {
        for (s32 i = 0; i < ARRAY_COUNT(self->bodyFlameTimers); i++) {
            self->bodyFlameTimers[i] = static_cast<u8>(Rand_S16Offset(0, 200));
        }
        self->bodyIsBurning = true;
    } else if (effect == 2) {
        response = PLAYER_HIT_RESPONSE_FROZEN;
    } else if (effect == 3) {
        response = PLAYER_HIT_RESPONSE_ELECTRIFIED;
    }

    // Enemy damage is in health units already (0x10 per heart), unlike the PvP damage in DAMAGE_PLAYER
    Vec3f attackerPos = payload.at("attackerPos").get<Vec3f>();
    self->actor.colChkInfo.damage = payload.at("damage").get<u8>();
    func_80837C0C(gPlayState, self, response, 4.0f, 5.0f, Math_Vec3f_Yaw(&attackerPos, &self->actor.world.pos), 20);
}

// MARK: - Debugging

// Shown in the Actor Viewer
std::string Anchor::GetCoopActorDebugInfo(Actor* actor) {
    if (!IsCoopSyncActive()) {
        return "";
    }

    CoopActorIdentity identity = GetCoopActorIdentity(actor);
    std::string info = "Co-op id: " + std::to_string(identity.actorId) + " (" + std::to_string(identity.x) + ", " +
                       std::to_string(identity.y) + ", " + std::to_string(identity.z) + ")";

    if (IsCoopMirroredEnemy(actor)) {
        CoopPuppet* puppet = ObjectExtension::GetInstance().Get<CoopPuppet>(actor);
        CoopEnemyTarget* target = ObjectExtension::GetInstance().Get<CoopEnemyTarget>(actor);
        if (!IsMirroredNow(actor)) {
            info += "\nMirrored, but not now (boss intro or defeat, run by every player)";
        } else if (IsCoopRoomAuthority()) {
            info += "\nMirrored, run by us";
            if (target != nullptr && target->clientId != 0) {
                info += ", going after client " + std::to_string(target->clientId);
            }
        } else if (GetCoopRoomAuthority() == 0) {
            info += "\nMirrored, but nobody runs the room yet";
        } else if (puppet != nullptr &&
                   gPlayState->gameplayFrames - puppet->lastSnapshotFrame <= PUPPET_TIMEOUT_FRAMES) {
            info += "\nMirrored, puppet of client " + std::to_string(GetCoopRoomAuthority());
        } else {
            info += "\nMirrored, but running its own AI (no updates from client " +
                    std::to_string(GetCoopRoomAuthority()) + ")";
        }
    } else if (IsCoopEnemy(actor)) {
        info += "\nShared health and death";
    }

    return info;
}

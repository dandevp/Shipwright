# C++ logic changes not yet in the apworld

The apworld's logic (`archipelago/oot_soh/location_access`, `LogicHelpers.py`) was ported from SoH's C++ logic as of
roughly `1de22d816` (2026-05-30, where the official 1.4.x client branched off develop). These are the commits that
touched `randomizer/location_access/**` or `randomizer/logic.{cpp,h}` since then
(`git log 1de22d816..HEAD -- <those paths>`).

Some may already be in the apworld, since 1.4.2 was released 2026-07-02: check before porting.

Tags:
- **fix**: logic fix for something the apworld already has. Port it.
- **feature**: logic for a setting the apworld doesn't have yet. Port it with that setting's phase in `ARCHIPELAGO_PORT.md`.
- **mq**: Master Quest only (Phase 4), or the MQ part of a mixed commit.
- **none**: no logic change (cleanup, renames, includes, text).

`lines` counts the changed lines in those paths; `mq` how many of them mention MQ.

| status | commit | lines | mq | tag | subject |
|---|---|---|---|---|---|
| [ ] | ea76550fc | 6 | 0 | fix | Fix owl talk logic to include kokiri (#6564) |
| [ ] | 5b4d8edf5 | 1 | 0 | fix | fix bad merge with the suns song fairy in spirit (#6590) |
| [ ] | 80d3114f7 | 4 | 0 | fix | Fix logic placing item on saving carpenters when carpenters start free (#6661) |
| [ ] | abcb3ad94 | 2 | 0 | fix | adult-only crate (#6674) |
| [ ] | ec27775e8 | 147 | 0 | fix | Fix Logic issues in DMC > Fire Temple and DMC wall PoH (#6679) |
| [x] | 8a14cd109 | 2 | 0 | none | Move to port-maintainance LUS branch to fix crash on close (#6629) |
| [ ] | 91769a0e3 | 2 | 0 | fix | boss key hint typo (#6583) |
| [ ] | 864908586 | 39 | 0 | feature | add option gating shields / tunics in shop behind finding one first (#6700) |
| [x] | 17f598377 | 4 | 0 | none | Merge remote-tracking branch 'origin/develop' into develop-ackbar |
| [ ] | 8b7109f60 | 89 | 3 | fix | Hard Code Small Key Doors (#6578) |
| [ ] | 7c42a63ce | 3 | 0 | fix | Add the 2 "decoy" crates as NL crates (#6672) |
| [ ] | 7b01a7bf2 | 50 | 0 | feature | Early Granny's Potion Shop (#6693) |
| [ ] | 041295f05 | 340 | 0 | fix | Fix Guard pots being breakable with hookshot in logic (#6671) |
| [ ] | adf31d5eb | 4 | 0 | fix | Fix Bottom of the Well Coffin Logic (#6743) |
| [ ] | 46f775ef8 | 16 | 0 | fix | fix a couple of small DMC oversights (#6784) |
| [ ] | ec2a3d7aa | 149 | 3 | fix | Fix numerous Small Key Tracking bugs (#6750) |
| [ ] | c5896c7ef | 128 | 0 | none? | Combine HasBossSoul & CanOpenOverworldDoor into HasItem (#6795): refactor, check behavior |
| [ ] | c238f0745 | 13 | 0 | none? | fix regression, lost boss souls from HasItem (#6798): follow-up of the above |
| [ ] | 210c5d588 | 2 | 0 | fix | fix oversight on rising target ledge > pillar high water (#6818) |
| [x] | 633e92096 | 1 | 0 | none | Cleanup unused includes (#6813) |
| [x] | 8d2d5c982 | 18 | 0 | none | Be consistent with check names for deku scrub grotto beehives (#6825): renamed in f8c1631d5 |
| [ ] | 1fd3a467c | 3 | 0 | feature | Add Kakariko watchtower butterfly fairy (#6839): butterfly fairies, Phase 3 |
| [ ] | ed4868f62 | 9 | 0 | feature | Check for the Wolfos Gate before letting child out of SFM without climb (#6851): climb, Phase 2 |
| [ ] | c76d17552 | 7 | 4 | mq | DC MQ: RT_VISIBLE_COLLISION to reach above stairs with hookshot (#6840) |
| [x] | 015440fdf | 115 | 0 | feature | Decouple GBK from LACS (#5838): Ganon's Boss Key triggers, Phase 1 |
| [ ] | 1ae55a0a7 | 2 | 0 | feature | Add water switch signs to shuffle signs (#6876): signs, Phase 3 |
| [x] | 29d4ff420 | 18 | 0 | none | Decomp, update AudioOcarina (#6941) |
| [ ] | d3dc95d8f | 50 | 29 | none? | cleanup region names (#6943): region names only matter if the apworld shares them |
| [ ] | 1ea720607 | 40 | 0 | feature | Shuffle Zelda's Letter (#6938) |
| [ ] | 848106039 | 34 | 26 | mq | fix climb requirements for MQ DC Torch room (#6950) |
| [ ] | da4e6dc33 | 207 | 74 | feature | Progressive Shuffle Open Chest (#6971): Phase 2 |
| [ ] | 581af4741 | 6 | 2 | fix | Fix typos (#6978) |
| [ ] | 688913270 | 8 | 6 | mq | Fix potential key softlock with hover boosts in MQ spirit (#7014) |
| [ ] | 7be4a5941 | 5 | 0 | none? | Added small key null checks (#7019) |
| [ ] | b76e303ce | 766 | 249 | feature | Shuffle Silver (#5659): Phase 3 |
| [ ] | 6706b49d1 | 28 | 0 | feature | Shuffle Treasure Chest Game (#7020): Phase 3 |
| [ ] | 29668a06d | 18 | 0 | feature | Progressive BGS (#7037): Progressive Goron Sword, Phase 1 |
| [ ] | b52c31f85 | 625 | 26 | fix | Get rid of Logic::Hearts, update Logic::EffectiveHealth to be 16/heart (#7026) |
| [x] | 490e13616 | 4 | 0 | none | fix warnings (#7051) |
| [x] | f35f134c9 | 18 | 0 | none | Fix grammar (#7053) |
| [ ] | 55b52a26a | 271 | 0 | fix | replace specialized bean logic with events (#5842) |
| [ ] | 0bae4961a | 9 | 0 | none? | Draw keys on keyring with code, consolidate dungeon key counting code (#7087) |
| [x] | 7a7f942b8 | 1 | 0 | none | Fix item tracker fishing pole & roc's feather (#7103) |
| [ ] | 47d138302 | 2 | 2 | mq | MQ DC: standing in corner slingshot can hit through eye through boulder (#7109) |
| [x] | ce969cab9 | 10 | 0 | none | clean up includes (#7114) |
| [x] | 78e3683ef | 8 | 0 | fix | Side room pot 5 and 6 logic fix (#7107): ported in f8c1631d5 |
| [ ] | cc63cc4ab | 15 | 0 | fix | fix logic bugs with Bombchus (#7117) |
| [ ] | 1413ceb33 | 8 | 0 | fix | Fixed carpenter guard hylian jabbernut requirement (#7121) |
| [ ] | 2ba5c40da | 3 | 0 | fix | Bombchu logic fix: don't allow bombchus in single bag without bag (#7134) |
| [ ] | 3175ea9c2 | 53 | 0 | feature | Remove completed mask quest option, add all masks to starting items menu (#7104): Phase 1 masks |
| [ ] | eca767dcb | 1682 | 518 | feature | Logical bunny hood (#7032): Bunny Hood option, Phase 1. Check whether it changes logic with the option off |
| [ ] | 73a739b7a | 35 | 6 | fix | Fix various trick issues (#7145) |
| [x] | 495a2b17d | 9 | 0 | none | revise randomizerEnumStrings (#7148) |
| [ ] | f04b570d2 | 2 | 1 | fix | Fix logic typo (#7181) |
| [ ] | 53f688df5 | 68 | 22 | fix | Fix logic for fire near boss (#7147) |
| [x] | 060787a23 | 1 | 0 | none | clean up headers (#7194) |
| [ ] | 31ba9e206 | 6 | 3 | fix | fix fire boss typo (#7195) |
| [ ] | a353b6e7c | 8 | 0 | feature | Shuffle Scarecrow Song (#7205): Phase 3 |
| [x] | 5fee87c23 | 144 | 6 | none | Replace tabs with spaces (#7223) |
| [ ] | 8fc1fc869 | 6 | 0 | none? | fix preplanted beans without souls being seen as planted on the tracker (#7251) |
| [ ] | ff0209e76 | 30 | 12 | fix | fix logical typos (#7283) |
| [x] | 94f950f8d | 293 | 84 | none | German (#7315): translations |
| [ ] | f3d881144 | 2 | 0 | fix | Fix water entrance graph typo (#7324) |

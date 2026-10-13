# Archipelago port

Branch `archipelago_port`. Goal: everything the internal randomizer can do, Archipelago can do too.

The work has two sides, and every feature needs both:

- **apworld** (Python, `archipelago/oot_soh/`): the options, items, locations, region logic, item pool and
  `fill_slot_data`. Baseline: an unchanged copy of `oot_soh.apworld` 1.4.2 (commit `fe52dddb2`).
- **client** (C++, this repo): `Context::ParseArchipelagoOptions` (`soh/soh/Enhancements/randomizer/SeedContext.cpp`)
  turns the slot data into `RSK_*` options. The game then runs the internal randomizer's in-game code with those options.
  Locations are matched by name (`StaticData::locationNameToEnum`); items by name (`StaticData::itemNameToEnum`).

## How to port one feature

1. **Source of truth**: the C++ side. Option definition in `randomizer/settings.cpp`; locations in
   `randomizer/location_list.cpp` or the feature's `Shuffle*.cpp`; logic in `randomizer/location_access/**`
   and `randomizer/logic.cpp`; item pool in `randomizer/3drando/item_pool.cpp`.
2. **apworld**:
   - `Options.py`: add the option class and add it to `SohOptions`. Match the C++ option values, or document the
     mapping.
   - `Enums.py`: location, item and region names. A name must be **exactly** the C++ `GetName()`, or the client
     silently maps it to `RC_UNKNOWN_CHECK`.
   - `Locations.py`: `SohLocData(id, tags)`. Add new ids after the highest existing one; never renumber.
   - `Items.py` / `ItemPool.py`: new items and their pool rules.
   - `location_access/**.py`: rules ported from the matching `location_access/**.cpp`.
   - `LogicHelpers.py`: helpers ported from `logic.cpp`.
   - `__init__.py` `fill_slot_data`: send the option.
3. **client**: read the key in `ParseArchipelagoOptions` with `slotData.value("key", default)`, so that a 1.4.x
   apworld without the key keeps working.
4. Note it in this file and in `features.md`.

## Compatibility rules

- New options go in `slot_data_options_15` (`Options.py`): sent in slot data under their own name, and read back
  by Universal Tracker with their default when a 1.4 seed doesn't have them.
- The new apworld becomes world version **1.5.0** (`archipelago.json`). The client accepts 1.4.x **and** 1.5.x:
  every key added in 1.5 is read with a default that means "behave like 1.4".
- Never renumber existing location or item ids. AP datapackages and running multiworlds depend on them.
- Existing option values keep their numbers. Add new values at the end. The rainbow bridge already shows what renumbering
  costs: see the `RO_BRIDGE_TRIFORCE_PIECES` shift in the client.

## Verification

- apworld: generate with a local Archipelago checkout (0.6.7+), with `archipelago/oot_soh` copied or symlinked into
  `worlds/`. Use one YAML per feature, with the feature on and off. Also check that `python -m unittest` in
  Archipelago `test/` passes for the world.
- client: build (**ask before building**), connect to a local `MultiServer.py` with the generated seed, and check
  that the new checks show up in the check tracker and send to the server.
- Name check (Phase 0): the client logs every scouted AP location / item name that doesn't resolve.

---

## Phase 0: groundwork

- [x] Commit the pending work, create the `archipelago_port` branch
- [x] Copy the 1.4.2 apworld source into `archipelago/oot_soh` (unchanged baseline)
- [x] Set up a local Archipelago checkout + venv (outside the repo) that can generate with `archipelago/oot_soh`:
      `~/code/ship/Archipelago` at 0.6.8, Python 3.12 venv; `archipelago/tools/generate.sh` + `tools/yamls/`
- [x] Client: fix `boss_key_shuffle == 0` setting `RSK_KEYSANITY` instead of `RSK_BOSS_KEYSANITY`
      (`SeedContext.cpp`, ParseArchipelagoOptions)
- [x] Client: read every slot data key with `.value(key, default)` instead of `operator[]`
- [x] Client: accept apworld 1.4.x and 1.5.x (`AP_WORLD_VERSION_*` in `Archipelago.h`, check in `Archipelago.cpp`)
- [x] Client: log the scouted location and item names that don't resolve (`ParseArchipelagoItemsLocations`);
      unknown locations are skipped instead of becoming `RC_UNKNOWN_CHECK`
- [x] Tool `archipelago/tools/check_names.py`: rebuilds the game's location names (area prefix + short name) and item
      names from the C++ sources and checks every apworld location and item against them (`--missing` lists what the
      apworld doesn't have). Fixed what it found: 5 locations renamed in develop (ZR Deku Scrub Grotto Beehive,
      DC Near Lizalfos Room Pots, Kak/LLR Tree, HC NL Tree Near Guards) and the Chest Game key items
- [x] apworld: bump `world_version` to 1.5.0 (the copy said 1.4.1 although the file was released as 1.4.2)
- [ ] Logic drift: port the C++ logic fixes made since the apworld was written. Triaged commit by commit in
      `archipelago/LOGIC_DRIFT.md` (62 commits since `1de22d816`). The `feature` and `mq` ones go with their phase.

## Phase 1: options without new locations

Mostly slot data plus a little logic.

- [x] Ganon's Boss Key: Own Dungeon, Start With, Any Dungeon, Overworld (`ganons_castle_boss_key` 8-11). The LACS
      values are triggers like in the game since 015440fdf: the key is a logic event, and the Light Arrow Cutscene is a
      normal check with its vanilla rule (adult + Shadow + Spirit).
- [x] Client: the trigger checks (`RC_GANONS_BOSS_KEY`, `RC_GANON_SOUL`, `RC_WINCON`) and `RC_GANON` get their fixed
      item locally (`PlaceArchipelagoTriggerItems`), as `item_pool.cpp` does. Before, they held nothing on an AP save: a
      1.4 LACS seed never gave the boss key from its trigger, and a Triforce Hunt never warped to the credits.
- [x] Win condition: `win_condition` (Defeat Ganon, Anywhere = the Triforce as an item, Trigger-Stones to
      Trigger-Tokens) with counts and Greg modifier. Numbered like `RO_WINCON_*`. Triforce Hunt still overrides it.
- [x] Ganon's Soul: `ganons_soul` (With Boss Souls = 1.4 behavior, Start With, Any Dungeon, Overworld, Anywhere,
      Trigger-Stones to Trigger-Tokens) with counts and Greg modifier. Independent of Shuffle Boss Souls, like the game.
- [ ] Triforce pieces without a Triforce Hunt win: Rainbow Bridge Triforce Pieces (`RSK_RAINBOW_BRIDGE_TRIFORCE_COUNT`),
      Ganon's Boss Key / Ganon's Soul / win condition Trigger-Triforce Pieces, and where the pieces go
      (`RSK_TRIFORCE_HUNT_PIECES_LOCATION`). Needs the pieces created whenever any option uses them, and the client
      setting `RSK_TRIFORCE_HUNT_PIECES_TOTAL` outside of the Triforce Hunt case.
- [x] Ganon's Trials: Random Number (`ganons_trials` 2, the apworld picks 0-6). Client fix: it read `skip_ganons_trials`,
      which no apworld sends; trials now follow `required_trials`
- [x] Link's Pocket reward type (`links_pocket_reward`)
- [ ] Starting inventory: every `RSK_STARTING_*` item. apworld: precollect the items and send them in slot data;
      client: set the options.
- [ ] Starting Skulltula tokens (`RSK_STARTING_SKULLTULA_TOKEN`)
- [ ] Masks one by one, instead of "complete mask quest" giving all of them
- [x] ~~Skip Child Stealth~~: dummied out in the game's settings (`settings.cpp:1344`), the time saver covers it
- [ ] Skip Planting Beans: port it together with the bean events commit 55b52a26a (`LOGIC_DRIFT.md`); the apworld
      still has the old per-patch bean logic
- [x] Early Granny's Shop (`early_grannys_shop`; the shop's rule now also matches the C++ one with trade items off:
      Claim Check or this option), Swordless Epona Items (`swordless_epona_items`, no logic)
- [ ] Masks: port the mask rework (3175ea9c2: masks are items, "complete mask quest" became starting masks), then
      Masks as Adult and logical Bunny Hood (eca767dcb). The apworld still borrows masks through mask shop events.
- [ ] Progressive Goron Sword, shop shields and tunics only refill
- [ ] All Checks Reachable (AP has `accessibility`; map `full` onto it)
- [ ] Fishsanity: Hyrule Loach only, pond fish count, child/adult split
- [x] Ice trap names (`ice_trap_names`). Client: options are now parsed before the items, which read this one
- [ ] Merchant prices (forced to vanilla); shop and scrub price modes, compared with the apworld's min/max prices
- [ ] Hints: warp song hints, Hyrule Loach hint
- [ ] Excluded locations: send AP's `exclude_locations` so the client marks them

## Phase 2: ability shuffles

New items plus logic helpers. Swim is the model to follow, both in the apworld and the client.

- [ ] Climb (`RSK_SHUFFLE_CLIMB`)
- [ ] Crawl (`RSK_SHUFFLE_CRAWL`)
- [ ] Grab (`RSK_SHUFFLE_GRAB`)
- [ ] Speak (`RSK_SHUFFLE_SPEAK`)
- [ ] Open Chest, On / Progressive (`RSK_SHUFFLE_OPEN_CHEST`)

## Phase 3: new check types

Each needs an option, locations with exact names, region rules ported from C++, items where needed, slot data and
the client mapping.

- [ ] Bushes (`ShuffleGrass.cpp` / `RSK_SHUFFLE_BUSHES`, forced off now)
- [ ] Rocks (`ShuffleRocks.cpp`)
- [ ] Boulders (`ShuffleRocks.cpp`, Off/Dungeons/Overworld/All)
- [ ] Silver rupees (`ShuffleSilver.cpp`, On/Wallet/Start With; logic in `logic.cpp`)
- [ ] Signs (`ShuffleSigns.cpp`)
- [ ] Icicles (`ShuffleIcicles.cpp`)
- [ ] Red ice (`ShuffleRedIce.cpp`)
- [ ] Wonder items (`ShuffleWonderItems.cpp`)
- [ ] Butterfly fairies (`ShuffleFairies.cpp`)
- [ ] Beggar (`ShuffleBeggar.cpp`)
- [ ] Treasure Chest Game (`ShuffleTreasureChestGame.cpp`) + its key ring (`RSK_KEYRINGS_CHEST_GAME`)
- [ ] Bean souls (`RSK_SHUFFLE_BEAN_SOULS`)
- [ ] Scarecrow's Song shuffle (`RSK_SHUFFLE_SCARECROWS_SONG`)

## Phase 4: Master Quest

The apworld has no MQ logic at all; the C++ files have it all.

- [ ] Option(s): MQ count / random / set / per dungeon. Send the chosen dungeons in slot data.
- [ ] Client: set `RSK_MQ_*` and the dungeons' quest from slot data
- [ ] MQ locations in `Enums.py` / `Locations.py` (added only for the MQ dungeons of a seed)
- [ ] MQ region logic for all 12 dungeons, ported from `location_access/dungeons/*.cpp`

## Phase 5: entrance randomizer

The largest phase. It uses Archipelago's generic entrance randomization (`entrance_rando.py`, GER).

- [ ] Model the shuffleable entrances as AP `Entrance`s with matching names / C++ entrance ids
- [ ] Pools: dungeons, boss rooms, Ganon's Tower, overworld, interiors, Thieves' Hideout, grottos, owl drops,
      warp songs, overworld spawns; mixed pools; decoupled
- [ ] Slot data: the entrance pairs (C++ ids); client: apply them with `mEntranceShuffler` instead of
      `UnshuffleAllEntrances`
- [ ] Logic: age/time access across shuffled entrances (`RegionAgeAccess.py`)
- [ ] Entrance tracker works for AP saves

---

## Log

Newest first. One line per commit or decision.

- 2026-10-14: trials Random Number, Link's Pocket reward, ice trap names; fixed the trials mapping my `.value()`
  change had broken (every AP seed skipped the trials). Compared all slot data keys: client and apworld now agree.
- 2026-10-13: Win condition and Ganon's Soul options. Generated one multiworld with every value of both (20
  players, 11 min): events/items land where expected, legacy `on_plus_ganons` and Triforce Hunt still behave like 1.4.
- 2026-10-13: Ganon's Boss Key options + trigger items. All 12 GBK values generate (one multiworld, a player each)
  and land where expected. Client not compiled yet.
- 2026-10-13: name check tool + generate script; the apworld's names now all resolve in the game. The two DC pots
  moved to `DODONGOS_CAVERN_NEAR_LOWER_LIZALFOS` like in C++ (78e3683ef). Location ids unchanged.
- 2026-10-13: Phase 0 client groundwork: boss key fix, `.value()` slot data reads (`starting_hearts`
  defaults to 3, the rest to 0), 1.4/1.5 version check, unknown name logging, apworld 1.5.0. Compiles (Linux, build-cmake); not play-tested.
- 2026-10-13: branch created from `ci/windows-build` (`5d11222c7`); apworld 1.4.2 copied in as baseline (`fe52dddb2`).
  The client's mapping (`ParseArchipelagoOptions`) was compared with every `RSK_*` setting; the gaps are the phases above.

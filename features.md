# Anchor Co-op Sync

Co-op Sync (room setting "Co-op Sync (Experimental)", set by the room owner) shares the live state of rooms and
enemies between teammates, so they can play through the same areas together.

## Requirements

- Only players on the same team, with the same game version, sync with each other. They may play different seeds
  (see "Playing different seeds").
- They sync with each other while in the same scene. Vanilla and Master Quest dungeons are kept apart.
- Their gameplay mods have to match (see "Mods").
- Story progress (NPCs reacting to it) needs "Sync Items and Flags" on as well.
- An Archipelago save never syncs items and flags (Archipelago moves the items), whoever owns the room and whether or
  not its slot is connected: it neither sends nor takes items, checks, flags, entrances or team state. The room's
  "Sync Items and Flags" still applies to teammates on other saves, and Co-op Sync stays available. Teammates on an
  Archipelago save aren't marked as breaking item sync in the room window.
- The Archipelago client is the official 1.4.2 one (jeromkiller/Shipwright_archipellago, tag Client_1.4.2), on the
  Settings page of file select: a new file loads the slot's data first (with its progress shown), then starts. The game
  is reported won when the end credits begin, for both Ganon and Triforce Hunt.
- The client connects to slots generated with apworld 1.4.x (official) and 1.5.x (the port of the randomizer's
  settings in `archipelago/oot_soh`, see `ARCHIPELAGO_PORT.md`). Slot options missing from the slot data fall back to
  defaults instead of failing. Locations and items whose names the game doesn't know are reported in the Archipelago
  console and left out, instead of becoming an invalid check.
- On an Archipelago save, reaching a trigger (Ganon's Boss Key, Ganon's Soul, a Triforce Hunt win) gives its item like in
  a normal randomizer seed. With apworld 1.5, Ganon's Boss Key can also be in Ganon's Castle, any dungeon, the overworld
  or the starting items, and the Light Arrow Cutscene holds a random item.
- apworld 1.5 also has the game's win conditions (the Triforce as an item, or a trigger), Ganon's Soul placements and
  triggers, Early Granny's Shop and Swordless Epona Items.
- Saves of the official Archipelago client (1.4.x) are recognized as Archipelago saves, but like any randomizer save
  of another SoH version (theirs is 9.2.3) they're moved aside as outdated: start a new file on the same slot, the
  server sends its items and checked locations again.

## What is shared

### Scene and room state

- Switch, clear and temporary collectible flags, live for teammates in the same scene. Room scoped flags (switch flags
  0x38-0x3F, temp clear flags) are shared with teammates in the same room only.
- Boss fights in progress: a teammate entering a boss room after the fight started skips the intro.
- Entering a scene or room catches up with a teammate already there:
  - flags
  - destroyed grass, pots, crates, rocks and enemies
  - pushed blocks, turned statues and lit torches
  - the health of the enemies still alive
- Like in vanilla, a room resets once nobody is in it anymore.

### Grass, pots, crates and rocks

- Breaking one breaks it for everyone in the room, whether it was cut, blown up or thrown. Silver rocks included.
- Its drops are rolled once, by the player who broke it, and appear for everyone. Whoever picks a drop up first removes
  it for the others.
- Each drop is adapted to whoever gets it, like in vanilla (e.g. arrows become rupees without a bow).

### Enemies

- Enemies placed in a room share their health and death. When one dies it dies for everyone, with its drops shared.
- One player per room runs the enemies (the room authority), the others see puppets that follow the authority's game:
  position, rotation, health, animation and what their drawing depends on.
  - The authority is whoever was in the room first. A player entering a room where a fight is going on joins it,
    instead of the fight starting over.
  - If the authority pauses, another player in the room takes over the moment the pause menu opens. If it leaves the
    room or its game stops responding (lag, crash), after a second.
  - While a player is in the pause menu with a teammate in the room, their enemies, bosses and what those shoot keep
    following the teammate's game, so the fight goes on for them too and nothing jumps when they come back. Their own
    Link stays where they left him: enemies leave them alone meanwhile, and nothing hits them or is hit by them. A
    boss's defeat or cutscene waits until they're back. Alone in the room, the pause stops everything as usual.
  - A player coming back from a pause doesn't take over again, however short the pause.
  - Taking over, enemies go on from exactly where the authority's last were, standing (no leftover speed from before
    they followed the authority's). What they were doing comes from their own state, which may be older.
  - The bosses, Deku Babas, Flare Dancers and Baris go on with the authority's action too. One the authority is in
    that isn't passed on (cutscenes, deaths that spawn things) leaves the taking-over player's own action as it was,
    instead of putting in another one: the merged Twinrova got one of her sisters' that way and crashed the game.
- Puppets can be Z-targeted, and pulled in by the hookshot, whenever the authority's enemy can (an Iron Knuckle once
  it wakes up, a Deku Baba while it's out of the ground), and are only seen through the Lens of Truth when it is.
- Hitting a puppet hits the authority's enemy, with its own damage table, reaction and death. That goes for what you
  shoot, throw or set off too: arrows, slingshot seeds, bombs, and a Deku Nut's flash stunning it.
- On the authority, enemies go after the most dangerous player in the room: whoever dealt them the most damage. While
  nobody has hurt them yet, they go after the nearest player, and switch once another one is clearly closer. Their hits
  on a teammate hurt that teammate.
- What they shoot is shown to everyone, but only the authority's copy can hurt anyone.
- Mirrored enemies:
  - Armos, Baby Dodongo, Bari, Beamos, Biri, Bubble, Dead Hand and its hands, Dodongo
  - Flare Dancer (and its core), Floormaster, Freezard, Gerudo fighter, Gohma Larva (and its egg), Guay
  - Iron Knuckle, Keese, Lizalfos, Dinolfos, Mad Scrub, Moblin, Octorok, Poe, ReDead, Gibdo, Like-Like, Shell Blade
  - Skulltula, Skullwalltula, Spike, Stalchild, Stalfos, Stinger, Tektite, Torch Slug, Wallmaster
  - Wolfos, Deku Baba, Withered Deku Baba, Dark Link
- Enemies a mirrored enemy spawns during the fight (the Biris a Bari splits into, a Flare Dancer's core) are spawned
  for everyone too.
- A puppet that has no animation yet (a Flare Dancer still in the ground) isn't animated until the authority's has
  one, as the enemy itself does.
- A Deku Baba's Deku Stick and a Poe's soul appear for everyone, and are gone for everyone once one player takes them.

### Bosses

- Bosses share their health. A teammate's hit never takes it below 1, so every player lands their own final blow and
  gets their own defeat cutscene, heart container and blue warp.
- Mirrored bosses are run by the room authority during the fight, like mirrored enemies: everyone fights the same boss,
  in the same place, and hitting it hits the authority's boss.
  - Unlike enemy puppets, a boss puppet keeps its own effects, sounds and animation, and only follows what the
    authority's boss is doing.
  - It goes after the same player as the authority's boss: it turns, aims and charges at them in everyone's game.
    Like enemies, it goes after whoever dealt it the most damage (counting all its parts together), or the nearest
    player while nobody has. A shot sent back at it, a bomb fed to King Dodongo and a beam reflected onto a Twinrova
    sister count for the player who did it.
  - What a player does against its attack in their own game is shown in everyone's (a Twinrova sister's beam reflected
    by their Mirror Shield).
  - Its attacks (body, claws, beams, fire breath) hit you as you see them in your game. What it throws or drops
    (Volvagia's rocks, King Dodongo's fire) works like enemy projectiles.
  - Intro and defeat are cutscenes, so every player plays them with their own boss. A boss joins the shared fight once
    both its intro and the authority's are over.
  - When the authority's boss is defeated, it's defeated for everyone at once, so every player still gets their own
    defeat cutscene, heart container and blue warp.
  - What it spawns during the fight is spawned for everyone (Queen Gohma's eggs and larvae).
- Shots you play back at the boss (Phantom Ganon's and Ganondorf's energy balls, Ganondorf's big magic, Twinrova's
  blasts) are run by the game of the player they fly at: only there can they be hit back, absorbed or dodged. Once one
  flies back at the boss, the authority's game runs it, until the boss sends it at a player again. Everyone sees the
  same shot, and when it ends (bursting on a player, hitting the boss, hitting the floor) it ends for everyone.
  - When one comes back to you, your last hit on it is cleared, so it only bounces back when you hit it again (Phantom
    Ganon's ball took the hit that sent it back before for a new one, so it couldn't get past a player).
  - The player it flies at gets it with what the boss gave it, like whether a Twinrova blast is fire or ice. That
    decides the Mirror Shield charge: a blast of the other kind empties it, and the charged blast uses it up.
  - Everyone else sees it at its full size from the start.
- Reflecting one Twinrova sister's beam onto the other, and stunning Twinrova with a charged Mirror Shield blast, count
  for everyone, whoever does it.
- King Dodongo swallows a bomb thrown into his mouth by any player: the thrower's game decides it, and his choking on
  it is shared.
- Morpha's core can be pulled out by any player's hookshot: the puller's game moves it, and the authority's follows.
- While a boss holds you (Bongo Bongo's hand, Morpha's tentacle) or your hookshot holds it, it runs in your game by
  itself until it lets go, so you get away from it like in vanilla (hitting Morpha's core cuts her tentacle). A
  boss that starts a cutscene in your game during the fight (a Morpha tentacle keeps the camera on you until you've
  landed after it let go) runs in your game by itself until the cutscene is over, even if it's still holding a teammate
  in the authority's game.
- A Morpha tentacle grabs you in your game whenever you're within its reach, whoever it goes after.
- When a boss jumps to where the authority's is (like after coming back from a pause), Volvagia's body and mane move
  along with her head.
- Ganon being downed (and every player getting their Master Sword back) is a cutscene too, played by every player:
  when the authority's Ganon is downed, everyone's is. He joins the shared fight again once he's back up in your game.
- Every boss is mirrored: Queen Gohma, King Dodongo, Barinade, Phantom Ganon (his horse and fakes too), Volvagia,
  Morpha, Bongo Bongo, Twinrova, Ganondorf, Ganon.

### Puzzles and objects

- Torches: lighting one lights it for everyone.
- Signs: cutting a piece off a sign cuts the same piece off for everyone in the room, flying away from whoever cut it.
- Toggle floor switches (one press on, the next off, like the randomizer's Lake Hylia water switch): when a teammate
  toggles one, yours goes down or up to match. The flags the scene sets up from your save as it loads (like the water
  level you left Lake Hylia at) aren't sent to teammates, you take over those of teammates already there.
- Push blocks: pushed for everyone, one player at a time.
- Cobra mirror statues: turned for everyone, one player at a time.
- These moving platforms and traps are kept in step for everyone. Standing on one counts as standing on it for the
  authority too, so ones that move when stood on start for everyone.
  - Dodongo's Cavern rising platform
  - Fire Temple lift, moving platform and rotating flame statue
  - Forest Temple elevator and falling ceiling
  - Shadow Temple spinning scythe
  - Spirit Temple elevator
  - collapsing platforms

### Carrying

- Teammates see what you carry in your hands, flying when you throw it, landing where you put it down. That covers
  pots, bushes, crates, rocks and Cuccos placed in the room, as well as bombs, Bomb Flower bombs and anything else.
- A teammate's player model shows the same pose as in their own game, including upper body animations like holding
  something over their head (sent as drawn, with animation blending, one frame behind their position).
- A teammate's bomb blows up in your game when it does in theirs. It is only the look and sound: its effects happen in
  their game and reach you through the rest of the sync.
- Copies of a teammate's bombs don't count towards your own bomb limit.
- Teammates see what you shoot or throw while it flies: arrows (including magic arrows, without their fire, ice or light
  effect around them), slingshot seeds, Deku Nuts and the boomerang. Their copies follow yours each frame and can't
  hit anything; what yours hits happens in your game and reaches them through the rest of the sync. Where a seed or
  Deku Nut hits, they see and hear it pop, and a Deku Nut's white flash when they're close to it (not its stun).

### Time of day

- Teammates in the same scene share the time of day: the one whose clock is furthest along sets it for everyone. A
  Sun's Song reaches everyone that way, and a player coming out of a dungeon (where time stands still) or back from a
  pause catches up with the others. Clocks are only moved forward, by less than half a day, and not during cutscenes.

### NPCs

- When a teammate in your scene makes story progress, the NPCs in your room are respawned, so they react to it right
  away rather than when you come back (like Mido moving out of the way).

### Room owner's enemy settings

- While Co-op Sync is on, the room owner's Enemy Randomizer (mode and enemy list, including Air-Walk Voidouts),
  Randomized Enemy Sizes (and Size Scales Health) and Hyper Enemies / Hyper Bosses settings apply to everyone in the
  room, both ways: a setting the owner has off is off for everyone. Your own saved settings aren't changed and come back
  when you leave the room.
- Enemies are always seeded in co-op, with a seed the room keeps (the owner can roll a new one in the room settings).
  Randomized enemies and their sizes come out the same for everyone, so they're mirrored and share health like any
  other enemy.
- Changes apply to rooms as they load: enemies already spawned keep their type and size. Hyper Enemies and Hyper Bosses
  switch right away.

### Playing different seeds

- Teammates can each play their own world: a vanilla file, a randomizer seed or an Archipelago slot (players on the
  same slot are one world). Between different worlds, everything that doesn't touch either world's loot is shared:
  - enemies and bosses (mirroring, health, deaths), boss rewards and blue warps stay each player's own
  - grass, pots, crates and rocks breaking, but only those that don't hold a check in your seed: a shuffled pot of
    yours stays until you break it yourself, and doesn't follow a teammate who carries their copy of it
  - pushed blocks, turned statues, lit torches, moving platforms, carried objects
  - switch and clear flags, boss fights in progress
- Kept within a world: drops and who picked them up, temporary collectibles, and NPCs reacting to story progress.
- The room window marks teammates on a different seed. It only warns when "Sync Items and Flags" is on, which needs
  the same seed.

### Mods

- Mods that change more than looks and sound (scenes, actors, text, custom items, ...) have to match between teammates,
  else the rooms they share aren't the same. Texture packs, sound packs and Link models don't count (models are per
  player, see below). Teammates whose gameplay mods differ aren't synced with each other.
- Other model swaps don't count either: display lists, vertices, matrices and skeletons (binary or XML) are looks, so
  equipment packs, get item models, the Master Sword pedestal or Dark Link only change what you see, as do loose sound
  files and a mod's cosmetics editor entries. Scenes, collision, paths, cutscenes, text and animations do count. Empty
  and hidden files (editor leftovers) are skipped.
- Whoever's gameplay mods differ from the room owner's is asked once whether to match them: the gameplay mods the owner
  doesn't have are disabled, and the owner's ones that were disabled this way come back. SoH closes to apply it (the
  files are renamed to `*.o2r.disabled` and back at the next start). Mods the owner has that you don't have at all are
  listed, to get from them.
- Network > Anchor > Your Link Model > Restore Mods Disabled for Co-op brings them all back at the next start.

### Player models

- Teammates' dummy players are drawn with their own Link model and equipment (a custom model, a sword replacement,
  ...) while you keep yours, and with vanilla Link when they have none, never with your model.
- A model is everything under `objects/object_link_boy*`, `objects/object_link_child*` and
  `objects/object_custom_equip` (custom equipment: swords and shields in their sheath, first person hands, ...), tunic
  specific skeletons and display lists included. `.meta` alias files there count too, with what they point at and
  everything in the same folder (its vertices and textures). It's drawn by the teammate's own equipment, tunic and
  boots. Parts a model doesn't replace are vanilla.
- Your model is the Link files of your enabled mods, as you have them equipped (it follows alternate assets being
  toggled). Under Network > Anchor > Your Link Model you can leave some of your mods out of it: that changes your own
  Link (in the game and on the pause screen) and what teammates see you as at the same time, without a restart and
  without being connected. Your mods stay enabled for everything else they change. While it's open, your model stands
  next to Link, turning around in his pose (Show It Next to Link).
- Your mods are looked into and your model is bundled on a worker thread, and only mods that are new or changed are
  looked into again. Models are prepared a few milliseconds each frame, so getting one doesn't stall the game; until
  it's ready, a teammate is drawn as vanilla Link and your own Link stays as your mods make him. Models are mounted
  while the audio thread, which loads resources too, waits.
- Models are sent between teammates through the Anchor server: a teammate who doesn't have yours yet is asked whether
  to download it (or downloads it always / never, a setting under Your Link Model). It's sent a little every frame and
  checked against its id before it's used, then kept in the `anchor-models` folder, so each model is only sent once.
  Models over 64 MB aren't sent.
- The Anchor server keeps the models that pass through it too (after checking them against their id) and sends them
  itself to whoever asks for them later, so each model is uploaded once. The server's `MODEL_CACHE_MB` (2048 by
  default, 0 turns it off) limits how much it keeps, the least recently used go first.
- A model can be tried out locally on all dummy players with the `gRemote.Anchor.Developer.PuppetModelTestPath` CVar
  (the path of a model `.o2r`).

## Known limitations

### General

- Puppets follow the authority's game about 10 times a second and are smoothed, so they trail slightly behind.
- When the authority leaves a room, it takes the others' enemies about a second to start running their own AI, so they
  may jump or reset in that moment.
- Destroyed grass is only shared live, not caught up on when entering a room, since grass can grow back.

### Enemies

- Between different seeds, a breakable you haven't loaded yet when a teammate breaks theirs stays whole for you, since
  there's no telling yet whether it holds one of your checks.
- Between different seeds, Boss Souls follow each player's own seed: without its soul a boss doesn't appear in your
  game at all, so you take no part in its fight. You never run the room for the others, get no copy of their boss, and
  their boss doesn't go after you or hit you. Finding the soul while in the room changes nothing until you enter it
  again, like without co-op.
- Matching mods only looks at gameplay mods as a whole: their load order isn't compared, and a mod that's both a texture
  pack and a gameplay mod counts as a gameplay mod. Old `.otr` mods aren't looked into.
- A `.meta` alias in a Link model brings the whole folder of what it points at into the model, which may be more than
  it needs if that folder holds other things too.
- Every other enemy type only shares its health and death, and runs its own AI for each player. Among them:
  - Peahat, Tailpasaran, Shabom
  - the Deku Scrubs of the Deku Tree puzzle
  - Parasitic Tentacles, Poe Sisters, Big Poes
- Nabooru's Iron Knuckle isn't mirrored, its fight is part of a cutscene.
- Puppets don't show what their enemy's own update would add besides their drawing: sounds, particles, dust, flames
  trails (Flare Dancer), armor pieces flying off (Iron Knuckle).
- Some details are approximated on puppets:
  - Stalfos upper body animations
  - Dark Link: a puppet shows the authority's Dark Link in the same pose (sent like a teammate's player model, with
    the frame between two snapshots guessed), fading in and out the same way. Hits on his sword or shield don't bounce
    off a puppet, they count like hits on his body.
  - Like-Like's wobble: puppets wobble, pulse and reach out like the authority's, moving towards the targets it sends
    every frame by themselves, so the wobble isn't in the exact same phase as the authority's
  - the Moblin's shield, which doesn't block from the front for teammates
- Enemies that act on the player they target directly (grab, freeze, damage outside of a normal hit) only go after the
  authority's own player:
  Floormaster, Wallmaster, ReDead, Gibdo, Like-Like, Moblin, Skulltula, Gerudo fighter, Dead Hand's hands, Dark Link
  (who copies that player's moves).
- Who dealt how much damage is counted by the authority. It starts over when another player takes over the room, and
  when a room is entered again.
- An enemy touching a teammate's dummy player in your game doesn't count as a hit, and the enemy doesn't react to it
  either (like Ganon knocking back whoever his sword hit).
- Hits from enemies run by a teammate always knock you back the same small way (plus burning, freezing or shocking),
  not with that enemy's exact hit reaction.
- A hit on a puppet counts if the authority's enemy can be hit within a few frames of it arriving (two players hitting
  at once both count). Otherwise, like while it's still reeling from the previous hit, it's lost.
- A weapon that keeps touching a puppet counts as one hit until it stops touching it for a couple of frames.

### Bosses

- Barinade: a teammate's boomerang hitting her body near the end of the fight doesn't get the reaction of a boomerang
  (it counts like any other hit).
- A player still in a boss's intro runs their own boss until it's over, and one entering after the authority's boss
  was defeated gets the defeat right away.
- Queen Gohma: only the authority's player can make her close her eye by aiming at her.
- Bongo Bongo's hands and Morpha's tentacles grab you in your own game only. In your teammates' games the grab
  misses, so they don't see you being held, and the tentacle may grab them at the same time in theirs.
- Ganondorf only blocks, and Twinrova's sisters only dodge, when the authority's player aims at them.
- Bongo Bongo's head goes by each player's own position, his hands go after the same player for everyone.
- What a shot does when it ends is shared, but not all of its look: the lightning of a ball bursting on a player, and
  the shield blast a player sends at Twinrova, are only seen by that player.
- Mirror Shield charge from Twinrova's blasts is each player's own.
- In the pause menu, the sounds of a boss that keeps following the authority's (Volvagia's roar) can be heard.
- Boss puppets follow the authority's boss about 10 times a second. In between they act on their own, so they may
  briefly do something else (like Volvagia heading for another hole) before snapping back.

### Time of day

- Players in different scenes keep their own clocks until they meet. Setting the time back (rather than forward) is
  undone by a teammate's clock.

### Drops

- Not shared:
  - fairies spawned at low health
  - randomizer items (they have their own sync)
  - permanent collectibles (heart pieces, keys), which go to whoever picks them up

### Puzzles and objects

- Only torches, signs, toggle floor switches, push blocks and cobra mirror statues are shared live. Other puzzle objects only share the switch and
  clear flags they set, so ones that only check those when they spawn may not update until the room is reloaded.
- Other moving platforms and traps move independently for each player.
- These flags are never shared (kept from before Co-op Sync):
  - the Water Temple water level
  - the Forest Temple elevator
  - the Ganon's Tower collapse timer
  - the Great Fairy cutscenes
- One player at a time can push a block or turn a statue, with a 100 frame lock.

### Carrying

- Picking a bomb off a Bomb Flower doesn't empty it for teammates until they reload the area.
- Objects placed in the room are followed for at most 60 frames after being thrown, or until they land.
- Bombchus aren't shown.

### NPCs, dialogue and cutscenes

- NPCs are only respawned on story progress (event flags), and only those placed in your room. They pop into place.
  NPCs that spawn helpers or walk a set path may act oddly after respawning.
- NPC movement and conversations aren't shared.
- Cutscenes only play for the player who triggers them (like the Deku Tree opening its mouth). Their outcome is shared.

## To do

### Testing

None of these have been tested in the game yet:

- [ ] Two clients in co-op: rocks breaking when thrown, the carrying pose, and Deku Nut, arrow, seed and bomb hits on
  enemies a teammate runs
- [ ] The room owner's enemy settings: seeded enemy randomizer, enemy sizes and Hyper Enemies/Bosses reaching
  teammates, and New Enemy Seed
- [ ] Different seeds, including two Archipelago slots: a pot holding one of your checks stays for you, no drops cross
  over
- [ ] Per-player models with a real model mod and a custom equipment mod (the PuppetModelTestPath developer CVar works
  with a single client), then sending a model between two clients
- [ ] The model picker on your own Link: leaving mods out and back in, the pause screen, toggling alternate assets
- [ ] Matching gameplay mods: Match and Close renaming the files at the next start, Restore Mods Disabled for Co-op
- [ ] A boss room with one player missing the boss's soul: the others still fight it together, the one without it is
  left alone
- [ ] A Link model mod using `.meta` aliases, sent to a teammate; the model preview next to Link; no stall when a
  model arrives or the picker changes
- [ ] Archipelago: connecting to a slot, sending and receiving items, file select, and the slot options mapped onto
  the reworked randomizer settings (Ganon's Boss Key triggers, Triforce Hunt, Zelda's Letter, Kakariko gate)

### Anchor server

- [x] Pass on unknown fields of the room state (`coopSettings`, `coopEnemySeed`) and the client state (`modelId`,
  `modelSize`, `modsHash`, `mods`): the server already keeps both as they come in
- [x] Relay `MODEL_REQUEST` and `MODEL_CHUNK`, and accept messages of about 50 KB: packets with a `targetClientId` are
  relayed whatever their type, and messages of up to 8 MB are accepted
- [x] Keep models by their hash, so each is uploaded once (anchor `coop-model-cache` branch, not on the public server)

### Open gaps

- [ ] Ocarina: the game replaying a recognized song and the song's jingle aren't heard by teammates, only notes
  played by hand
- [ ] A breakable that isn't loaded yet when a teammate on another seed breaks theirs stays whole for you
- [ ] Matching mods doesn't compare their load order, and doesn't look into old `.otr` mods

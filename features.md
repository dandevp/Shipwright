# Anchor Co-op Sync

Co-op Sync (room setting "Co-op Sync (Experimental)", set by the room owner) shares the live state of rooms and
enemies between teammates, so they can play through the same areas together.

## Requirements

- Only players on the same team, with the same game version and the same randomizer seed, sync with each other.
- They sync with each other while in the same scene. Vanilla and Master Quest dungeons are kept apart.
- Story progress (NPCs reacting to it) needs "Sync Items and Flags" on as well.
- While an Archipelago slot is connected, "Sync Items and Flags" is greyed out (Archipelago moves the items),
  Co-op Sync stays available.

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
  - If the authority leaves the room or pauses for more than a second, another player in the room takes over.
  - A player coming back from a pause doesn't take over again, their enemies stood still meanwhile.
- Hitting a puppet hits the authority's enemy, with its own damage table, reaction and death.
- On the authority, enemies go after the most dangerous player in the room: whoever dealt them the most damage. While
  nobody has hurt them yet, they go after the nearest player, and switch once another one is clearly closer. Their hits
  on a teammate hurt that teammate.
- What they shoot is shown to everyone, but only the authority's copy can hurt anyone.
- Mirrored enemies:
  - Armos, Baby Dodongo, Bari, Beamos, Biri, Bubble, Dead Hand and its hands, Dodongo
  - Flare Dancer (and its core), Floormaster, Freezard, Gerudo fighter, Gohma Larva (and its egg), Guay
  - Iron Knuckle, Keese, Lizalfos, Dinolfos, Mad Scrub, Moblin, Octorok, Poe, ReDead, Gibdo, Like-Like, Shell Blade
  - Skulltula, Skullwalltula, Spike, Stalchild, Stalfos, Stinger, Tektite, Torch Slug, Wallmaster
  - Wolfos, Deku Baba, Withered Deku Baba
- Enemies a mirrored enemy spawns during the fight (the Biris a Bari splits into, a Flare Dancer's core) are spawned
  for everyone too.
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
- Reflecting one Twinrova sister's beam onto the other, and stunning Twinrova with a charged Mirror Shield blast, count
  for everyone, whoever does it.
- King Dodongo swallows a bomb thrown into his mouth by any player: the thrower's game decides it, and his choking on
  it is shared.
- Morpha's core can be pulled out by any player's hookshot: the puller's game moves it, and the authority's follows.
- While a boss holds you (Bongo Bongo's hand, Morpha's tentacle) or your hookshot holds it, it runs in your game by
  itself until it lets go, so you get away from it like in vanilla (hitting Morpha's core cuts her tentacle).
- Ganon being downed (and every player getting their Master Sword back) is a cutscene too, played by every player:
  when the authority's Ganon is downed, everyone's is. He joins the shared fight again once he's back up in your game.
- Every boss is mirrored: Queen Gohma, King Dodongo, Barinade, Phantom Ganon (his horse and fakes too), Volvagia,
  Morpha, Bongo Bongo, Twinrova, Ganondorf, Ganon.

### Puzzles and objects

- Torches: lighting one lights it for everyone.
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

### NPCs

- When a teammate in your scene makes story progress, the NPCs in your room are respawned, so they react to it right
  away rather than when you come back (like Mido moving out of the way).

## Known limitations

### General

- Puppets follow the authority's game about 10 times a second and are smoothed, so they trail slightly behind.
- When the authority leaves a room, it takes the others' enemies about a second to start running their own AI, so they
  may jump or reset in that moment.
- Destroyed grass is only shared live, not caught up on when entering a room, since grass can grow back.

### Enemies

- Every other enemy type only shares its health and death, and runs its own AI for each player. Among them:
  - Peahat, Tailpasaran, Shabom
  - the Deku Scrubs of the Deku Tree puzzle
  - Parasitic Tentacles, Poe Sisters, Big Poes, Dark Link
- Nabooru's Iron Knuckle isn't mirrored, its fight is part of a cutscene.
- Puppets don't show what their enemy's own update would add besides their drawing: sounds, particles, dust, flames
  trails (Flare Dancer), armor pieces flying off (Iron Knuckle).
- Some details are approximated on puppets:
  - Stalfos upper body animations
  - Like-Like's wobble
  - the Moblin's shield, which doesn't block from the front for teammates
- Enemies that act on the player they target directly (grab, freeze, damage outside of a normal hit) only go after the
  authority's own player:
  Floormaster, Wallmaster, ReDead, Gibdo, Like-Like, Moblin, Skulltula, Gerudo fighter, Dead Hand's hands.
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
  misses, so they don't see you being held.
- Ganondorf only blocks, and Twinrova's sisters only dodge, when the authority's player aims at them.
- Bongo Bongo's head goes by each player's own position, his hands go after the same player for everyone.
- What a shot does when it ends is shared, but not all of its look: the lightning of a ball bursting on a player, and
  the shield blast a player sends at Twinrova, are only seen by that player.
- Mirror Shield charge from Twinrova's blasts is each player's own.
- Boss puppets follow the authority's boss about 10 times a second. In between they act on their own, so they may
  briefly do something else (like Volvagia heading for another hole) before snapping back.

### Drops

- Not shared:
  - fairies spawned at low health
  - randomizer items (they have their own sync)
  - permanent collectibles (heart pieces, keys), which go to whoever picks them up

### Puzzles and objects

- Only torches, push blocks and cobra mirror statues are shared live. Other puzzle objects only share the switch and
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

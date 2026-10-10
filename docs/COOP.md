# ⚔️ Hyrule, Together ⚔️

### The ultimate way to play Ocarina of Time Randomizer with your friends

> **Hey! Listen!** 🧚 Your friends aren't just *somewhere* in Hyrule anymore. They're **right next to you**.

Grab your sword, rally your party and storm the dungeons **side by side**. This isn't a "we both play and chat on
Discord" kind of co-op. You share the same rooms, live:

- 🐉 **Fight bosses together.** One Volvagia, one Bongo Bongo, everyone hacking away at the same health bar.
  Whoever deals the most damage gets the boss's attention, so pick your tank wisely. Everyone still gets their own
  victory cutscene, heart container and blue warp. 🏆
- 💀 **Shared enemies.** That Stalfos your buddy is fighting is the *same* Stalfos you're fighting. Gang up on it.
- 🧩 **Shared puzzles.** Switches, torches, pushed blocks, cleared rooms: what one of you solves is solved for all.
  Divide and conquer that Water Temple. 🌊
- 🎲 **Your own seed, your own loot.** Everyone can run their **own randomizer seed or Archipelago slot**.
  Your chests, drops and checks stay yours. Nobody steals your Hookshot. 🪝
- 🌈 **Show off your style.** Custom Link models, swords and shields travel with you. Your friends see *your* Link,
  you see *theirs*. Pick your look in the menu and watch it spin right next to you. ✨
- 🎵 **Jam session.** Ocarina notes played by hand are heard by everyone nearby.

Twice the heroes. Twice the chaos. Exactly one Ganon who is about to have a **very** bad day. 🔥

> 🧪 This is an **experimental test build**: things *will* break, and that's the point. Tell us when they do
> (see the bottom of this page). Linux only (AppImage).

---

## Setup

1. Unpack this folder anywhere.
2. Run `./start.sh` (if it doesn't start: `chmod +x start.sh SoH-Coop.appimage`).
3. On the first start the game asks for your Ocarina of Time ROM (a supported N64 dump, check it at
   https://ship.equipment) and extracts the game data from it. That takes a minute. No game data comes with this build.

Everything the game stores (settings, saves, game data) ends up in `data/` next to `start.sh`.

## Playing together

1. In the game, press **Esc** for the menu: **Network > Anchor**.
2. Host: `YOUR_SERVER_ADDRESS`, Port: `43383`.
3. Enter your name, and the **same room id and team** as everyone else. Connect.
4. The room owner (whoever made the room first) turns on **Co-op Sync (Experimental)** in the room settings.

You sync with the others while you're in the same area. Items and drops stay your own when you play different seeds.

## Mods

- Link models (and custom swords, shields, ...) are per player: the others see you with yours. They're downloaded
  when you meet. Under **Your Link Model** you pick which of your mods make up your Link.
- Mods that change the game itself must be the same for everyone. If yours differ from the room owner's, the game
  offers to match them and restarts.

## 🛠️ Building it yourself

Want to build it from source instead of using the AppImage? The code lives on the `develop` branch of
https://github.com/dandevp/Shipwright.

### What you need

- Linux (for Windows and macOS see `docs/BUILDING.md` in the repo)
- **GCC 10+** or **Clang 16+**, **CMake 3.26+**, **Ninja**, **Git**
- About 3 GB of disk space, and your Ocarina of Time ROM to play
- The libraries, in one go for your distro:

| Distro | Command |
| --- | --- |
| Arch / Manjaro | `sudo pacman -S git cmake ninja lsb-release sdl2 libpng libzip nlohmann-json tinyxml2 spdlog sdl2_net opusfile libvorbis python openssl` |
| Debian / Ubuntu | `sudo apt install libusb-dev libusb-1.0-0-dev libsdl2-dev libsdl2-net-dev libpng-dev libglew-dev nlohmann-json3-dev libtinyxml2-dev libspdlog-dev ninja-build libogg-dev libopus-dev opus-tools libopusfile-dev libvorbis-dev libespeak-ng-dev libzip-dev zipcmp zipmerge ziptool git cmake lsb-release libssl-dev` |
| Fedora | `sudo dnf install git cmake ninja-build lsb_release SDL2-devel SDL2_net-devel libpng-devel libzip-devel libzip-tools nlohmann-json-devel tinyxml2-devel spdlog-devel opusfile-devel libvorbis-devel openssl-devel` |
| openSUSE | see `linux-build-deps/zypper.txt` in the repo |
| Nix | `nix develop ./linux-build-deps` |

On Ubuntu, check `cmake --version`: older releases ship one below 3.26, get a newer one from https://cmake.org or pip.

### Build

```sh
git clone -b develop https://github.com/dandevp/Shipwright.git
cd Shipwright
git submodule update --init                       # libultraship, Torch and the Archipelago client libraries

cmake -H. -Bbuild-cmake -GNinja -DCMAKE_BUILD_TYPE=Release
cmake --build build-cmake --target GenerateSohOtr  # SoH's own assets (soh.o2r)
cmake --build build-cmake                          # the game: build-cmake/soh/soh.elf
```

The first build downloads a few more libraries and takes a while (10–20 minutes on an average PC).

### Run

```sh
cd build-cmake/soh
./soh.elf
```

On the first start it asks for your ROM and extracts the game data, like the AppImage does.

### Make your own AppImage (optional)

```sh
cd build-cmake
cpack -G External          # creates Ship-9.3.0-rolling.appimage
```

If the AppImage ends up missing or old (the "strip" step fails on some distros, like Arch), run it as
`NO_STRIP=1 cpack -G External` instead.

## Reporting problems

Tell us what you were doing, where in the game, and send `data/logs/` (the newest log file).

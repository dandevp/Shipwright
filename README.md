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
> (see "Reporting problems" below). Ready-made build for Linux (AppImage); on Windows you build it yourself
> (see "Building it yourself").

---

## 🚀 Get started

Got the Linux test build (`SoH-Coop-friends.tar.gz`) from a friend? Then:

1. Unpack it anywhere.
2. Run `./start.sh` (if it doesn't start: `chmod +x start.sh SoH-Coop.appimage`).
3. On the first start the game asks for your Ocarina of Time ROM (a supported N64 dump, check it at
   https://ship.equipment) and extracts the game data from it. That takes a minute. No game data comes with this build.

Everything the game stores (settings, saves, game data) ends up in `data/` next to `start.sh`.

Got a Windows zip from a friend instead (see "Make a zip for your friends" below)? Unpack it anywhere and run
`soh.exe`. If Windows SmartScreen warns about an unknown app, click **More info > Run anyway** (the build isn't
signed). Settings, saves and game data are stored next to `soh.exe`.

## Playing together

1. In the game, press **Esc** for the menu: **Network > Anchor**.
2. Host: `YOUR_SERVER_ADDRESS`, Port: `43383`.
3. Enter your name, and the **same room id and team** as everyone else. Connect.
4. The room owner (whoever made the room first) turns on **Co-op Sync (Experimental)** in the room settings.

You sync with the others while you're in the same area. Items and drops stay your own when you play different seeds.

## Mods

- Link models (and custom swords, shields, ...) are per player: the others see you with yours. They're downloaded
  when you meet. Under **Your Link Model** you pick which of your mods make up your Link. Most model mods are
  alternate assets: they only show (for you and the others) while alternate assets are on, toggled with **Tab**.
- Mods that change the game itself must be the same for everyone. If yours differ from the room owner's, the game
  offers to match them and restarts.

## 🛠️ Building it yourself

Want to build it from source instead of using the AppImage? The code lives on the `develop` branch of
https://github.com/dandevp/Shipwright.

### 🐧 Linux

#### What you need

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

#### Build

```sh
git clone -b develop https://github.com/dandevp/Shipwright.git
cd Shipwright
git submodule update --init                       # libultraship, Torch and the Archipelago client libraries

cmake -H. -Bbuild-cmake -GNinja -DCMAKE_BUILD_TYPE=Release
cmake --build build-cmake --target GenerateSohOtr  # SoH's own assets (soh.o2r)
cmake --build build-cmake                          # the game: build-cmake/soh/soh.elf
```

The first build downloads a few more libraries and takes a while (10–20 minutes on an average PC).

#### Run

```sh
cd build-cmake/soh
./soh.elf
```

On the first start it asks for your ROM and extracts the game data, like the AppImage does.

#### Make your own AppImage (optional)

```sh
cd build-cmake
cpack -G External          # creates Ship-9.3.0-rolling.appimage
```

If the AppImage ends up missing or old (the "strip" step fails on some distros, like Arch), run it as
`NO_STRIP=1 cpack -G External` instead.

### 🪟 Windows

#### What you need

- Windows 10 or 11, 64-bit, with **at least 8 GB of RAM** (the compiler runs out of memory with less)
- About 10 GB of disk space (Visual Studio, the libraries and the build)
- **Visual Studio 2022** (the free Community edition is fine): https://visualstudio.microsoft.com
  - In the installer, tick the **Desktop development with C++** workload.
  - Under "Installation details" on the right, make sure **MSVC v143 - VS 2022 C++ x64/x86 build tools**, a
    **Windows 10/11 SDK** and **C++ CMake tools for Windows** are ticked (they usually are). The CMake tools bring
    CMake and Ninja along.
- **Git**: https://git-scm.com/download/win (keep the default options)
- **Python 3**: https://www.python.org/downloads/ (tick "Add python.exe to PATH" in the installer)

Install Git and Python on their own rather than through the Visual Studio installer, that has caused trouble before.
All other libraries (SDL2, OpenSSL, libzip, ...) are downloaded and built automatically on the first build, through
vcpkg.

#### Build

Open **Developer PowerShell for VS 2022** from the Start menu (not the plain PowerShell: this one has the compiler,
CMake and Ninja set up), then:

```powershell
cd $HOME                                           # or wherever you want the code to live
git clone -b develop https://github.com/dandevp/Shipwright.git
cd Shipwright
git submodule update --init                       # libultraship, Torch and the Archipelago client libraries

cmake -S . -B build-windows -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-windows --target GenerateSohOtr  # SoH's own assets (soh.o2r)
cmake --build build-windows                          # the game: build-windows\soh\soh.exe
```

The first configure step sets up vcpkg and builds all libraries, which takes a long time (30–60 minutes is normal).
Later builds only rebuild what changed.

Keep the path short and without spaces or umlauts (like `C:\Users\you\Shipwright`): very long paths can break the
vcpkg step.

#### Run

```powershell
cd build-windows\soh
.\soh.exe
```

On the first start it asks for your ROM and extracts the game data. Settings, saves and game data are stored in the
folder you start it from, and mods go into a `mods` folder there.

#### Make a zip for your friends (optional)

```powershell
cd build-windows
cpack                       # creates a .zip in ..\_packages
```

The zip holds `soh.exe` and `soh.o2r`, everything a friend needs besides their own ROM. Never put your ROM or
`oot.o2r` in it.

#### Updating

```powershell
git pull
git submodule update --init
cmake --build build-windows
```

#### If it goes wrong

- **`cl` or `ninja` not found**: you're in the plain PowerShell. Open "Developer PowerShell for VS 2022".
- **vcpkg fails to download or build a library**: check your internet connection and run the `cmake -S . -B ...`
  step again, it continues where it stopped. If it keeps failing, delete the `vcpkg` and `build-windows` folders and
  start over.
- **The compiler crashes or the PC freezes**: not enough RAM. Build with fewer jobs:
  `cmake --build build-windows --parallel 2`.
- **Missing `soh.o2r` at start**: run the `GenerateSohOtr` step again.
- Want to work on the code in Visual Studio? `docs/BUILDING.md` explains how to generate a Visual Studio solution.

macOS: see `docs/BUILDING.md`.

## 🐛 Reporting problems

Tell us what you were doing, where in the game, and send `data/logs/` (the newest log file).

---

# About Ship of Harkinian

![Ship of Harkinian](docs/shiptitle.darkmode.png#gh-dark-mode-only)
![Ship of Harkinian](docs/shiptitle.lightmode.png#gh-light-mode-only)

* [Website](https://www.shipofharkinian.com)
* [Discord](https://discord.com/invite/harbourmasters)

If you're having any trouble after reading through this `README`, feel free to ask for help in the Support text channels. Please keep in mind that we do not condone piracy.

# Quick Start

The Ship does not include any copyrighted assets.  You are required to provide a supported copy of the game.

### 1. Verify your ROM dump
You can verify you have dumped a supported copy of the game by using the compatibility checker at https://ship.equipment/. If you'd prefer to manually validate your ROM dump, you can cross-reference its `sha1` hash with the hashes [here](docs/supportedHashes.json).

### 2. Download The Ship of Harkinian from [Releases](https://github.com/HarbourMasters/Shipwright/releases)

### 3. Launch the Game!
#### Windows
* Extract the zip
* Launch `soh.exe`

#### Linux
* Execute `soh.appimage`.  You may have to `chmod +x` the appimage via terminal.
* When prompted, select your supported copy of the game.
* Saves, settings and `oot.o2r` are stored in `~/.local/share/soh/`. If the folder you launch from already has `shipofharkinian.json`, `oot.o2r` or `oot-mq.o2r`, that folder is used instead. Set `SHIP_HOME` to choose another folder.

#### macOS
* Run `soh.app`. When prompted, select your supported copy of the game.
* You should see a notification saying `Processing OTR`, then, once the process is complete, you should get a notification saying `OTR Successfully Generated`, then the game should start.

### 4. Play!

Congratulations, you are now sailing with the Ship of Harkinian! Have fun!

# Configuration

### Default keyboard configuration
| N64 | A | B | Z | Start | Analog stick | C buttons | D-Pad |
| - | - | - | - | - | - | - | - |
| Keyboard | X | C | Z | Space | WASD | Arrow keys | TFGH |

### Other shortcuts
| Keys | Action |
| - | - |
| ESC | Toggle menu |
| F2 | Toggle capture mouse input |
| F5 | Save state |
| F6 | Change state |
| F7 | Load state |
| F9 | Toggle Text-to-Speech |
| F11 | Fullscreen |
| Tab | Toggle Alternate assets |
| Ctrl+R | Reset |

# Project Overview
Ship of Harkinian (SOH) is built atop a custom library dubbed libultraship (LUS). Back in the N64 days, there was an SDK distributed to developers named libultra; LUS is designed to mimic the functionality of libultra on modern hardware. In addition, we are dependent on the source code provided by the OOT decompilation project.

In order for the game to function, you will require a **legally acquired** ROM for Ocarina of Time. Click [here](https://ship.equipment/) to check the compatibility of your specific rom. Any copyrighted assets are extracted from the ROM and reformatted as a .o2r archive file which the code uses.

### Graphics Backends
Currently, there are three rendering APIs supported: DirectX11 (Windows), OpenGL (all platforms), and Metal (MacOS). You can change which API to use in the `Settings` menu of the menubar, which requires a restart.  If you're having an issue with crashing, you can change the API in the `shipofharkinian.json` file by finding the line `gfxbackend:""` and changing the value to `sdl` for OpenGL. DirectX 11 is the default on Windows.

# Custom Assets

Custom assets are packed in `.otr` archive files. To use custom assets, place them in the `mods` folder.

If you're interested in creating and/or packing your own custom asset `.otr` files, check out the following tools:
* [**retro - OTR generator**](https://github.com/HarbourMasters64/retro)
* [**fast64 - Blender plugin**](https://github.com/HarbourMasters/fast64)

# Development
### Building

If you want to manually compile SoH, please consult the [building instructions](docs/BUILDING.md).

### Playtesting
If you want to playtest a continuous integration build, you can find them at the links below. Keep in mind that these are for playtesting only, and you will likely encounter bugs and possibly crashes. 

* [Windows](https://nightly.link/HarbourMasters/Shipwright/workflows/generate-builds/develop/soh-windows.zip)
* [macOS](https://nightly.link/HarbourMasters/Shipwright/workflows/generate-builds/develop/soh-mac.zip)
* [Linux](https://nightly.link/HarbourMasters/Shipwright/workflows/generate-builds/develop/soh-linux.zip)

### Further Reading
More detailed documentation can be found in the 'docs' directory, including the aforementioned [building instructions](docs/BUILDING.md).

* [Credits](docs/CREDITS.md)
* [Custom Music](docs/CUSTOM_MUSIC.md)
* [Formatting](docs/FORMATTING.md)
* [Controller Mapping](docs/GAME_CONTROLLER_DB.md)
* [Modding](docs/MODDING.md)
* [Versioning](docs/VERSIONING.md)

<a href="https://github.com/Kenix3/libultraship/">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="./docs/poweredbylus.darkmode.png">
    <img alt="Powered by libultraship" src="./docs/poweredbylus.lightmode.png">
  </picture>
</a>

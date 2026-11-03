# The five platforms: what a player downloads, what it needs, and how much of it is known

Planning step 209 (PORT-11), brought up to date at planning step 211 (2026-11-03), after the first release was made. One page for a stranger who asks "does it run on my machine?". It states what the repository can say and no more. Every claim is one of the three kinds of
[`apple.md`](apple.md): **WRITTEN** (the file exists; nothing is said about what it does), **CHECKED** (a tool printed a result, and the tool is named; a check against a stand-in is said to be one)
or **NOT KNOWN** (nobody has run it). Each platform ends with `VERIFIED BY` and `NOT KNOWN`.

**The game's data is not in this repository and not in a program.** Every download below is either a *shell* (the program, with no game in it: it starts, says so and stops) or an archive the OWNER
assembles on his PC from a program and his own content package (`packaging.md` section 8, `ci.md` section 7). A shell run with the fixture level (`building.md` section 5) shows a hand-written test
level, not the game.

Every archive carries `LICENSE` (the project's licence) and `NOTICE.md` (the notices of the software the game contains), beside the APK for Android; the packager refuses to write one without them.

| | Windows | Linux | Android | macOS | iOS |
|---|---|---|---|---|---|
| **What is downloaded** | `MagicPortals-<version>-windows-x64.zip` | `MagicPortals-<version>-linux-x86_64.tar.gz` | `MagicPortals-<version>-android.apk` (+ `LICENSE`, `NOTICE.md` beside it) | `MagicPortals-<version>-macos-universal.zip` (a `MagicPortals.app`) | `MagicPortals-<version>-ios-arm64.ipa` |
| **Program built by** | the owner's PC (MSVC) or `release.yml` | the owner's WSL or `release.yml` (Ubuntu 22.04, g++ 12.3) | the owner's PC with his key (`build_android.sh --release`) | `apple.yml` on GitHub's macOS runner | the same |
| **Operating system** | 64-bit Windows; Windows 11 is the one it has been run on | 64-bit Linux, x86-64 CPU, an X11 display (XWayland on Wayland: not tried) | Android 8.0 (API 26) or later; `arm64-v8a`, `armeabi-v7a` (a phone whose Android is 32-bit) or `x86_64` | macOS 13.3 or later, Apple silicon or Intel (one universal program) | iOS 16.3 or later, arm64 |
| **Graphics** | a Vulkan 1.2 driver (`vulkan-1.dll`; without one the program says so in words) | a Vulkan 1.2 driver and the loader `libvulkan.so.1` (Mesa radv, anv or lavapipe, or the vendor's) | Vulkan 1.1 (the manifest requires it; Google Play filters on it, a sideload does not); the engine creates its instance with `apiVersion` 1.2 (`engine/src/renderer/VulkanContext.cpp`, READ), so whether a phone with Vulkan 1.1 only starts the game is NOT KNOWN | Metal, through MoltenVK 1.4.2 linked into the program (no Vulkan SDK) | the same |
| **Runtime it needs** | the Visual C++ x64 runtime: in the archive when the owner passes `--vc-redist`, else the player's PC must have it (`PLAYING.txt` says so) | libasound (ALSA) for sound; glibc as new as the one the program names (the floor, below); with `--portable` no libstdc++ | none | none | none |
| **Install** | unpack the zip; start `MagicPortals.exe` | `tar xzf` (keeps the executable bit); start `./MagicPortals` | sideload the APK (`adb install` or the phone's installer) | unpack; the seal is invalid once the owner adds the data (`READ ME FIRST.txt`) | sideload with Sideloadly, AltStore or SideStore (`apple.md` section 11) |
| **Controls** | keyboard and mouse (below) | keyboard and mouse | touch: the modern on-screen controls | keyboard and mouse | touch |
| **Saves and log** | `%APPDATA%\Magic Portals` (medals, sound switches, achievements, control choices); log `%TEMP%\magicportals.log` | `~/.local/share/Magic Portals` (or under `$XDG_DATA_HOME`); log `<temporary folder>/magicportals.log` | the app's private folder: `files/Magic Portals/` (saves), `files/Magic Portals/scratch/magicportals.log` | `~/Library/Application Support/Magic Portals/` | NOT KNOWN beyond `apple.md` section 2 |

**The names in the first row are the ones the packager writes.** The GitHub Release of the first release publishes its three files under fixed names, so that the README's `releases/latest/download/<name>` links work: `MagicPortals-Windows.zip`, `MagicPortals-Linux.tar.gz` and `MagicPortals-Android.apk` (a copy of the release APK under a new name; its contents are unchanged), beside `LICENSE`, `NOTICE.md` and a `SHA256SUMS.txt` ([`releasing.md`](releasing.md), step 9). No macOS or iOS archive with the game was made. **Where each of the three files has been started (CHECKED, 2026-11-03):** the Linux archive, unpacked and started in a Linux subsystem on the owner's PC under a virtual display on Mesa's software Vulkan driver, with no GPU (below); the Windows archive, nowhere (the program in it was built by a CI runner and read as bytes); the Android APK, nowhere (it has not been installed or started on any device or emulator).

**Where the untagged facts of that table come from (READ in the source, not tried on each platform):** the keys, `game/MagicPortalsLayer.cpp` lines 1148 to 1156 (`Left`/`A`, `Right`/`D`, `R` for a retry, `Escape` for Back); the
save folders, `UserDataDirectory` in `engine/src/platform/ExecutablePath.cpp` (an absolute `XDG_DATA_HOME`, else `~/.local/share`; `~/Library/Application Support`; the app's internal data path on Android) joined with the game's title `Magic Portals`
(`game/main.cpp`), and `docs/android.md` section 0 for the Android folder as it was seen on an emulator; Android 8.0 and Vulkan 1.1, `game/android/AndroidManifest.xml` lines 34 and 36 (`minSdkVersion` 26; the feature version `0x401000`);
macOS 13.3 and iOS 16.3, `MACOS_MINIMUM` and `IOS_MINIMUM` in `tools/apple/test_plists.py`, which the plist builder is tested against.

**Controls, all platforms.** The modern on-screen controls are the default a player starts with (`CTL-13`): two walk arrows (one in each bottom corner), restart and pause (top right), clear portals
(top left). On a desktop the mouse is the finger: a click fires a portal shot toward the pointer, a held click on an arrow walks; the keyboard also works (`←`/`→` or `A`/`D` walk, `Esc` pauses,
`R` retries a level). On a phone it is touch, and a second finger works the controls while one works the world (replayed ticks only: no touch build exists on a desktop, and a phone has not been
tried). **A gamepad:** the engine reads connected pads as input sources (its README, `src/platform/Gamepads.hpp`), and the game binds none of its actions to one (a search of `game/` finds no
pad button): gamepad play is NOT KNOWN to work and should be assumed not to.

## Windows

* **VERIFIED BY.** CHECKED: the developer's build runs on the owner's Windows 11 PC, which is where every parity capture, route and suite was taken (`README.md`); the release program of planning
  step 202 (MSVC, no baked path, no developer tools, the file properties, the icon) was built and launched there, and `release.py check-program` reads it as bytes and passes it
  (`packaging.md` section 8, step 208); that program is the owner's own build, not the file of the release. CHECKED on GitHub (2026-11-03, `ci.md` section 9): the `windows` job of `release.yml` built the release program on `windows-2025-vs2026` (MSVC 19.51) in its second dispatch
  and `release.py check-program` passed it (its first dispatch failed in that job, at the CMake configure: the `windows-2022` image has no Vulkan loader). That program, which is the one in the release's Windows archive, was read as bytes and **has not been started on any PC**.
  WRITTEN, never run: the `windows` job of `ci.yml` (the developer's build and the suites on a Windows runner).
* **NOT KNOWN.** Windows 10 and every other version; any GPU but the owner's; a PC without the C++ runtime or with a Windows N edition (the text exists in `PLAYING.txt`, the case was not seen); an
  archive with the game in it started on a PC that is not the owner's (none has been); the Windows archive of the release started on any PC (it has not been started on the owner's own either); a touch screen; whether a Windows runner can start the fixture level (it has no checked Vulkan driver: `ci.md` section 4).

## Linux

* **VERIFIED BY.** CHECKED, in the Linux subsystem of the owner's PC (Ubuntu 24.04, g++ 13.3, Python 3.12): the game builds with g++ and clang; every suite passes or skips; all 128 recorded routes win on
  the ticks Windows wins them on (step 199); the fixture level starts and draws on Mesa's software driver (lavapipe) under a virtual display, from the build tree and from an unpacked release archive
  with no flag but `--level` (step 208); the release archive is the same bytes when it is made twice; `check-program` passes the release program and refuses the developer's. The numbers of the
  portable build (the glibc it names, the libraries it still needs) are in `ci.md` section 5. CHECKED on GitHub (2026-11-03, `ci.md` section 9): `release.yml`'s `linux` job built the release program on Ubuntu 22.04 (g++ 12.3, glibc 2.35), and `check-program --max-glibc 2.35` passed it; its glibc ceiling, 2.35, was also read with `objdump -T`.
  CHECKED (2026-11-03): the Linux archive of the release, made from that program, was unpacked and run in the Linux subsystem of the owner's PC (Ubuntu 24.04, glibc 2.39) under Xvfb, on Mesa's software Vulkan driver, with no real GPU: it finds its `content` folder beside itself with no flag; a level starts and draws the
  original's art; exit 0 and "finished: clean"; all 128 levels start; with `content/` removed it exits 1 with a message; with the Vulkan driver hidden it exits 1 with the documented message. At the engine's present pin (`4261485`) the game and its suites build in that subsystem with no warning (GCC 13) and all 80 `ctest` tests pass (0 skipped, with the owner's data).
* **NOT KNOWN.** A real GPU on a real desktop; Wayland; a distribution other than Ubuntu; Ubuntu 22.04 itself, or any system whose glibc is older than 2.39 (the program was BUILT on 22.04, but the only system that started it has glibc 2.39); an
  ARM Linux machine (not built); sound from a speaker (the decoder was checked on the original's files, a Linux run was never listened to).

## Android

* **VERIFIED BY.** CHECKED, on an x86_64 emulator (Android 13, API 33, SwiftShader) on the owner's PC (`android.md` section 0, steps 204 and 205; those were earlier debug APKs, built at an older engine pin than the release APK): the APK installs; the first run unpacks 900 files and each
  has the size and sha256 of the package; the menu draws; levels start by touch; a score survives a force-stop; Back, Home and an update behave; all 128 levels start and all 128 routes win on the Windows
  ticks (x86_64 code). The packager's `inspect` checks the manifest, the 16 KB alignment, the signature and the libraries on every build; it holds the 16 KB alignment to the two 64-bit libraries (`arm64-v8a`, `x86_64`) only, because NDK r28 links `armeabi-v7a` with 4 KB pages.
  CHECKED (2026-11-03): the release APK carries THREE libraries (`arm64-v8a`, `armeabi-v7a`, `x86_64`); it was built on the owner's PC with `tools/build_android.sh --release` and the owner's existing release key (the key of his other game; it is not in the repository), and `release.py android` passed it.
  `release.yml`'s `android` job built the shell APK without game data on GitHub with the same three ABIs, debug-signed, and checked their libraries. At the engine's present pin the game compiles and links for the three ABIs with NDK r28 with no warning; and in the Linux subsystem the same sources built
  as a 32-bit x86 process (`-m32 -msse2 -mfpmath=sse`, **not an ARM one**) build with no warning and pass all 80 `ctest` tests, the 128 win routes among them. **Nothing has run on a 32-bit ARM device.**
* **NOT KNOWN.** **The release APK itself: it has never been installed or started on any device or emulator.** Any phone; arm64 or 32-bit ARM code running at all; speed, heat, memory, sound, real two-finger play, notches and edge gestures (an emulator cannot say); a 16 KB-page device; whether a phone with Vulkan 1.1 only starts the game (the manifest asks for 1.1, the engine asks the loader for an instance of 1.2); **the report Android
  shows after the game ended unexpectedly: it has never been shown on an emulator or a phone** (`android.md` section 11; the flows are WRITTEN in `tools/ci/android_smoke.py` and
  `android-emulator.yml`, which has never been dispatched).

## macOS

* **VERIFIED BY.** CHECKED on GitHub's `macos-15` runner, twice, on 2026-10-25 (`apple.md` section 4b): the game compiles and links as a universal program; the suites were built and run (that run ended 40 passed, 32 skipped and 2 failed, `test_mp_shoot` and `test_mp_sprites`, the two failures that planning step 209 turned into a skip and a pass; the workflow has run since that edit, as the two calls of `release.yml` on 2026-11-03, and its jobs succeeded in both: `ci.md` section 9, which does not state those runs' suite counts); the shell is assembled and its
  ad-hoc seal verifies; the fixture level starts from the unpacked zip, draws and exits clean on arm64 and, under Rosetta, as x86_64; a start with no data says so and exits 1. The GPU was a
  paravirtual Metal device, not an Apple GPU. CHECKED (2026-11-03): the `collect` job of `release.yml` ran `release.py assemble` over this game's two real CI-built shells, the macOS zip and the iOS `.ipa`, and it passed: `check-program`'s `/Users/` rule did not fire on their Mach-O files (`ci.md` section 9).
* **NOT KNOWN.** A person's Mac; a real Apple GPU; how the game looks or plays there; Gatekeeper and notarisation; audio; the seal after the data is added (the owner's route, `apple.md` section 7).

## iOS

* **VERIFIED BY.** CHECKED on the same runner: the device build and the simulator build compile and link, and the shell `.ipa` passes the structure checks (`Payload/` first, system libraries only,
  ad-hoc signed); and the `.ipa` that `release.yml` built passed the `collect` job's `release.py assemble` check on 2026-11-03 (see macOS above).
* **NOT KNOWN.** Everything about running it: no device and no simulator has started the program (the sibling port's own record is that its simulator stops at the first draw).

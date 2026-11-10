# Magic Portals Enhanced on macOS and iOS: what is written, what was checked, what nobody knows

Planning step 206 (PORT-8), the eighth step of the platform program. The owner has no Mac and no iPhone or iPad within reach, so **until one run on GitHub's macOS runner (section 4b) nothing here had ever run on an Apple machine, and nothing has yet run on a person's Mac, an iPhone or an iPad, or on a real Apple GPU.** Every claim in this page is one of three kinds, and says which:

* **WRITTEN**: the file exists. Nothing is said about what it does.
* **CHECKED**: a tool on a Windows PC, in WSL (Ubuntu) or on GitHub's macOS runner (section 4b) printed a result, and the tool is named. A check against a stand-in is said to be one.
* **NOT KNOWN**: nobody has run it. Each of these is a row of `docs/parity-backlog.md` (APPLE-1 and following).

No sentence in this repository calls the game a working build for a Mac, an iPhone or an iPad, and none can until a person has played the result on a Mac or an iPhone (the one run so far, section 4b, started the macOS shell and drew the fixture level: that is not playing it).

## 1. What exists

| Piece | File | Kind |
|---|---|---|
| macOS entry: `main()` of a `MagicPortals.app`, finds the data in the bundle, makes the engine's working folder, alerts and exits non-zero when it has no data | `game/macos/MacMain.mm` | WRITTEN; syntax CHECKED against stand-in Foundation and AppKit headers (section 9) |
| iOS entry: `SupersonicMain` for the engine's `IOSApp.mm`, the same lookup, a flags file in Documents, a failed start recorded in the game's log | `game/ios/IOSMain.mm` | WRITTEN; syntax CHECKED as above |
| the choices both entries make, as pure functions of strings: the two layouts, which folder wins, the `--root` hand-over, absolute path flags, whether a run may open a dialog, the words of "no data" | `game/sim/AppleLayout.hpp` | WRITTEN and CHECKED: `test_mp_applelayout` (suite, with mutations) |
| the `Info.plist` templates, filled with the project's one version by CMake | `game/macos/Info.plist.in`, `game/ios/Info.plist.in` | WRITTEN; keys CHECKED by `tools/apple/test_plists.py` (`plistlib`) |
| the CMake branches (Objective-C++, the linker flag, the entries, ARC, frameworks, the plist, the rename of `main`) | `CMakeLists.txt`, `game/CMakeLists.txt`, `game/apple/RenameDesktopMain.hpp` | WRITTEN; for Windows and Linux the generated build files are CHECKED to be the same (section 8); the Apple branches' own logic CHECKED on Linux by a simulation (section 8), never configured on Apple |
| the assembly shell: executable, plist, shaders, licences, placeholder icon, ad-hoc `codesign`, `ditto` zip, strict re-verify, the `.ipa` checks | `tools/apple/make_app.sh` | WRITTEN; argument lists CHECKED under stand-in tools (`tools/apple/test_make_app.py`, with mutations) and `shellcheck` |
| the workflow: dispatch only, three jobs | `.github/workflows/apple.yml` | WRITTEN; CHECKED by `actionlint` and `tools/apple/test_workflow.py` (with mutations); dispatched twice on 2026-10-25 (green) and called twice by `release.yml` on 2026-11-03 (green both times; docs/ci.md section 9) |
| the release workflow: dispatch only; calls `apple.yml`'s `macos` and `ios-device` jobs, then puts the published content package into the two shells, signs the Mac app again and starts it | `.github/workflows/apple-release.yml`, `tools/package/release.py assemble --resealed` | WRITTEN; CHECKED by `tools/ci/check_workflows.py` and `tools/package/test_release.py`; dispatched twice on 2026-11-10, the second run green (section 4c) |
| the fixture level a data-less runner starts | `tests/fixtures/ci/`, `tools/ci/sync_fixture_tables.py`, `test_mp_cifixture` | WRITTEN; CHECKED on Windows and Linux (section 5) |
| a capture check, and the iOS icons drawn opaque | `tools/apple/check_png.py`, `tools/apple/ios_icons.py` | WRITTEN and CHECKED (`tools/apple/test_images.py`) |

Where each Apple file differs from the sibling port's file it was carried from (the sibling's Apple glue ran on GitHub's macOS runners; this game's has not),
every changed line and its reason are in the table of Step 206 in `docs/planning/2026-09-11-magic-portals-remaster.md`.

## 2. What a bundle looks like, and how the entry finds the game

**All of this section is WRITTEN and CHECKED only as far as section 8 says; none of it has run on an Apple machine.**

```
MagicPortals.app/                                   macOS                          MagicPortals.app/                  iOS (a flat bundle)
  Contents/                                                                           MagicPortals            the executable
    Info.plist                                                                        Info.plist
    MacOS/MagicPortals                                                                AppIcon60x60@2x.png ...
    Resources/                                                                        engine/assets/shaders/*.spv
      MagicPortals.icns                                                               licenses/
      engine/assets/shaders/*.spv                                                     GameData/               the game's data: NOT in a shell
      licenses/                                                                         levels/  tables/chapters.json  data/  port-data/  original/  assets/ ...
      GameData/               the game's data: NOT in a shell
        levels/  tables/chapters.json  data/  port-data/  original/  assets/ ...
```

What `GameData` holds is the shape `tools/package/package.py content` writes (`docs/packaging.md`, the packaged shape). The packager's own folder is `content` (by default `<repo>/out/dist/content`); `GameData` is the name the Android packager gives it inside an APK (`tools/package/android_package.py`) and the name these Apple layouts give it inside a bundle.
A data-less shell, which is all CI can make, has none.

**The lookup** (`game/sim/AppleLayout.hpp`, run by `test_mp_applelayout`): a folder holds the game when its `levels` folder and `tables/chapters.json` exist.

* macOS, in this order: `<bundle>/Contents/Resources/GameData`, then `~/Library/Application Support/Magic Portals/GameData` (the fallback a player can drop a
  data folder into by hand; only *looked at*, never made).
* iOS: `<bundle>/GameData`.

The entry hands the folder it chose to the game through the existing `--root` flag. **If the arguments already carry `--root`, nothing is looked up** and
the game's own strict check says in words what is wrong with it (this is how CI points the program at the fixture). The game's own run-time tiers (next to the
program, the bundle, `<user data>/content`) are never reached from an Apple entry. (The game's own tier 4 is `content`, the Apple fallback is `GameData`: two names for two
different resolvers, and the entry never falls through to the game's.)

**Before the game starts**, the macOS entry (inside a `.app` only; a build-tree run is passed through untouched) makes the path flags absolute against the folder the
program was started in (**only the flags `AppleLayout::kPathFlags` lists**: `--root`, `--levels`, `--art`, `--data`, `--original`, `--chapters`, `--achievements`, `--port-data`,
`--padart`, `--saves` and `--screenshot`; the engine's other path flags, `--screenshot-ui`, `--record`, `--replay` and `--scene` (`engine/src/core/LaunchOptions.cpp:184-186, 227-235`), are handed on as
typed, so a relative value of one of them lands in the engine's working folder and not where it was typed: WRITTEN that way and not fixed in this step, APPLE-24), copies the bundle's shaders into `~/Library/Caches/<bundle id>/engine/assets/shaders` **at every launch** and makes that its working
directory. **Why:** the engine takes the executable's folder or the working directory as its asset root when it holds `assets/shaders`, and a signed
bundle is read-only (and must stay unchanged to keep its seal), while the engine writes where it runs. The iOS entry does the same into `Library/Caches/engine`.

**A start with no game data** says so in words on standard error, with every place it looked, and exits 1. On macOS a run somebody is at (no `--frames`,
`--screenshot`, `--fixed-step`, `--saves` and no other script's flag: `AppleLayout::Unattended`, the same narrow rule as `RunKind.hpp`) also shows an
**alert**; a run a script made never does, because nobody would close it and a runner would wait until its job timed out. The alert is **NOT KNOWN**: it has no precedent
in the sibling and was never shown. iOS has no alert: the engine ends the process at once with what the entry returns, so a dialog would not be seen; the words go
to the log and to the game's log file, as the Android entry's failed unpack does.

**Which flags a macOS build keeps.** A macOS player's build is not a phone's: `MAGICPORTALS_MOBILE` is OFF there and its desktop profile applies. The shell the
workflow makes is configured with `MAGICPORTALS_DEV_TOOLS=OFF` (the desktop *player's* flags: the developer's `--route`, `--tap`, `--visit-levels` ... are refused with a
sentence), `MAGICPORTALS_BAKED_PATHS=OFF` (no checkout path or account name compiled in). An iOS build is a phone's build: `MAGICPORTALS_MOBILE` ON, the developer tools off,
baked paths off, and the suites refused (the configure stops on a request for either).

## 3. How to build, with a Mac (not run)

The same commands as the workflow, with MoltenVK's static library from the release (v1.4.2, the sibling's pin) named on the command line, never baked into CMake:

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DSUPERSONIC_ENABLE_VALIDATION=OFF \
      -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=13.3 \
      -DMAGICPORTALS_BAKED_PATHS=OFF -DMAGICPORTALS_DEV_TOOLS=OFF -DGLSL_COMPILER=GLSL_COMPILER-NOTFOUND \
      -DSUPERSONIC_MOLTENVK_LIBRARY=<MoltenVK.xcframework/macos-arm64_x86_64/libMoltenVK.a>
cmake --build build --target MagicPortals
bash tools/apple/make_app.sh macos build <a folder under out/> --moltenvk-license <MoltenVK's LICENSE>
```

For iOS: `-DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphoneos` (or `iphonesimulator`) `-DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=16.3` and the matching slice
(`ios-arm64` or `ios-arm64_x86_64-simulator`), then `make_app.sh ios build <folder> --sdk iphoneos|iphonesimulator`. `GLSL_COMPILER-NOTFOUND` is deliberate: the engine rewrites
its committed `.spv` files when it finds a shader compiler, and any compiler but the Windows Vulkan SDK's writes different bytes (the sibling's record).
`make_app.sh --dry-run` prints every command and runs none. The script writes only under the folder it is given, and refuses `/`, a home folder, the repository outside `out/` and a folder that holds it.

The deployment targets (macOS 13.3, iOS 16.3) are the sibling's, and `out/parity/program/PLAT/platforms.md` section 6.2 gives them; they are given on the command line and by the workflow, and nothing in CMake invents them. (From memory of libc++'s availability macros, and NOT checked here: 13.3 and 16.3 are the first versions Apple marks floating-point `std::to_chars` available, and the game calls it once, `game/sim/Controls.cpp`.)

## 4. The workflow, and how to read what it says

`.github/workflows/apple.yml`: **dispatch only** (the repository is private; a macOS minute is billed at a multiple of a Linux one: `out/parity/program/PLAT/platforms.md`
section 5 gives both readings of the rules, neither confirmed for this account). Nothing runs on a push or a pull request. It needs **nothing but the repository**: no secret, no
key, no game data, no token beyond the default read-only one, **and the engine submodule is fetched over https from `engine/`'s URL (`.gitmodules`)**: that repository is public, and the first dispatch's checkout fetched it with no credential (section 4b; APPLE-25, answered).

**To run it:** the owner opens Actions, Apple, *Run workflow*, picks `job` (`all` by default, or `macos`, `ios-device`, `ios-sim`) and starts it. **Dispatching it is the owner's decision and
nothing here dispatched it.** The first run measures the minutes. For scale only, the sibling's runs: its release workflow's macOS job (one universal build, the zip, both runs) took 5.0 to 6.7 billed minutes, its push workflow's macOS job (one native build and the suites, ccache warm) 1.9 to 3.4 (`out/parity/program/PLAT/platforms.md` section 5.2); this game's `macos` job makes two builds, the first cold, and its layer is one very large translation unit, so expect more.

| Job | Name in the workflow | What it proves | What it cannot |
|---|---|---|---|
| `macos` | `macOS - build, test, shell, fixture` | the game compiles and links for macOS (native tree, then a universal arm64+x86_64 release-style tree); every suite is run once; the shell is assembled and verified after unpacking; the fixture level starts and draws from the unpacked zip on the runner's architecture and, if Rosetta installs, as x86_64; a start with no game data says so and exits 1 | that the game plays; that any real Mac, GPU or person is happy; Gatekeeper |
| `ios-device` | `iOS device - build, shell ipa` | the device build compiles and links, and the shell `.ipa` passes the sibling's structure checks (`Payload/` first, no AppleDouble, system libraries only, not encrypted) | anything about a run |
| `ios-sim` | `iOS simulator - build, shell app` | the simulator build compiles and links | a run: the sibling's own record is that its simulator stops at the first draw (no non-zero base-instance drawing) |

**Runner and Xcode.** Every job runs on `macos-15` (arm64) and selects the newest release Xcode the image has (not a beta), as the sibling's workflow does: MoltenVK's static library names Metal symbols an older SDK may not have. At the sibling's last measured run (the engine's README, 2026-09-28) that was Xcode 26.3 with MoltenVK 1.4.2. What the image holds on the day it is dispatched is not known here.

The jobs have no `needs:` and no matrix, so a failure in one never hides the next job's result; every job ends by uploading its configure, build and test logs, whatever happened.

**Reading the result.** In the `macOS - build, test, shell, fixture` job:

* `Test (ctest, data-less)` prints `skipped: N (never counted as passed)` and the number of suites `tests/CMakeLists.txt` marks as needing the owner's data. A skip is not a pass; the two
  numbers should be equal, and the step warns when they are not. **Any failed test fails the step** (planning step 209; the step runs `ctest` with a `--timeout 900`). Until then two suites FAILED without the owner's data
  and the step let them through by name: `test_mp_shoot` (it ran fewer checks than its floor) and `test_mp_sprites` (one case read an entity file of the original with no skip guard). Each is now a suite that runs what needs no data and a
  second suite of the same file that exits 77 for what does (`test_mp_shoot_levels`; `test_mp_sprites_original` and `test_mp_sprites_levels`), so on Linux with the data hidden `ctest` lists 80 tests and gives
  44 passed, 36 skipped (the 35 marked suites and `parity_reachability`) and none failed. **Run on Linux only**: the macOS run of 2026-10-25 above was taken before the change and gave 40 passed, 32 skipped, 2 failed (the two it let through); the
  edited workflow has not run on a Mac, and on macOS the skipped count should be 35 (no Python is given to the configure, so `parity_reachability` and the two build pins are not registered there).
  (The step's script is run against recorded `ctest` outputs by `test_workflow.py`.)
* `Run the fixture (arm64)` and `Run the fixture (x86_64 under Rosetta)` must exit 0 and write a picture (1280x720 is asked for; macOS keeps a window on the screen and the first run drew 1024x653, so `Check the picture` requires at least 640x360); `Check the picture` then decodes it and fails if it holds one flat colour
  (a black capture of a swapchain that drew nothing would otherwise pass an existence test).
* `Run with no game data (must fail in words)` must exit 1 and print the sentence that begins "Magic Portals cannot start".
* `MoltenVK (download)` checks the tarball's sha256 against `MOLTENVK_SHA256` (the hash the first run printed, the same in all three jobs; Khronos publishes none, so this is trust on first use): a different file under the same version fails the job before anything is unpacked.
* `Source tree untouched` fails if the build changed a tracked file or anything in `engine/`.

Artifacts (7 days): `macos-shell` (the zip), `ios-device-shell` (the ipa), `ios-sim-shell`, and one `*-logs` each. They hold no game data.

## 4b. What the first run showed (run 40236827494, 2026-10-25, 21:43 UTC)

One dispatch of this workflow (`job` = all) on GitHub's `macos-15` runners: macOS 15.7.9, Xcode 26.3 (build 17C529), five CPUs, 14 GB, and a Metal device called **"Apple Paravirtual device"**:
not an Apple GPU, so nothing below says how the game looks or runs on a Mac's. The jobs took 6 min 42 s (`macos`: the suites' tree 191 s, the universal shell 149 s, the fixture 8 s on arm64 and 12 s under Rosetta),
1 min 41 s and 1 min 43 s (the iOS jobs, started together); a cold ccache (205 of 205 misses). That is 7 + 2 + 2 = 11 minutes rounded up per job; at the usual ten times for macOS about **110 billed minutes**
(the account's own rule is not read here). All of the following is **CHECKED by that run** and nothing else:

* The engine repository is public: the checkout with submodules fetched it with the default token. (APPLE-25, answered.)
* MoltenVK v1.4.2's tarball has the same sha256 in all three jobs, `562a15a29bc358446a56a4091c5f7e08f604184187c1d34f712148b61ef17276`; the workflow now checks it.
* **The two entries compile and link with Apple's real SDK.** `MacMain.mm` (macOS, universal arm64 and x86_64, deployment target 13.3) and `IOSMain.mm` (iOS device and iOS simulator, arm64, 16.3) built with no error;
  `main.cpp` with the force-included rename (APPLE-15) linked in all three. (APPLE-13 and APPLE-18, answered in that direction: the compile accepted `NSAlert`, the library calls and the C++ library at those targets.
  Which branch of the floating-point `from_chars` guard the compiler took is not recorded.)
* Warnings: 16 in the iOS device build, 16 in the macOS suites' build and 32 in the universal shell build (two architectures); **all in `engine/`** (entt's `-Wmissing-field-initializers` at `storage.hpp:665` and
  `registry.hpp:639` for the engine's `meshKey` and `parameters` fields, and `-Wunused-but-set-variable` at the engine's `MeshRegistry.cpp:54`), none in `game/` or `tests/` (APPLE-26).
* macOS: the 74 suites built and `ctest` gave 40 passed, 32 skipped and 2 failed (`test_mp_shoot`, `test_mp_sprites`, the two the data-less run is known to fail: APPLE-22): the same numbers as on Linux. (That was the workflow before planning step 209, which made the two skip or pass and took the exception out: section 4.)
  The universal shell was assembled, signed ad hoc, `codesign --verify` said "valid on disk" and "satisfies its Designated Requirement" for the app unpacked from the zip, and the minimum system is 13.3.
* **The macOS shell ran.** From the unpacked zip, on arm64: `--root tests/fixtures/ci --port-data game/data --level ci_level --window 1280x720 --fixed-step --frames 300` exited 0 and the log ends "finished: clean", with
  MoltenVK reporting the paravirtual device; it wrote a **1024x653** picture of five distinct pixel values (a black clear and one rectangle): macOS kept the window on the screen, which is why the size check was loosened.
  The x86_64 half, run under Rosetta 2 (the step installed it), exited 0 and wrote a picture of the same size and the same byte count (27,002). A run with no game data exited 1 and printed the "cannot start" sentence with its two places.
  **Not** shown: that the pictures are right (nobody compared them with the Windows one, which is another size), the look of the picture, audio (the log says CoreAudio pulled frames from a device that may have been virtual),
  Gatekeeper (the files never had a quarantine flag), a real GPU, a person's Mac.
* **Two things failed, and are fixed in the commit that wrote this section.** (1) Both iOS jobs built and linked, then `make_app.sh` stopped with "No executable at build-ios-device/game/MagicPortals": CMake leaves an iOS program
  inside a bundle of its own, `game/MagicPortals.app/MagicPortals`; the script now looks there for iOS. So **no iOS shell has been assembled yet and nothing iOS has run anywhere.**
  (2) `Check the picture` asked for exactly 1280x720 and the window was 1024x653: the check now asks for at least 640x360 (`check_png.py --min-size`).
  **The second run (run 40238093331, 22:00 UTC, all three jobs green) showed the fixes hold:** both iOS jobs assembled their shells (the device `.ipa`: arm64, `MinimumOSVersion` 16.3, ad hoc signed, `codesign --verify` "valid on disk" and "satisfies its Designated Requirement"; the simulator `.app` in a zip, the same checks) and the macOS job's two picture checks passed on the 1024x653 pictures (five pixel values each, 27,002 bytes each). The `macos` job took 3 min 51 s and the iOS jobs about 30 s each on a warm cache: about 6 minutes rounded up per job, about 60 billed at ten times. The three downloaded shells hold the program, the engine's shaders, the icons, the licences and a plist: no game data (no name with GameData, level, port-data, .esc, .ent or .mp3 in any of them). **Still not shown:** that an iOS program starts (no device or simulator ran one), that anything looks right, and everything of section 12.

## 4c. The release workflow and its two runs (planning step 241, 2026-11-10)

`.github/workflows/apple-release.yml` makes the two Apple downloads of a release. **Dispatch only**, with the version, the address of the PUBLISHED Windows archive of that version and its SHA-256. It calls `apple.yml`'s `macos` and `ios-device` jobs (the shells are built in the same run), and then its own `assemble` job on `macos-15`: it fetches the Windows archive and checks the sum before unpacking, runs `package.py verify` on its `content` folder (the content package itself), assembles the macOS archive with `release.py assemble --platform macos --resealed`, **signs the app ad hoc again** (`codesign --force --sign - --timestamp=none`), verifies it, zips it with `ditto`, unpacks that zip as Finder would and verifies it again, compares the file names before and after, **starts the app from the unpacked zip with the game inside** (level 0, 120 fixed ticks, `--lang` en, de, ru, ja and ar, a wall limit on each, the picture checked and thrown away), assembles the iOS archive, gives both their fixed names (`MagicPortals-macOS.zip`, `MagicPortals-iOS.ipa`) and sums them. It uploads the two files with their sums (`apple-release`) and the logs (`apple-release-logs`), reads no secret, and creates no release: the files are downloaded and attached to the release by hand (`releasing.md`). No game data is in the repository: it comes from the release that is already published.

* **Run 42909278794 (commit `3b7f6be`), failed.** The iOS shell job was green and the macOS shell job failed in "Assemble the shell": `tools/apple/make_app.sh: line 178: printf: write error: Broken pipe`, exit 1. The line read the minimum OS from the listing of the universal binary with `printf ... | awk '... { print $2; exit }'`: awk left at its first match while printf was still writing, and under `pipefail` with `set -e` the writer's failure ended the script. It is a race, which is why the same line had passed in `apple.yml`'s and `release.yml`'s earlier runs. The `assemble` job stopped at its first step, as designed (it needs both shells). Fixed in `b485524`: awk reads all its input and keeps the first match, the `grep -q` after a pipe in the iOS check (which under `pipefail` could have turned "encrypted" into "not encrypted") reads all of its input too, `head -1` became `sed -n 1p`, and `tools/apple/test_make_app.py` has a test that fails on the old script and pins that no pipe of `make_app.sh` ends in a reader that stops early. The race was NOT reproduced here (neither Git Bash nor Ubuntu's bash lost it in ten tries); the cause is read from the log's own line number.
* **Run 42912601119 (commit `b485524`), green in all four jobs.** What its logs show: the Windows archive's sum matched, `package.py verify` said `ok` (923 files, 26,968,364 bytes, 0 bad hashes); before the second signing Apple's `codesign --verify --deep --strict` said "a sealed resource is missing or invalid" (the packager's data had broken the first seal, as section 7 said it would); after it, on the app unpacked from the finished zip, "valid on disk" and "satisfies its Designated Requirement", `Signature=adhoc`, `TeamIdentifier=not set`, "Mach-O universal (x86_64 arm64)", 949 sealed resources, and the same 988 file names before and after the signing (the seal's folder apart); `spctl --assess` said "rejected" (an ad-hoc app names no developer: expected). The five starts of the app each read the data from `Contents/Resources/GameData` (`tier: --root, layout: packaged`), logged `language: <code>` and read `level0` in a few milliseconds, baked their fonts (Russian, Japanese, Arabic: two atlases each), and ended `finished: clean`, exit 0, with no `ERROR`, `WARN`, "refused" or "cannot read" line; the GPU was "Apple Paravirtual device" through MoltenVK 1.4.2 on macOS 15, and the log says Vulkan validation was not loaded. The two files: `MagicPortals-macOS.zip` 35,929,708 bytes and `MagicPortals-iOS.ipa` 29,143,315 bytes.
* **Checked again on the PC afterwards** (not by the workflow): both sums match `SHA256SUMS.txt`; both archives' 923 game files are byte-equal to the content package (the package's own `manifest.json` and `SHA256SUMS.txt` are in the app too, as in the other archives); the property lists name `com.ivancvetanovic.magicportals`, the project's version, `LSMinimumSystemVersion` 13.3 (macOS) and `MinimumOSVersion` 16.3 with `UIDeviceFamily` 1 and 2 and `arm64` and `metal` required (iOS); the programs are mode 0755 in the zips, with no `__MACOSX`, no `.DS_Store` and no private name or path in any byte (the word "password" that a scan flags in both programs is Dear ImGui's demo widget, which the Windows and Linux programs hold too).
* **What the run does NOT show.** A person's Mac; a real Apple GPU; how it looks, plays or sounds (the runner has no audio device); any level but level 0; the Intel half with the game inside (the runner is an arm64 machine, so it was the arm64 half that started, which is inferred from that and not shown by a log line; the x86_64 half has only run under Rosetta on the fixture, in `apple.yml`); macOS 13 and 14; Open Anyway or any other way past Gatekeeper; and **anything about the iOS file**: no device, no simulator and no runner has started it, and no sideloading tool has been given it.

## 5. The fixture level

`tests/fixtures/ci/` is a level written for this purpose: a room of four static boxes (a floor, two walls, a ledge), a pressure plate, a gate the plate opens, a start and an exit, with no picture, and no
geometry, number or name taken from a level of the original (its node and role names are invented: `ci_start`, `ci_gate`, `ci_plate` ...; the suite checks that none appears in `data/entity_roles.json`). Hand-written `.tscn` and `.json` only.
**One exception, and it is the owner's to ratify:** the folder's two copied tables (below) carry the numbers decoded from the original, and their notes name files of the original
(21 lines in all: 20 of `tests/fixtures/ci/data/player.json`, for example lines 4, 16 and 59, and 1 of `portals.json`, line 26; counted with `grep -cE '\.ent|reference/|out/parity/original|\.png|\.esc|\.enml'` on each file), so the sentence "no number, name or file from the original" is not true of the folder as a whole.

```
MagicPortals --root <repo>/tests/fixtures/ci --port-data <repo>/game/data --level ci_level --window 1280x720 --fixed-step --frames 300 --screenshot <absolute path>.png
```

**Why `--port-data` is on the command.** `--root` fills every path the game reads from the root alone, and the game refuses to start a level when one of its tables is missing: it reads 45 tables of the port's own
(`game/data`) and three of Enhanced's (`data/`: the role table, the player's tuning, the portals' rules). A fixture that was complete by itself would hold copies of all 48, which would put the notes of the port's tables (they name other
projects, and the original's makers) into the repository a second time. So the folder holds only what is the fixture's, and the run names the rest: the port's tables from the checkout by `--port-data`.

**Why the folder holds two copies anyway** (`data/player.json`, `data/portals.json`): `--data` would replace the whole `data` folder, and with it the fixture's own role table (invented names, so that no name of the original is in the level), so the two
tables of Enhanced's that the game also reads from `data/` are byte-for-byte copies (`tools/ci/sync_fixture_tables.py` refreshes them, `--check` compares) and `test_mp_cifixture` fails when they drift. They carry the port's decoded numbers (the player's physics, the portals' rules, decoded from the original) and notes that name the original's files; they are copies of tables this repository already tracks, so nothing extracted is added to it. An alternative that
carries none would mean inventing a player, which the fixture does not do (LEFT FOR THE OWNER 8 in Step 206, APPLE-20). **The cost:** whoever changes either table must run that command too.

The fixture can only say "the shell starts, loads a level, ticks and draws"; there is no art, so the picture is a few placeholder boxes on black.

## 6. What a bundle carries of the licences

`make_app.sh` copies, into `Resources/licenses` (iOS: the bundle's `licenses`): the repository's `LICENSE` and `NOTICE.md` (the game's licence, and the notices of the software it contains, with the
notices of the two places where the tools follow Ethanon's and AngelScript's source: planning step 210), the engine's `LICENSE` and `THIRD_PARTY_LICENSES.md` (which names every library the engine vendors,
GLFW and the stb and dr_libs headers among them), MoltenVK's licence text when `--moltenvk-license` hands it over (MoltenVK is Apache-2.0 and is **linked into the program**; the engine's list does not
name it, because the engine links it only when a build asks), and a `README.txt` that says which file is which. The licence is the one proposed in planning step 210 until the owner confirms it (D10). The
sibling's licence list (its own MIT-0 file, its LGPL and GPL texts) is for its game logic and is not carried.

## 7. Getting the data into a shell: the three routes, one of them taken

**Taken (planning step 241, 2026-11-10): route 1 on a hosted runner**, by `apple-release.yml` (section 4c). The data it sent to a machine that is not the owner's is the content package of a release that was already public, taken from that release's own Windows archive. The seal made again held under Apple's own `codesign --verify --deep --strict`, on the app unpacked from the finished zip. Routes 2 and 3 below were not tried and remain alternatives. What the owner decided and what the run showed are two different things: the file now exists and verifies on a GitHub Mac; it has not met a person's Mac.

A macOS bundle's ad-hoc signature covers every file of the bundle: **a `GameData` folder added to `Contents/Resources` after the signature is made breaks the seal** ("is damaged and can't be
opened", the sibling's note), so a data-bearing zip needs the seal **made again after the data is in**. Nothing here decides how; the route is the owner's to choose (planning record D1 and D2):

1. **A Mac assembles and signs it** (his, a borrowed one, or a hosted one, which sends the data to someone else's machine). Not run: no Mac.
2. **A cross-platform ad-hoc signer on the Windows PC** (for example Apple-codesign's `rcodesign`) re-signs the bundle after the data is added. **Untested**: nothing has done it, and whether macOS accepts the result is NOT KNOWN.
3. **The shell plus a folder the player drops into `~/Library/Application Support/Magic Portals/GameData`.** Needs no signer; clumsy; untested. (A folder *beside* the `.app` is rejected: an app run in place from a
   quarantined download is run from a randomised read-only path by App Translocation, where a sibling folder is expected to be invisible: NOT KNOWN, never tested.)

On iOS the data goes **inside** the bundle (`GameData` at its root): the packager unzips the CI's `.ipa`, adds the folder and zips it again, and the sideloading tool re-signs the whole bundle, so the ad-hoc seal not covering the new files is harmless: inferred, never run. Since planning step 208 `tools/package/release.py assemble --platform macos|ios` (`docs/packaging.md` section 8) does the adding for both platforms, deterministically, and says in the archive that the macOS seal is invalid and must be made again; it was run on synthetic bundles only (the real shell zips were not on the PC) and has never been run on a Mac.

## 8. What was checked, by which tool

On this PC (Windows 11, MSVC 14.50; WSL Ubuntu 24.04 with g++ 13.3, clang 18.1.3, libc++ 18; the NDK r28.2):

* **The Windows program does not move.** `build/game/MagicPortals.exe` was not rebuilt and not relinked (md5 04dc77e2... before and after, same modification time). The build files of Windows and Linux are the same:
  two scratch configures of the same sources with the three CMake files of HEAD and of the tree: `build.ninja`, `rules.ninja` and `compile_commands.json` equal except for the two new suites' compile and link steps (`out/parity/program/PORT/PORT8/impl/g6/`).
  `ninja -t inputs` of the program lists sources, objects and libraries and **no header**, so its empty intersection with the changed files proves nothing about headers; the equal commands do.
* **The Apple branches of CMake** were run on Linux as a **simulation**: `APPLE` (and for iOS `IOS`) set by hand, clang as the Objective-C and Objective-C++ compiler, an empty file as the MoltenVK library: configure only. It showed the entry in the target,
  `-include .../RenameDesktopMain.hpp` on `main.cpp` alone, `-fobjc-arc` on the `.mm`, `-framework Foundation` (and `AppKit` for macOS), the plist written with the version, and the two refusals (baked paths ON and the suites ON for iOS). **It is not an Apple configure**
  (no SDK, no Apple linker); it shows that this repository's own CMake logic runs.
* **The two `.mm` files** were parsed as Objective-C++ by clang 18 on Linux against **stand-in `Foundation.h` and `AppKit.h`** written for the purpose (`impl/objc_stubs`), with ARC: 0 errors and 0 warnings. The stand-ins are the implementer's memory of Apple's declarations: this
  checks the files' syntax and their C++ calls into this repository, and says **nothing** of Apple's real SDK. A deliberate typo in each file was refused, so the check is not empty.
* **`main` is renamed on Apple only, by a force-included header.** `main.cpp` defines `main()` for every platform but Android, and a macOS program has its own (`MacMain.mm`) and an iOS one has the engine's: a second `_main` would be a duplicate symbol in both links. Editing `main.cpp` is out (it feeds the Windows
  program), so `game/apple/RenameDesktopMain.hpp` is force-included into it alone: `#define main(...) MagicPortalsDesktopMainUnusedOnApple(__VA_ARGS__)`. It is function-like because `MagicPortalsLayer.hpp` has a member called `main` that an object-like `-Dmain=` would also rename in this one translation
  unit, and because CMake **drops** a function-like `-D`. A preprocessor run counts exactly one replacement under g++ and clang (with libstdc++ and with libc++): the definition. `test_mp_applelayout` fails if `main.cpp`'s guard gains `__APPLE__` (then the header and its CMake line are to be deleted).
* **Apple's C++ library**, by proxy: clang 18 with libc++ 18 on Linux (section 9).

## 9. Apple's libc++ and SDK: the proxy, and the table of what is gated

Every `game/` translation unit and every suite was compiled with `clang++ -stdlib=libc++ -Wall -Wextra -ffp-contract=off` (libc++ 18 has no floating-point `from_chars`, which came in LLVM 20; the research of this program inferred, without running it, that Apple's library gates it behind a newer OS, and the game's `ParseDouble` falls back where the library says it has none): the counts are in
Step 206. That is a proxy for Apple's library, **not Apple's library**: Xcode's libc++ carries availability annotations (a function exists but is marked unavailable below an OS version) that no Linux compile shows.

| What Apple's library or SDK is known to refuse or gate | Present in `game/`? | What the code does | Not known |
|---|---|---|---|
| floating-point `std::from_chars` | in `sim/Numbers.cpp` (one call) and the integer `from_chars` in `sim/Tscn.cpp` | `Numbers::ParseDouble` calls it only where `__cpp_lib_to_chars` says the library has it, and otherwise a fallback pinned against it by `test_mp_numbers` (libc++ 18 takes the fallback) | whether Xcode's SDK defines the macro and then marks the function unavailable at 13.3/16.3 (a compile error the first dispatch would show) |
| floating-point `std::to_chars` | `sim/Controls.cpp:441` | none: it is called as the library offers it; the deployment targets are the sibling's | whether the SDK marks it unavailable at 13.3/16.3 (a compile error the first dispatch would show) |
| `std::format`, `std::jthread` and `stop_token`, chrono calendar and zone types, `std::ranges`, `<execution>`, `std::pmr`, `osyncstream`, coroutines, `std::source_location` | none (grepped) | n/a | n/a |
| atomic `wait`/`notify`, `latch`, `barrier`, semaphores | none | n/a | n/a |
| aligned allocation | none | n/a | n/a |
| `std::filesystem` (copy, rename, directory iteration, `temp_directory_path` ...) | the unit is a line of a source file (`*.cpp`, `*.hpp`, `*.mm`) under `game/`: 134 lines in 23 files name `std::filesystem::` (`grep -rn --include='*.cpp' --include='*.hpp' --include='*.mm' 'std::filesystem::' game`; the manifest `game/MagicPortals.manifest` also holds the text once and is not a source), 21 files under `game/` include `<filesystem>` (`grep -rl '#include <filesystem>' game`), and 4 files (the Android, macOS and iOS entries and `sim/Unpack.cpp`) reach it through a `namespace fs = std::filesystem` alias on 38 more lines, in the layer, the sim and the entries; of the operations the row names, counted as lines that CALL them (a comment that names one is not counted): `copy_file` 4, `rename` 7, `directory_iterator` 2, `recursive_directory_iterator` 1, `temp_directory_path` 2 (`main.cpp`, `spike/main.cpp`) | `std::error_code` overloads; macOS has had it since 10.15 | behaviour on APFS |
| `std::filesystem::path` conversions (wide, u8) | `std::wstring` only in `main.cpp` under `_WIN32` | n/a | n/a |
| `getenv` and a hard-coded `/tmp` | none in `game/` | the scratch folder is chosen by trying a folder (`sim/Scratch.hpp`), `temp_directory_path` on macOS is `$TMPDIR` | whether a sandboxed app can write where it is told |
| case-insensitive file names | the original's file names are used with their own case; the case audit found 0 mismatches in 849 level references | n/a | APFS case-insensitive volumes are the default; a case-sensitive one would behave as Linux does |
| `std::span`, `<numbers>`, `<bit>`, `starts_with`, `<=>` | yes (12, 8, 2, 10, 1) | all in libc++ 18 | availability marks (none known for these) |

Items for the engine's list (the engine is read-only here): no gap that this step hit. Old warnings that are the engine's, found by the step's verifier and not caused here: building the engine with g++ 13 prints 21 warning lines from `engine/third_party` (entt, stb), and with clang 18 about 6,200 `-Wnullability-extension` lines, almost all from the VMA header; none is in `game/` or `tests/`.

## 10. Decisions that are the owner's, and left

* the bundle identifier (a placeholder shared with the Android package), the display name (`Magic Portals`; the Android label says `Magic Portals Enhanced`), the icon (a placeholder drawn by arithmetic), the version (`0.1.7`);
* signing: ad hoc only; a Developer ID and notarisation (a paid account), and whether to publish through TestFlight or the App Store at all;
* the export-compliance statement (`ITSAppUsesNonExemptEncryption`): not claimed in either plist; it is a declaration of the owner's;
* `UIFileSharingEnabled`: ON in the iOS template as in the sibling's (the flags file lives in Documents); it shows Documents in the Files app;
* whether to dispatch the workflow, and when; the minutes it costs;
* which of the three routes of section 7 makes a data-bearing macOS zip.

## 11. The sideloading story for the iOS shell, as the sibling documents its own

Said as the sibling's, **NOT KNOWN here**: the `.ipa` is signed ad hoc only; a free sideloading tool (Sideloadly on a computer, AltStore or SideStore) re-signs it with the player's own Apple ID when it installs it; with a free Apple ID the app
stops opening after 7 days until it is refreshed or installed again, and a free Apple ID can have 3 sideloaded apps at a time. Nobody has installed this game's ipa anywhere.

## 12. What nobody knows (APPLE-1 and following in `docs/parity-backlog.md`)

Gatekeeper and App Translocation; notarisation; the sandbox and writes to Application Support; retina scaling; MoltenVK on real Apple GPUs (the runner's is a paravirtual device) and on an Intel Mac; touch on an iPad; the safe area and the home indicator;
a first frame on any device; Rosetta; re-signing the ipa; TestFlight and the App Store; the data-bearing zip; that the two entries compile with Xcode; the alert; the silent failure after the entry (the game's own failure dialog is empty on Apple); the
cost of a dispatch; MoltenVK's checksum; CoreAudio and the MP3 decoder; Apple's libc++'s availability marks; the placeholder identity; whether the engine's repository is public (APPLE-25).

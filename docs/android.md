# Magic Portals Enhanced on Android: how it is built and packaged, and what has been run

**Status, plainly: built, packaged, statically checked, and RUN ON AN x86_64 EMULATOR on the maker's PC (planning step 204). NOT RUN ON ANY PHONE.**
The emulator proves startup, the unpack, the assets, input, the lifecycle and the Android code paths. It proves nothing about a phone's speed, its
GPU's behaviour, heat, memory pressure, sound, real two-finger play, notches or edge gestures, or about arm64 code (section 1). Every statement below says
whether it was *run* (a tool printed it), *read* (taken from code or documentation) or *not known*.

The record of the steps is [`planning/2026-09-11-magic-portals-remaster.md`](planning/2026-09-11-magic-portals-remaster.md), Steps 203 (the build) and 204 (the
first run). The data the APK carries comes from the content packager ([`packaging.md`](packaging.md), [`tools/package/README.md`](../tools/package/README.md));
the Windows and Linux builds are in `packaging.md` and the README.

## 0. What ran on the emulator, and what did not

The emulator is `MP_Android_API33_x86_64` (Android 13, API 33, `google_apis` x86_64, 720x1280 at 320 dpi shown in landscape, 2 GB, 2 cores, software
Vulkan through SwiftShader, which reported Vulkan 1.2.0), driven only by [`tools/android_emulator.py`](../tools/android_emulator.py). Every number below is
"SwiftShader x86_64 emulator on this PC" and is not a phone's. The implementer's run and the independent verifier's retake used two identical AVDs (the second is `MP_Android_API33_x86_64_v`, serial `emulator-5566`); where their numbers differ, the table gives both.

| | Result (step 204; the implementer's run is in `out/parity/program/PORT/PORT6/impl/`, the verifier's in `verify1/`) |
|---|---|
| **Install** | the data-bearing debug APK installs (v2 and v3 signature only, no JAR signature); `install -r` of the same APK keeps the data and does not unpack again |
| **The first-run unpack** | 900 files, 24,707,748 bytes, **1.3 to 1.65 s** (first progress line to "Unpacked", the implementer's three runs; the verifier's seven takes: 1.05 to 1.79 s); the first frame 3.6 to 3.7 s after `am start` (the verifier's: 2.67 to 5.06 s); the second start has no unpack and reaches its first frame 1.5 to 3.5 s after `am start` (the verifier's: 1.04 to 4.81 s) |
| **The unpacked tree** | every one of the 900 files has the size and sha256 of `manifest.json` (883) and of the engine's shaders (17); 0 missing, 0 wrong; the only other file inside the owned folders is the engine's `engine/cache/pipeline_cache.bin` |
| **Killed mid-unpack** | a `force-stop` after 469 of 900 files (the verifier's: 481) left no stamp; the next start unpacked again and the tree is identical to a clean one |
| **First frame, menu, a level** | the menu renders; levels 1-01 and 4-32 start from `files/magicportals_args.txt` (`--level`); the modern touch controls are drawn inset in the corners |
| **Touch** | a held walk arrow moves the player; a tap shoots (`shot fired on tick N`, then `sound portal_placed`) and places a portal; no "not responding" dialog appeared |
| **Back, Home** | Back in a level pauses and Back again resumes; Back in a menu goes back; Back on the main menu quits; Home then resume rebuilds the swapchain (no black screen) |
| **Configuration changes** | seven changes were made while a level ran (font scale, font weight, night mode, the app's language, density, the display size, airplane mode) and none recreated the activity (section 2); Step 203's APK, which lacked four kinds, was destroyed by a font-weight change. The kinds of the mask `0x5000ffff` that no command triggered: `layoutDirection`, `colorMode`, `keyboard` and `keyboardHidden`, `navigation`, `touchscreen`, `grammaticalGender` (API 34; this image is API 33), `mcc` and `mnc` |
| **A 20:9 screen** | `wm size 2400x1080`, `wm density 420`: the game fills the window (swapchain 2400x1080), the controls sit in the corners, nothing is clipped; a display-cutout overlay changed nothing (safe area 0 on every side) |
| **A score** | a replayed winning route wins 1-01 on tick 217 and writes `files/Magic Portals/scores.json` (medal 3); it is still there after a force-stop and a plain restart, and the level select shows the medal |
| **128 levels** | `--visit-levels all`: 128 of 128 start, none "did not load"; 1.65 s a level (1.49 to 1.73 s), 215 s in all (the verifier's run on the final APK: 2.4 s a level, 304 s in all, while its own Windows sweep ran on the same PC) |
| **128 routes** | the 128 recorded winning routes: **128 of 128 won**, each on exactly the tick Windows wins it on (the tick of `test_mp_routes`'s census); 65 minutes of route time; this is a determinism data point for Android **x86_64** (bionic's libm), not for arm64. The implementer ran them on the first build's verification APK, and a sample of 17 on the final one (all 17 won on the Windows ticks); **the verifier then replayed all 128 on the final build's verification APK (`cd09e464581d`): 128 of 128 won, each on exactly the tick of its own Windows census** (6,349 s of route time with its own Windows and WSL work running beside it, the longest, 4-32, 221 s on tick 3264, no emulator incident during the routes) |
| **An update** | a same-stamp APK installed over keeps everything; another stamp unpacks again, removes a stray file in the owned folders and leaves the saves; uninstalling removes the data |
| **Failures** | a cut list, a missing entry, an unreadable entry and a short entry each show a dialog in words and end with exit status 1 (`EXIT_SELF`); a new copy that cannot fit is refused with the old copy untouched; a fresh install with 15 MB free is refused; the failure's words are in the game's log file and in the report Android shows at the next start (section 4) |
| **A native crash** | `kill -SEGV` leaves a report at the next start: "crash (native) (5)", a status field (11 in the implementer's run, 0 in the verifier's), "in the foreground" and the end of the game's log (its title says "Penumbra": an engine string, section 7) |

**Not run, and not claimable from an emulator:** a phone's frame rate, memory, heat or battery (the one `dumpsys meminfo` sample, 55 MB PSS in level 1-01, is an
emulator figure); a real GPU or driver, and any phone that reports Vulkan 1.1 only (this emulator reports 1.2, so the floor of 1.1 was logged, `minimum Vulkan 1.1`,
and not exercised); sound from a speaker (the AAudio stream opens, nobody listened); real two-finger play, edge back-gestures and real notches (`adb shell input`
injects one pointer); arm64 execution (the image lists only x86_64, so the arm64-only APK is refused with `INSTALL_FAILED_NO_MATCHING_ABIS` and no arm64
library was ever loaded); a 16 KB-page device; the `mcc` and `mnc` configuration change (declared and shown in the built manifest, but no command of the shell
changes it on this image); a release key; Google Play. **The three things only a phone can say:** how fast and how hot it runs on a real GPU (and whether a
Vulkan 1.1 driver draws it right); whether real fingers, gestures and speakers behave; and whether arm64 code (and 16 KB pages) runs.

## 1. What you need

| | Used here (run) | Why |
|---|---|---|
| Android SDK | `%LOCALAPPDATA%\Android\Sdk`: platform 35, build-tools 35.0.0 (aapt2, zipalign, d8, apksigner), cmake 3.22.1 and its Ninja, platform-tools, the emulator, cmdline-tools | the manifest's `targetSdk` is 35, so its `android.jar` is the compile target |
| Android NDK | r28.2 (28.2.13676358), clang 19 | r28 or newer: it links 16 KB-aligned by default, which newer phones demand |
| JDK | 17 (`JAVA_HOME`) | `javac` for the engine's Java activity, `java -jar` for d8 and apksigner |
| Python | 3.8 or newer, standard library only; `tools/android_emulator.py` runs on a **Windows host only** (it calls `ctypes.windll`, PowerShell's `Get-CimInstance`, `taskkill` and `creationflags`) | `tools/package/android_package.py`, `tools/android_emulator.py` |
| the game's data | `tools/package/package.py content ...` output | built from **your** copy of the original; none of it is in this repository |
| a signing key | the machine's standard `~/.android/debug.keystore` for a debug APK | a release key is **yours to make** (section 6) |
| an emulator | the system image `system-images;android-33;google_apis;x86_64` | to run it: section 7 |

`tools/build_android.sh` finds the SDK and NDK itself (`--sdk`, `--ndk`, `$ANDROID_HOME`, `$ANDROID_NDK_HOME`, else the newest NDK in the SDK) and
says what it found. It was run under Git Bash on Windows 11. **The Linux and macOS paths of the script were written from the same rules and have
never been run.**

## 2. Build and package

```
python tools/package/package.py content --repo . --reference reference --out out/dist/content     (once; docs/packaging.md section 3)
tools/build_android.sh --data out/dist/content                                  (Git Bash; or tools\build_android.bat --data ...)
```

That builds `libMagicPortals.so` for `arm64-v8a` and `x86_64` in `out/build/android-<abi>` (always under `out/`, the one part of the repository
that is ignored), strips them, and calls the packager, which writes `out/android/MagicPortals-<version>-debug.apk`, `SHA256SUMS.txt` and
`play_store_512.png`. Options (`--help` lists them all): `--abi`, `--no-data` (a shell with no game in it, which is what CI can build),
`--verification`, `--release`, `--version-code`, `--jobs`, `--out`, `--build-only`, `--package-only`.

* **The program** is the CMake target `MagicPortals`, the same name as on a desktop, built as a *shared library* by `game/CMakeLists.txt`'s
  `if (ANDROID)` branch from `main.cpp` and `game/android/AndroidMain.cpp`. The two build facts (`MAGICPORTALS_MOBILE`, `MAGICPORTALS_DEV_TOOLS`)
  reach both files by the same `target_compile_definitions` line (`game/BuildProfile.hpp` stops a file that lacks them). For an Android
  configure CMake **forces** what a phone needs: `MAGICPORTALS_MOBILE` ON (a configure that turns it OFF stops), `MAGICPORTALS_BAKED_PATHS` OFF
  (ON stops: there is no repository on a phone), no suites, no measurement tool. The script also passes Vulkan validation OFF, the committed
  shaders as they are (`GLSL_COMPILER=OFF`), `ANDROID_STL=c++_static` and `ANDROID_PLATFORM=android-<minSdk>`, where minSdk is read out of the manifest.
* **The version** is `project(MagicPortals VERSION ...)` in the top-level `CMakeLists.txt`, the one definition; the packager reads it and
  aapt2 writes it in as `versionName`. **`versionCode`** is `--version-code` (default 1): a whole number that goes **up by one with every
  release**, or Android refuses the update. Neither is written in the manifest.
* **The manifest** (`game/android/AndroidManifest.xml`): application id `com.ivancvetanovic.magicportals` (a **placeholder** the owner
  confirms, section 6), label "Magic Portals Enhanced", minSdk 26 (AAudio), targetSdk 35, `uses-feature` Vulkan 1.1 required, touchscreen and
  gamepad not required, landscape locked, **all 18 configuration-change kinds that the android-35 platform defines handled in place** (a recreated
  NativeActivity would start its native thread again over a running game): `mcc mnc locale touchscreen keyboard keyboardHidden navigation orientation
  screenLayout uiMode screenSize smallestScreenSize density layoutDirection colorMode grammaticalGender fontScale fontWeightAdjustment`, which the built
  APK states as `0x5000ffff`. Step 203 listed 14 and missed four; **run:** its APK was destroyed (the process ended) by `settings put secure font_weight_adjustment 300`
  while a level ran, and this APK is not. The packager requires all 18 offline and, at package time, every kind the SDK's own `attrs_manifest.xml` lists for
  the target (so a newer `targetSdk` that adds one cannot be built until the manifest names it), and `inspect` reads the mask back out of the built APK.
  No permissions, `allowBackup` false, and the engine's post-mortem report meta-data. The packager checks every one of those on every build
  (`read_manifest`) and refuses a manifest that has a permission or spells a version.
* **The signature** is APK Signature Scheme v2 and v3 (the debug key by default). There is **no JAR (v1) signature** (`apksigner --v1-signing-enabled false`):
  the minimum SDK is 26 and v2 is read from Android 7.0, so the three `META-INF` files that Step 203's APKs carried (221,636 bytes) served no phone this
  app installs on. **Run:** the APK verifies (v2 true, v3 true) and installs on the API 33 emulator; `inspect` refuses an APK that carries `META-INF`.
* **The icon** is drawn at package time by `tools/art/make_app_icon.py` (the five launcher sizes and the 512 px store icon) into the work folder;
  nothing of the kind is committed. Its colours are its own choice.
* **The variants.** A *player's* APK has `MAGICPORTALS_DEV_TOOLS` OFF: none of the developer's flags (`--route`, `--tap`, ...) exist in it.
  `--verification` builds the other kind (DEV_TOOLS ON, trees `out/build/android-<abi>-verification`, APK `MagicPortals-<version>-VERIFICATION-debug.apk`)
  **for driving an emulator run with `files/magicportals_args.txt`; it is never signed with a release key** and must never be mistaken for the shipped file.
  It differs from the shipped one in exactly the developer flags (and the library, 10,611,384 bytes against 10,502,440).

## 3. What is in the APK

| Entry | From |
|---|---|
| `AndroidManifest.xml`, `resources.arsc`, `res/mipmap-*/ic_launcher.png` | aapt2, from the manifest and the drawn icon |
| `classes.dex` | the engine's Java (`engine/src/platform/android/java`): javac for Java 8 bytecode, then d8 |
| `lib/<abi>/libMagicPortals.so` | the stripped libraries, each checked as an ELF file (machine, only these NEEDED libraries: `libandroid`, `liblog`, `libvulkan`, `libaaudio`, `libc`, `libm`, `libdl`; every LOAD segment aligned to 16 KB; no executable-and-writable segment; no text relocation) |
| `assets/gamedata.list` | the list the first-run unpack reads (below) |
| `assets/GameData/**` | exactly the files of the content package's `manifest.json`, re-hashed against it |
| `assets/engine/assets/shaders/*.spv` | the engine's 17 compiled shaders (the GLSL sources are not taken) |

On the owner's data (2026-10-24, step 204's own run of the content packager and the Android packager; it reproduces Step 203's figures): **883 content files,
24,589,032 bytes** (23.45 MiB) plus **17 shaders, 118,716 bytes**: **900 files, 24,707,748 bytes** in the list; the data-bearing APK is **29,978,925 bytes** (28.59 MiB),
the verification APK 26,005,727 bytes, the stripped libraries 9,591,424 (arm64-v8a) and 10,502,440 (x86_64) bytes.

Nothing else can get in: the packager reads the *package folder* only through its manifest (a listed file that is missing, short, altered, a link, or
resolves outside the folder is refused, so is a file the manifest does not list, and so is anything on the never-package list: the original's scenes,
`.pvr`, `.pkm`, `.par`, `.enml`, `.exe`, `.dll`, `.as`, an APK, a build folder), and the shaders and Java only from the named engine folders. A package
folder under `reference/apk` or `reference/ethanon` is refused. The APK and everything it is made from are under `out/`; a script that writes into
the tracked tree is refused.

The assets are added by the packager itself, entry by entry, with a fixed date and fixed attributes, after aapt2 has made the manifest and resources
(not with `aapt2 -A`: whether aapt2 applies its default ignore patterns to dot-files and underscore folders was not found out, and the list must equal
the assets exactly; the packager checks that it does). Pictures and sounds are stored, the libraries and everything else deflated; the manifest says
`extractNativeLibs="true"` (the sibling port's choice), so the installer extracts the libraries of the phone's own ABI to disk (about 10 MB more storage) and
`zipalign -P 16` has nothing to page-align inside the zip: the 16 KB property that matters is in the ELF segments, which `llvm-readelf` and the packager's ELF reader
both show (every LOAD aligned to 0x4000). Two packager runs on the same inputs give **byte-identical APKs** (run again in step 204); the compressed bytes are those of
the machine's Python zlib, so two machines may differ and one machine does not.

## 4. The first run: the unpack

The engine cannot read an APK (`grep AAssetManager engine/src` finds nothing: every open is by path). So the app copies its data out of the APK into its
private files folder once. All the decisions are `game/sim/Unpack.{hpp,cpp}`, pure code with a suite (`test_mp_unpack`, 509 checks); `game/android/AndroidMain.cpp`
is the thin glue (`AAssetManager` as the source, `Unpack::FsSink` as the folder, the dialog, the pump): since step 204 the rules for the flags file, the dialog, the
exit after the unpack, the argument list and the log of a failed start are in `Unpack` too, where a host compiler builds them and the suite runs them.

* **The list** (`assets/gamedata.list`): line 1 the *stamp*, 32 hex digits, the first 128 bits of the sha256 of every listed file's path, size and sha256 (it
  moves when any byte moves); then `size TAB path` for each file, paths relative to the files folder: `GameData/<package path>` and
  `engine/assets/shaders/<name>.spv`. Blank lines and CRLF are tolerated; there is no comment syntax. The packager and the parser spell the format twice, and a
  test ties them: it reads the constants out of `Unpack.hpp`, and the list the packager makes for four invented files is parsed (and unpacked) by the C++ suite.
  The list's text is at most 4 MiB, read as the bytes arrive; the sizes, rounded up to 4 KB blocks, must add up to less than 64 bits (a list that adds to
  exactly 2^64 - 1, which step 203's check let through, is refused).
* **The rule.** The stored stamp (`files/magicportals_gamedata.stamp`) equal to the list's *and* every listed file there at its size: nothing is done. Otherwise
  **free space is checked first**, from the list's sizes in 4 KB blocks, counting what the old copy holds as given back (so an update that would fit once the old
  copy is gone is not refused), and a copy that cannot fit fails the start **with the old copy and its stamp untouched**; then the stamp is removed, then the
  two folders the unpack owns (`GameData`, `engine`), then every file is copied and checked against the list (a chunk that would pass a file's listed size is
  refused as it arrives, not at the end of the file), and the stamp is written **last**. A process killed from the first removal on leaves no stamp, so the next
  start unpacks again from the top. The player's saves, the log, the flags file and the stamp are *beside* the owned folders and are never touched.
  The words of a refusal round the need up and the free figure down ("needs about 26 MB and 15 MB are free"; **run**).
* **A hostile or damaged list** must not write outside the folders: every path must be relative, free of `..`, `.`, empty segments, backslashes, colons,
  control characters and NUL, not end in `/`, be unique, and begin with `GameData` or `engine`. A bad list is refused before anything on the disk is touched.
* **The event pump.** The engine's activity waits for the game thread to read its lifecycle commands, and a thread that does not for about five seconds is
  "not responding". The unpack calls `Android::PumpEvents()` after every 16th file. If the app is asked to finish meanwhile the unpack **stops without an
  error** (no further file, no stamp, no dialog).
* **No progress picture.** The engine's window does not exist before the game starts, and drawing on the native window first would clash with the Vulkan
  surface made later (read, not tried). **Run:** the whole unpack took 1.3 to 1.65 s on the emulator, so the black screen of a first run was not noticeable there; on a
  phone with slow storage it is **not known**.
* **A failure** (no space, an unreadable asset, a bad list) is a dialog that says what happened in words, exit status 1 without a crash, **and a line in the
  game's log file** `files/Magic Portals/scratch/magicportals.log`, so that the report Android 11 and later shows at the next start (`supersonic.reportLog`)
  carries it (**run**, all four of cut list, missing entry, unreadable entry and short entry). A *scripted* run (a flags file present) never meets the dialog. The next start
  after such an end shows that report first ("the game ended itself with an error (1), status 1"), and the game's start waits behind its OK button.
* **Where things go.** The game's data: `files/GameData/`, the shaders `files/engine/assets/shaders/`, the working directory `files/engine/` (the engine
  looks there for `assets/shaders`, and writes its pipeline cache there, which a re-unpack removes with the folder). The player's saves (`scores.json`, `audio.json`,
  `achievements.json`, `controls.json`): `files/Magic Portals/`, which is where the engine puts a user's data on Android (internal storage; **run:** it survives a
  force-stop and an update, and uninstalling removes it). The log and the generated files: `files/Magic Portals/scratch/` (`Scratch::ArrangedFor` puts it first on a
  phone, because the NDK's C++ library names `/data/local/tmp` as the temporary folder and an app cannot write there), so the log is always
  `files/Magic Portals/scratch/magicportals.log`, the path the manifest's `supersonic.reportLog` names (a test ties the two).
* **Developer flags.** A phone has no command line. `files/magicportals_args.txt` (whitespace-separated: one flag per line, or several on a line; double quotes
  around a token with spaces) is read at start and appended after `--root files/GameData`; the file is renamed `magicportals_args.used` once read, so one scripted
  run is not every later launch (also when it is empty, unreadable or broken; when the rename fails the flags are still used and the log says the file will be
  read again). Only a debuggable APK lets `adb run-as` write the file; a release APK has no way to receive flags. No `--saves` is passed: it is a developer's
  flag and makes the run "not a player's", which suppresses the dialog that a phone with no usable Vulkan needs.

## 5. When Vulkan is missing (read, not run)

`libvulkan.so` is a NEEDED library of the program (run: `llvm-readelf -d` on each stripped library); that a phone's system provides it is Android platform knowledge,
**not checked here**. With a loader and no driver, or a GPU that reports less than Vulkan 1.1, the engine throws from its constructor: `Failed to create Vulkan Instance! VkResult ...`
(`engine/src/renderer/VulkanContext.cpp:248`), `Failed to find GPUs with Vulkan support!` (`VulkanDevice.cpp:107`) or `Failed to find a suitable Vulkan physical GPU!
<why each GPU was refused>` (`VulkanDevice.cpp:142`). `MagicPortalsMain` catches it, writes it to the log, and, for a player's run, shows it in the activity's dialog
(`ShowFatalDialog` calls the engine's `Android::ShowMessage`, `engine/src/platform/android/AndroidApp.hpp:85`, on Android; `game/main.cpp`) with the engine's own words:
they are technical, and no friendlier text was written because none could be tried. A phone whose loader file were absent would fail to *load the library*
and the app would close with nothing shown (by the dynamic linker's rule; not tried); Google Play filters on the manifest's `uses-feature`, a sideload is not filtered.
The phone profile sets the lowest accepted Vulkan minor to 1 (`game/data/engine.json`, `mobile`), because many Android 11 and 12 phones report 1.1; **run:** the
log of every start says `minimum Vulkan 1.1` and the emulator's GPU, `SwiftShader Device (LLVM 10.0.0), Vulkan 1.2.0`; a 1.1-only device cannot be tried here.
If that file cannot be read, a phone's run stops with a message that says so instead of starting with the engine's default of 1.2.

## 6. What the owner must provide or decide

1. **The application id.** `com.ivancvetanovic.magicportals` is a placeholder; an id is permanent once the app is on a store.
2. **A release key.** None exists for this game. Make one outside the repository (`keytool -genkeypair ...`), keep it and its password, and pass
   `--release --keystore <file> --keystore-pass-file <file> --key-alias <alias>`. The script and the packager take no key from anywhere else and refuse the debug
   key for a release. A release APK is *not* debuggable, has no `.idsig`, and its certificate is checked against the keystore's. A release cannot be installed over a
   debug build (another signer): uninstall first. Losing the key means no update can ever be installed over the app.
3. **Each release's `--version-code`** (+1).
4. **The icon's look**, the store listing, the rights question. `docs/permission.md` governs what may be shipped; it was not changed.
5. **Whether the 16 KB-aligned libraries and `targetSdk` 35 suit the store you publish to**; the store's rules change yearly.
6. **Whether the post-mortem report should stay on** until the engine names the game in it: its title says "Penumbra closed unexpectedly last time" in this game too
   (a string of the engine's `PostMortem.java`, line 222). The manifest's `supersonic.reportUnexpectedExit` turns the report off, and with it the words of a failed start at the next start.

## 7. How to check an APK, and how to run it on an emulator

Without a device (all run):

```
tools/build_android.sh --abi all --package-only --data out/dist/content            (packages, then prints every check below)
python tools/package/android_package.py inspect out/android/MagicPortals-0.1.0-debug.apk --sdk <sdk>
python tools/package/android_package.py compare out/android/MagicPortals-0.1.0-debug.apk --package out/dist/content
<build-tools>/aapt2 dump badging <apk>;  <build-tools>/zipalign -c -P 16 -v 4 <apk>;  java -jar <build-tools>/lib/apksigner.jar verify --print-certs <apk>
```

`inspect` asks aapt2, zipalign, apksigner and the zip itself: package id, versionName equal to the CMake version, versionCode, minSdk 26, targetSdk 35, the Vulkan 1.1
`uses-feature`, no permission, landscape, both ABIs, `application-debuggable` only for a debug APK, 16 KB alignment, the signature, **no JAR signature, the activity's
`configChanges` equal to every kind the platform defines**, the APK's assets equal to the list, and every library as an ELF file. `compare` proves that the APK's list
**equals** the content package's `manifest.json` (same files, sizes and sha256) and the shaders. (Run against Step 203's APK, `inspect` now says it is missing 0x10008003 of
the configuration kinds and carries a JAR signature: its own negative control.)

On an emulator: **one script, one serial.** [`tools/android_emulator.py`](../tools/android_emulator.py) is the only way this project talks to adb or the emulator. It
knows two slots (`impl`: `MP_Android_API33_x86_64` on console port 5564, serial `emulator-5564`; `verify`: `MP_Android_API33_x86_64_v`, 5566), refuses any other serial, any
`-s` of its caller's and the adb verbs that act on the adb server or on every device (`kill-server`, `start-server`, `reconnect`, `devices`, ...), refuses to start below 6 GB of
free memory, never stops an emulator that is not its slot's, and logs every call before it runs it (`adb -s emulator-5564 ...`), so the log shows that nothing else was addressed. Its guard is
pinned by `tools/package/test_android_emulator.py` (no emulator needed). Commands:
`create` (avdmanager, never `--force`), `start`, `stop`, `status`, `install [--fresh]`, `launch`, `args`, `run-as`, `pull`, `tree-hash` (size and sha256 of every file under the app's folder, in one
pass), `screenshot`, `tap`, `swipe`, `key`, `logcat`. The machine also holds the owner's parity rig and the sibling port's emulators, which share one adb server: **never run a bare `adb`**.

**What the emulator does that a phone does not** (found in step 204; the emulator's, not the game's): a `force-stop` of an app that is presenting frames took the emulator down once
(`Failed to find ColorBuffer`, the host's GL emulation), so a script sends Home first; and in all the emulator died or stopped answering **11 times in about 3.5 hours**: that once, once during the
configuration probe, three times during the 128 routes (each within seconds of an app start) and six times in the retake, which cluster around uninstall, install and first launch (flows 1 to 3 needed several attempts).
Each time `stop` ended its own processes and `start` brought it back in 28 to 60 s; a route that was running then was replayed from the start. A verifier should expect about three restarts in the 128 routes and several attempts at flows 1 to 3. **The verifier's retake agrees:** 9 incidents in 10 launches of its own AVD (6 in flows 1 to 3 before it sent Home before every stop, 1 before the lifecycle flow, 2 in the shape flow, none in the failure flows, the level visit or the 128 routes: 6,349 s of route time without one); flows 1 to 3 took it 8 attempts, and the one that passed sent Home before every `force-stop` or uninstall of an app that is presenting and let the guest settle after a restart (an app started about 10 s after a boot is paused behind the system's load, and was killed once that way). The flow drivers (`flowlib.py`, `flow_*.py`, `supervise_routes.py`, `final_rerun.py`, `make_variants.py`) live only under the ignored `out/parity/program/PORT/PORT6/impl/` and are not committed: a retake needs them, and `tools/android_emulator.py` is the only committed script that touches an emulator (PORT6-4).

## 8. What step 204 found, and where each went

* **Fixed in step 204** (the riders of Step 203's verification, each pinned by a test and a named mutation): `Unpack::NeededBytes` wrapping on a hostile list (D4); five failure branches of `Unpack::Run` with no pin (D1);
  a source read to its end before its byte count was compared, an uncapped list and a free-space sentence that rounded the free figure up (D5, D6); the shim's rules that no host compiler built (D3); the
  manifest's 14 `configChanges` kinds (D10); the JAR signature (D12); the packager's and the parser's spelling of the list, now tied (D2); the failed unpack that left no line in the game's log file and
  cleared the old copy before it checked the space (PORT5-10).
* **For the engine's list** (the engine is read-only here): the report's title says "Penumbra" (`PostMortem.java:222`); the engine's `Android::ShowMessage` blocks (as before, PORT5-6); the first start shows Android's own
  "Viewing full screen" confirmation over the menu once (the engine hides the system bars; the system asks the person to confirm, `settings put secure immersive_mode_confirmations confirmed` silences it on a test device).
* **Recorded for a later step** (`docs/parity-backlog.md`, PORT6-1 and on): the developer's `--visit-levels` pool checks report 381 failures on Windows and on the emulator's final build (378 on the first build's run) (the same lines for level 1-03 on both, e.g. "the layer holds 9
  lightmap(s), the level names 12": the tool's own expectations do not match what the layer bakes; the cause was not traced; not an Android difference); the engine's pipeline cache lives in the owned `engine/` folder and is removed with it; the questions only a phone can answer (section 0).
* **Found by the verification of step 204 and recorded, not fixed** (`docs/parity-backlog.md`, PORT6-6 to PORT6-8): the emulator script's guard test never looks at what `adb_argv` builds; the script's output raises `UnicodeEncodeError` on a cp1252 console for a character such as U+2192; and three latent ties (the packager's list tie does not hold the 4 MiB cap or the rounded-sum rule, the failed-start log path is spelled in two places, the script's application id and activity name have no tie to the manifest).

## 9. What was not done, and is not known

* A phone: its Vulkan driver, its audio, its touch screen, its speed, heat and memory are untested until somebody runs one.
* The release path (a real key) was only run with a throwaway key made for a structure test (Step 203) and deleted afterwards.
* The Linux and macOS paths of `tools/build_android.sh` were not run.
* Android 5.x/6.x are not supported (minSdk 26), 32-bit ABIs are not built, and Google Play's policies were not checked.
* The phone profile's performance values (the scene's sample cap, dynamic resolution) are the owner's decision and are not set (`docs/packaging.md` section 6).

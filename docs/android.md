# Magic Portals Enhanced on Android: how it is built and packaged, and what has not been run

**Status, plainly: built, packaged and statically checked. NOT RUN on any phone or emulator.** Nothing in this page has been installed, started
or looked at on an Android device. The next step of the platform program (planning step 204) runs the APK on an emulator; a phone, its Vulkan
driver, its audio, its touch screen and its speed are untested until somebody runs one. Every statement below says whether it was *run* (a tool
printed it), *read* (taken from code or documentation) or *not known*.

The record of the step is [`planning/2026-09-11-magic-portals-remaster.md`](planning/2026-09-11-magic-portals-remaster.md), Step 203. The data
the APK carries comes from the content packager ([`packaging.md`](packaging.md), [`tools/package/README.md`](../tools/package/README.md)); the
Windows and Linux builds are in `packaging.md` and the README.

## 1. What you need

| | Used here (run) | Why |
|---|---|---|
| Android SDK | `%LOCALAPPDATA%\Android\Sdk`: platform 35, build-tools 35.0.0 (aapt2, zipalign, d8, apksigner), cmake 3.22.1 and its Ninja | the manifest's `targetSdk` is 35, so its `android.jar` is the compile target |
| Android NDK | r28.2 (28.2.13676358), clang 19 | r28 or newer: it links 16 KB-aligned by default, which newer phones demand |
| JDK | 17 (`JAVA_HOME`) | `javac` for the engine's Java activity, `java -jar` for d8 and apksigner |
| Python | 3.8 or newer, standard library only | `tools/package/android_package.py` |
| the game's data | `tools/package/package.py content ...` output | built from **your** copy of the original; none of it is in this repository |
| a signing key | the machine's standard `~/.android/debug.keystore` for a debug APK | a release key is **yours to make** (section 6) |

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
  gamepad not required, landscape locked, 14 configuration-change kinds handled in place (a recreated NativeActivity would start its native thread
  again over a running game; the 14 are orientation, screenSize, smallestScreenSize, screenLayout, keyboard, keyboardHidden, navigation, uiMode,
  density, fontScale, layoutDirection, locale, touchscreen and colorMode, **not** `mcc` and `mnc`: a SIM or network-code change would still recreate
  the activity, and neither would `fontWeightAdjustment` or `grammaticalGender`, PORT5-8), no permissions, `allowBackup` false, and the engine's
  post-mortem report meta-data. The packager checks every one of those on every build (`read_manifest`) and refuses a manifest that has a
  permission or spells a version.
* **The icon** is drawn at package time by `tools/art/make_app_icon.py` (the five launcher sizes and the 512 px store icon) into the work folder;
  nothing of the kind is committed. Its colours are its own choice.
* **The variants.** A *player's* APK has `MAGICPORTALS_DEV_TOOLS` OFF: none of the developer's flags (`--route`, `--tap`, ...) exist in it.
  `--verification` builds the other kind (DEV_TOOLS ON, trees `out/build/android-<abi>-verification`, APK `MagicPortals-<version>-VERIFICATION-debug.apk`)
  **for driving an emulator run with `files/magicportals_args.txt`; it is never signed with a release key** and must never be mistaken for the shipped file.

## 3. What is in the APK

| Entry | From |
|---|---|
| `AndroidManifest.xml`, `resources.arsc`, `res/mipmap-*/ic_launcher.png` | aapt2, from the manifest and the drawn icon |
| `classes.dex` | the engine's Java (`engine/src/platform/android/java`): javac for Java 8 bytecode, then d8 |
| `lib/<abi>/libMagicPortals.so` | the stripped libraries, each checked as an ELF file (machine, only these NEEDED libraries: `libandroid`, `liblog`, `libvulkan`, `libaaudio`, `libc`, `libm`, `libdl`; every LOAD segment aligned to 16 KB; no executable-and-writable segment; no text relocation) |
| `assets/gamedata.list` | the list the first-run unpack reads (below) |
| `assets/GameData/**` | exactly the files of the content package's `manifest.json`, re-hashed against it |
| `assets/engine/assets/shaders/*.spv` | the engine's 17 compiled shaders (the GLSL sources are not taken) |

On the owner's data (2026-10-23, this step's own run of the content packager and the Android packager): **883 content files, 24,589,032 bytes**
(23.45 MiB) plus **17 shaders, 118,716 bytes**: **900 files, 24,707,748 bytes** in the list; the data-bearing APK is **30,040,564 bytes** (28.65 MiB),
the data-less shell **8,448,527 bytes**, the stripped libraries 9,576,200 (arm64-v8a) and 10,486,408 (x86_64) bytes. These replace the two figures in the
research reports, which disagreed with each other and with this (about 4,400 files and 62 MiB in one, about 1,300 files and 40 MB in the other).

Nothing else can get in: the packager reads the *package folder* only through its manifest (a listed file that is missing, short, altered, a link, or
resolves outside the folder is refused, so is a file the manifest does not list, and so is anything on the never-package list: the original's scenes,
`.pvr`, `.pkm`, `.par`, `.enml`, `.exe`, `.dll`, `.as`, an APK, a build folder), and the shaders and Java only from the named engine folders. A package
folder under `reference/apk` or `reference/ethanon` is refused. The APK and everything it is made from are under `out/`; a script that writes into
the tracked tree is refused.

The assets are added by the packager itself, entry by entry, with a fixed date and fixed attributes, after aapt2 has made the manifest and resources
(not with `aapt2 -A`: whether aapt2 applies its default ignore patterns to dot-files and underscore folders was not found out, and the list must equal
the assets exactly; the packager checks that it does). Pictures and sounds are stored, the libraries and everything else deflated; the manifest says `extractNativeLibs="true"` (the sibling port's choice), so the installer extracts the
libraries of the phone's own ABI to disk (about 10 MB more storage) and `zipalign -P 16` has nothing to page-align inside the zip: the 16 KB property that matters is in the ELF segments, which
`llvm-readelf` and the packager's ELF reader both show (every LOAD aligned to 0x4000). Two runs on the same
inputs gave **byte-identical APKs** (the debug key's signature is deterministic RSA; the bytes of the compressed entries are those of the machine's
Python zlib, so two machines may differ and one machine does not).

## 4. The first run: the unpack

The engine cannot read an APK (`grep AAssetManager engine/src` finds nothing: every open is by path). So the app copies its data out of the APK into its
private files folder once. All the decisions are `game/sim/Unpack.{hpp,cpp}`, pure code with a suite (`test_mp_unpack`); `game/android/AndroidMain.cpp` is the
glue (`AAssetManager` as the source, `Unpack::FsSink` as the folder).

* **The list** (`assets/gamedata.list`): line 1 the *stamp*, 32 hex digits, the first 128 bits of the sha256 of every listed file's path, size and sha256 (it
  moves when any byte moves); then `size TAB path` for each file, paths relative to the files folder: `GameData/<package path>` and
  `engine/assets/shaders/<name>.spv`. Blank lines and CRLF are tolerated; there is no comment syntax.
* **The rule.** The stored stamp (`files/magicportals_gamedata.stamp`) equal to the list's *and* every listed file there at its size: nothing is done. Otherwise
  the stamp is removed **first**, then the two folders the unpack owns (`GameData`, `engine`), then free space is checked from the list's sizes in
  4 KB blocks (a message that says how much is needed and how much is free), then every file is copied and its byte count checked against the list, and the stamp
  is written **last**. A process killed at any point leaves no stamp, so the next start unpacks again from the top. The player's saves, the log, the flags
  file and the stamp are *beside* the owned folders and are never touched.
* **A hostile or damaged list** must not write outside the folders: every path must be relative, free of `..`, `.`, empty segments, backslashes, colons,
  control characters and NUL, not end in `/`, be unique, and begin with `GameData` or `engine`. A bad list is refused before anything on the disk is touched.
* **The event pump.** The engine's activity waits for the game thread to read its lifecycle commands, and a thread that does not for about five seconds is
  "not responding". The unpack calls `Android::PumpEvents()` after every 16th file. If the app is asked to finish meanwhile the unpack **stops without an
  error** (no further file, no stamp, no dialog).
* **No progress picture.** The engine's window does not exist before the game starts, and drawing on the native window first would clash with the Vulkan
  surface made later (read, not tried). A first run is a black screen for as long as the copy takes (**not timed on any device**; the 900 files are 23.6 MiB), and
  the progress is a log line.
* **A failure** (no space, an unreadable asset, a bad list) is a dialog that says what happened in words, and exit code 1 without a crash. A *scripted* run (a
  flags file present) never meets the dialog.
* **Where things go.** The game's data: `files/GameData/`, the shaders `files/engine/assets/shaders/`, the working directory `files/engine/` (the engine
  looks there for `assets/shaders`, and writes its pipeline cache there). The player's saves (`scores.json`, `audio.json`, `achievements.json`,
  `controls.json`): `files/Magic Portals/`, which is where the engine puts a user's data on Android (internal storage; it survives the app being stopped
  and is removed by uninstalling) *(read from `Supersonic::UserDataDirectory`, `engine/src/platform/ExecutablePath.cpp:221`; not observed after a force-stop)*. The log and the generated files:
  `files/Magic Portals/scratch/` (`Scratch::ArrangedFor` puts it first on a phone, because the NDK's C++ library names `/data/local/tmp` as the temporary folder
  and an app cannot write there), so the log is always `files/Magic Portals/scratch/magicportals.log`, the path the manifest's `supersonic.reportLog` names (a
  test ties the two).
* **Developer flags.** A phone has no command line. `files/magicportals_args.txt` (whitespace-separated: one flag per line, or several on a line; double quotes
  around a token with spaces) is read at start and appended after `--root files/GameData`; the file is renamed `magicportals_args.used` once read, so one scripted
  run is not every later launch. Only a debuggable APK lets `adb run-as` write the file; a release APK has no way to receive flags. No `--saves` is passed:
  it is a developer's flag and makes the run "not a player's", which suppresses the dialog that a phone with no usable Vulkan needs.

## 5. When Vulkan is missing (read, not run)

`libvulkan.so` is a NEEDED library of the program (run: `llvm-readelf -d` on each stripped library, in this step's implementation and again in its verification); that a phone's system provides it is Android platform knowledge, **not checked
here**. With a loader and no driver, or a GPU that reports less than Vulkan 1.1, the engine throws from its constructor: `Failed to create Vulkan Instance! VkResult ...`
(`engine/src/renderer/VulkanContext.cpp:248`), `Failed to find GPUs with Vulkan support!` (`VulkanDevice.cpp:107`) or `Failed to find a suitable Vulkan physical GPU!
<why each GPU was refused>` (`VulkanDevice.cpp:142`). `MagicPortalsMain` catches it, writes it to the log, and, for a player's run, shows it in the activity's dialog
(`ShowFatalDialog` calls the engine's `Android::ShowMessage`, `engine/src/platform/android/AndroidApp.hpp:85`, on Android; `game/main.cpp`) with the engine's own words:
they are technical, and no friendlier text was written because none could be tried. A phone whose loader file were absent would fail to *load the library*
and the app would close with nothing shown (by the dynamic linker's rule; not tried); Google Play filters on the manifest's `uses-feature`, a sideload is not filtered.
The phone profile sets the lowest accepted Vulkan minor to 1 (`game/data/engine.json`, `mobile`), because many Android 11 and 12 phones report 1.1.
If that file cannot be read, a phone's run now stops with a message that says so (planning step 203) instead of starting with the engine's default of 1.2.

## 6. What the owner must provide or decide

1. **The application id.** `com.ivancvetanovic.magicportals` is a placeholder; an id is permanent once the app is on a store.
2. **A release key.** None exists for this game. Make one outside the repository (`keytool -genkeypair ...`), keep it and its password, and pass
   `--release --keystore <file> --keystore-pass-file <file> --key-alias <alias>`. The script and the packager take no key from anywhere else and refuse the debug
   key for a release. A release APK is *not* debuggable, has no `.idsig`, and its certificate is checked against the keystore's. A release cannot be installed over a
   debug build (another signer): uninstall first. Losing the key means no update can ever be installed over the app.
3. **Each release's `--version-code`** (+1).
4. **The icon's look**, the store listing, the rights question. `docs/permission.md` governs what may be shipped; it was not changed.
5. **Whether the 16 KB-aligned libraries and `targetSdk` 35 suit the store you publish to**; the store's rules change yearly.

## 7. How to check an APK without a device (all run)

```
tools/build_android.sh --abi all --package-only --data out/dist/content            (packages, then prints every check below)
python tools/package/android_package.py inspect out/android/MagicPortals-0.1.0-debug.apk --sdk <sdk>
python tools/package/android_package.py compare out/android/MagicPortals-0.1.0-debug.apk --package out/dist/content
<build-tools>/aapt2 dump badging <apk>;  <build-tools>/zipalign -c -P 16 -v 4 <apk>;  java -jar <build-tools>/lib/apksigner.jar verify --print-certs <apk>
```

`inspect` asks aapt2, zipalign, apksigner and the zip itself: package id, versionName equal to the CMake version, versionCode, minSdk 26, targetSdk 35, the Vulkan 1.1
`uses-feature`, no permission, landscape, both ABIs, `application-debuggable` only for a debug APK, 16 KB alignment, the signature, the APK's assets equal to the list,
and every library as an ELF file. `compare` proves that the APK's list **equals** the content package's `manifest.json` (same files, sizes and sha256) and the shaders,
so the unpack on a device can be checked file by file.

## 8. What was not done, and is not known

* Nothing was installed or started. Whether the APK installs, whether the unpack completes (and how long it takes), whether the first frame draws, the frame rate,
  memory, the touch controls, the audio (the 46 sounds decode: Step 202; nobody has heard one off Windows), rotation and Home/resume cycles are all **not known**.
* The release path (a real key) was only run with a throwaway key made for a structure test and deleted afterwards (the Step says what was checked).
* The Linux and macOS paths of `tools/build_android.sh` were not run.
* Android 5.x/6.x are not supported (minSdk 26), 32-bit ABIs are not built, and Google Play's policies were not checked.
* The phone profile's performance values (the scene's sample cap, dynamic resolution) are the owner's decision and are not set (`docs/packaging.md` section 6).

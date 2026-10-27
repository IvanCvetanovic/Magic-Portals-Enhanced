# The workflows: what each does, what it costs, and what has and has not run

Planning step 209 (PORT-11), the eleventh step of the platform program. **Status, plainly: three workflows were written in this step (`ci.yml`, `release.yml`, `android-emulator.yml`) and `apple.yml`
was edited; NONE OF THEM HAS BEEN DISPATCHED, and no sentence in this repository says that a step of any of them works on GitHub.** `apple.yml` ran twice, green, on 2026-10-25 in the form it had then
(`docs/apple.md` section 4b); the edited file has not run. What was done instead is listed in section 6, with the tool that did it: a checker for the rules every workflow keeps, `actionlint`
(with `shellcheck`) over the four files, the commands of two Linux jobs rehearsed in the Linux subsystem from a clean clone, and tests that edit copies of the files and require each rule to refuse them.

Every claim below is one of three kinds, and says which: **RUN** (a tool printed it; the tool is named), **READ** (taken from a source) or **NOT KNOWN** (nobody has run it).

## 1. The four files

| File | Inputs | Jobs (runner, timeout) | What it proves | What it cannot |
|---|---|---|---|---|
| `ci.yml` | `job`: `linux`, `windows`, `android`, `tools`, `all` (default) | `linux` (Ubuntu 24.04, 90 min); `linux-release` (Ubuntu 22.04, 90); `windows` (Windows Server 2022, 120); `android` (Ubuntu 24.04, 120); `tools` (Ubuntu 24.04, 45) | that the project builds off the owner's PC; that every suite passes or SKIPS (exit 77) with none of his data; that the fixture level starts and draws on the software Vulkan driver; that the release programs are what `release.py check-program` accepts; that the shell APK packages and the release check refuses a debug APK; that the Python tools and the workflows keep their rules | that the game plays; that a real GPU, phone or person is happy; the Windows fixture level (NOT RUN, section 4) |
| `release.yml` | `version` (a string; checked first) | `version` (10); `linux`, `windows`, `android` (90, 90, 120); `apple-macos`, `apple-ios` (calls of `apple.yml`); `collect` (30) | that every platform's release PROGRAM is built the way the owner's release is, passes `check-program`, and is uploaded with a `SHA256SUMS.txt` and a `VERSIONS.txt` | anything about a program with the game in it: no job holds game data |
| `android-emulator.yml` | `api_level` (default 33), `abi` (`x86_64`) | `smoke` (Ubuntu 24.04, 150 min) | **NOT KNOWN: never dispatched.** It is written to make the engine's crash-report dialog show itself on an emulator and read it from the UI hierarchy | everything, until it is dispatched |
| `apple.yml` | `job`: `macos`, `ios-device`, `ios-sim`, `all` (dispatch) or the same as a string (called by `release.yml`) | `macos` (macOS 15, 90), `ios-device` (60), `ios-sim` (60) | `docs/apple.md` section 4 and 4b | `docs/apple.md` section 12 |

**What every file keeps** (the checker, `tools/ci/check_workflows.py`, rules W01 to W16, each with a test that edits a copy of a real file and requires the rule to refuse it): it runs on a dispatch and on
nothing else (`apple.yml` may also be called by `release.yml`); `permissions: contents: read` and nothing wider; no secret, no token expression, no path of the owner's data, no account name; every action is a
commit sha of 40 hex digits; every job has a timeout; every job ends with an upload of its logs that runs whatever happened; every launch of the game's program has a wall limit and every `ctest` a `--timeout`;
a job waits for another only where the table in the checker says so; no download is piped into a shell; every runner is a named image; no `${{ }}` inside a script; the default shell is bash (so a pipe to `tee`
fails when the command before it fails); no `| head` in a script unless the line ends in `|| true` (W16: under that pipefail, `head` closes the pipe once it has its lines and a writer that writes again
dies of SIGPIPE, exit 141; `ldd --version | head -1` failed 33 times in 40 runs on Ubuntu 24.04 when a verifier measured it, and the workflows use `sed -n 1p`, which reads all its input).

**The actions, and how each sha was resolved.** All four are GitHub's own (`actions/*`), and every sha came from a read-only `git ls-remote` against the action's public repository, on 2026-10-27; each is a
lightweight tag (no peeled `^{}` line), and each is the commit the floating `v4` tag named on that day. (`apple.yml`'s two runs of 2026-10-25 used the floating tag; what it named on the 25th is not known.)

| Action | Tag | Commit |
|---|---|---|
| `actions/checkout` | v4.4.0 | `11d5960a326750d5838078e36cf38b85af677262` |
| `actions/upload-artifact` | v4.6.2 | `ea165f8d65b6e75b540449e92b4886f43607fa02` |
| `actions/download-artifact` | v4.3.0 | `d3f86a106a0bac45b974a628896c90dbdf5c8093` |
| `actions/cache` (also `/restore`, `/save`) | v4.3.0 | `0057852bfaa89a56745cba8c7296529d2fc39830` |

(`tools/apple/test_workflow.py` holds that each action has one sha across all four files.) Nothing else is downloaded by a workflow except: MoltenVK's tarball in `apple.yml` (sha256 checked, section 4b of
`apple.md`), `actionlint`'s Linux tarball in the `tools` job (sha256 checked before it is unpacked), and the Android SDK parts, which `sdkmanager` takes from Google and checks against its own listing.

## 2. How the owner starts one, and what it costs

Actions, the workflow's name, *Run workflow*, the branch, the inputs. **Dispatching any of them is the owner's decision and nothing here dispatched one.** A dispatch needs nothing but the repository: no
secret, no key, no game data. The results are the run's artifacts (7 days; the release programs 30) and its logs.

**Cost.** The repository is private, so a dispatch spends the account's Actions minutes. The usual published multipliers are 1 for a Linux minute, 2 for a Windows minute and 10 for a macOS minute; **the account's
own rule (its included minutes, whether it applies those multipliers) was not read here.** Nothing in `ci.yml`, `release.yml` or `android-emulator.yml` has been timed. The only measured run is
`apple.yml`'s (`docs/apple.md` section 4b): 6 min 42 s for `macos` on a cold cache and 3 min 51 s on a warm one, about 30 s for each iOS job warm. `release.yml` calls `apple.yml` for `macos` and
`ios-device` only (not `ios-sim`), and the macOS job also runs the suites, so a release run costs what those two cost plus the Linux, Windows and Android builds, whose times are NOT KNOWN: the first dispatch
measures them. The timeouts above are ceilings, not predictions.

**When the repository is public.** The triggers to add are the owner's change: under `on:` of `ci.yml`, `push:` with `branches: [main]` and `pull_request:`. The rules do not allow either today (W01), so the
change is a decision in two places: the file and the trigger rule W01 of `tools/ci/check_workflows.py` (`check_file`, where `REUSABLE` names the one file that may also be called). Linux, Windows and macOS minutes on a public repository are not billed (the usual rule; not read for this account).

## 3. `ci.yml` job by job

* **`Linux - build, test, fixture`** (Ubuntu 24.04). The packages `tools/build_linux.sh` names in its header (a test ties the two lists) plus `xvfb` and `mesa-vulkan-drivers`; `tools/build_linux.sh` builds
  `MagicPortals` and `test_mp` (baked paths, which point at this checkout where the owner's data is not); `ctest` runs every suite with a `--timeout 900`; the step prints the number skipped and **fails on any
  failed test**; the fixture level is started under `xvfb-run` on the software Vulkan driver for 300 fixed ticks at 1280x720 and `tools/apple/check_png.py` requires a picture of at least 640x360 that is not one
  flat colour; and the tracked tree must be unchanged at the end (the engine's shaders above all).
* **`Linux - release program`** (Ubuntu 22.04). The release recipe: `tools/build_linux.sh --no-baked-paths --dev-tools OFF --portable --strip --cxx g++-12`. `release.py check-program --max-glibc 2.35`; an
  archive without game data (`assemble --no-data`, named `...-nodata`) made twice from two folders, `cmp` requiring the same bytes; unpacked as a player would; the fixture level started from it with
  `--root` (the archive has no game); and a start with no game data and no `--root`, which must exit 1 and say `the game's data was not found`. **Which image and why:** 22.04 because it is the oldest Ubuntu image
  this step knew of that has a g++ the engine's README accepts (GCC 11 or newer; whether GitHub still offers it on the day of a dispatch is NOT KNOWN); its glibc is 2.35, which becomes the floor of the program (section 5). **NOT KNOWN:** that the project builds with GCC 12 (it was built
  with 13 and with clang 18 only), that CMake and the other tools of that image are what the build wants, and that GitHub still offers the image on the day of the dispatch.
* **`Windows - build, test, release program`** (Windows Server 2022). MSVC's environment from the newest Visual Studio on the image (`vswhere`, then `vcvars64.bat`); a configure like `tools\parity\port_build.bat`
  (Ninja, Release, `cl`) with validation off and no shader compiler; the two targets `MagicPortals` and `test_mp` (not the default target, which also builds `MagicPortalsSpike`, the stale spike CLAUDE.md says is never built); every suite under `ctest`; then the release program as `docs/packaging.md` section 6 builds it (no baked paths, no developer tools,
  `MAGICPORTALS_WIN_GUI=ON`, the icon `tools/art/make_app_icon.py` draws); `check-program`; an archive without game data. **The fixture level is NOT RUN on Windows**: see section 4.
* **`Android - shell APK, inspect, release refusal`** (Ubuntu 24.04). The SDK parts of `docs/android.md` section 1 (NDK 28.2.13676358, build-tools 35.0.0, platform 35) installed with `sdkmanager` if the image
  lacks them; `tools/build_android.sh --no-data --abi all`; `android_package.py inspect`; and `release.py android` on the debug APK, which must REFUSE it (exit 2, the sentence of the refusal, nothing placed).
* **`Tools - Python tests, workflow checks`** (Ubuntu 24.04). The packages `tools/build_linux.sh` names in its header (the tests of `tools/ci` run that script's configure four times, which is a real CMake configure of the project and of the engine's GLFW, and GLFW stops without the X11 development headers; a test ties this job's list to the header's, and the configure test also skips, loudly, where `pkg-config` does not find them); a virtual environment with pytest, ruff and PyYAML (never on the owner's PC); the unittest suites of `tools/package`, `tools/ci` and `tools/apple`; the
  converter's pytest and ruff; `tools/package/check_version.py`; the workflow checker; and `actionlint`.

## 4. What the Windows job does not do, and why

The runner image has no Vulkan driver that this step checked, and the Windows fixture run needs one. A software driver for Windows would have to be downloaded, and this step was allowed no download but one (the
linter's, section 6), so none was fetched, checked or pinned. That is a fact about this step and not about software drivers: nobody here looked for one that can be pinned. So `ci.yml` builds the Windows program, runs the suites, builds the
release program and reads it as bytes, and **does not start the game on Windows**: its step `Run the fixture (NOT RUN on Windows)` says so in the log. What would settle it is a pinned software driver
(by commit, with its sha256) or a runner with a GPU; both are the owner's to choose.

## 5. The Linux program that runs on more than the machine that built it (PORT10-3)

**RUN** in the Linux subsystem (Ubuntu 24.04, g++ 13.3; `out/parity/program/PORT/PORT11/impl/needs_*.txt`), the same sources, release program of step 208 against the release recipe with the two new options:

| | step 208's program | `--no-baked-paths --dev-tools OFF --portable --strip` |
|---|---|---|
| version names asked of the system | `GLIBC_2.38`, `GLIBCXX_3.4.32`, `CXXABI_1.3.9`, `GCC_4.3.0` | `GLIBC_2.38` alone |
| shared libraries (`readelf -d`) | `libvulkan`, `libasound`, `libstdc++`, `libm`, `libgcc_s`, `libc` | `libvulkan`, `libasound`, `libm`, `libc` |
| symbol table | present | none (`check-program`: "stripped") |
| size | 12,019,040 bytes (10,627,824 once stripped) | 12,135,904 bytes (the C++ runtime inside is about 1.5 MB) |
| archive without game data | | 5,488,118 bytes, the same sha256 (`c73647a1...`) from two folders at two times |
| `check-program --max-glibc 2.38` / `2.35` | | passes / REFUSED in words ("the program asks for glibc 2.38 and the floor is 2.35") |
| fixture level from the unpacked archive | | exit 0, a 1280x720 picture of 4 distinct pixel values, under a display of its own |
| a start with no data and no `--root` | | exit 1, "the game's data was not found (looked in ...)" |

**`GLIBC_2.38` is carried by seven symbols** (`objdump -T`): `fmod` and `fmodf` (glibc 2.38 versioned them anew) and five `__isoc23_*` functions (`strtol`, `strtoll`, `strtoul`, `strtoull`, `sscanf`: what a glibc 2.38
header redirects to under C23-aware compilers). They belong to the build machine, not to the code: glibc 2.35 has neither version (READ from the version maps; NOT RUN on a 22.04 machine, which this PC does not
have, so what a 22.04 build names is a prediction). `--portable` removes the `GLIBCXX`, `CXXABI` and `GCC` needs and two of the libraries; it cannot remove the glibc one (the game opens the Vulkan loader, ALSA
and X11 at run time, which need the system's C library), so **the floor is whatever system built the program**, and the release is built on the oldest one it should run on: the CI jobs build on Ubuntu 22.04 and
pass `--max-glibc 2.35`. Whether to strip a release is the owner's. **The build does not depend on its folder (RUN):** the program built again in a clean local clone, in another checkout folder and another build folder (the rehearsal of the `linux-release` job, section 6), has the sha256 `dc128308aca2da4ecf41b178a4c24d926c88ee0e7ae3a4275249a0a54d4b19e6`, the same as the one above, and so the same archive.

## 6. What was run in this step, and what was not

All on the owner's PC, 2026-10-27, in the Linux subsystem (Ubuntu 24.04, g++ 13.3, CMake 3.28, Ninja 1.11, Python 3.12) unless the row says Windows. The Step in
`docs/planning/2026-09-11-magic-portals-remaster.md` has the gates, the deviations and the mutation table; the logs are under `out/`, which is not in the repository.

| What | Tool | Result |
|---|---|---|
| the rules every workflow keeps, over the four files | `python tools/ci/check_workflows.py` (needs PyYAML; exit 3, never a pass, without it) | every rule kept by `ci.yml`, `release.yml`, `android-emulator.yml` and `apple.yml`; 82 tests of the checker (rules W01 to W16), and each rule goes red on an edited copy of a real file |
| the workflows as GitHub reads them | `actionlint` 1.7.12 (release tarball, sha256 `8aca8db9...a3d8` compared with the project's published checksums before it was unpacked) with `shellcheck` 0.11.0 on `PATH` | zero findings, after two `shellcheck` findings (SC2129 in `android-emulator.yml`, SC2094 in `release.yml`) were fixed; run again after fix round 1 (below) with the binary of the `actionlint-py` wheel of the Linux subsystem's virtual environment (also 1.7.12, its checksum not compared: the checked tarball had been deleted and no second download was allowed): zero findings; the independent verifier's round 2 then downloaded the release once more in its own run, compared its sha256 with the published file before unpacking it, and ran it with `shellcheck` 0.11.0 over the four files: zero findings (an unquoted `$RUNNER_TEMP/x*` in a copy gave SC2086 as a positive control) |
| the `Linux - build, test, fixture` job's commands, from a clean clone, BEFORE fix round 1 | `tools/ci/job_script.py` takes the job's scripts out of the YAML; `out/.../rehearse.sh` runs them in order from the clone, with a runner's environment variables | every script step rc 0: the engine at its pinned commit; the build (311 steps; 21 compiler warnings, all in the engine's third-party headers (entt 20, stb 1), none in `game/` or `tests/`); `ctest` 80 tests, **100 % passed, 0 failed, 36 skipped**; the fixture level exit 0 and a 1280x720 picture of 4 distinct pixel values (37,014 bytes); the tracked tree unchanged |
| the `Linux - release program` job's commands, BEFORE fix round 1 | the same | every script step rc 0: the release program built (232 steps; the first 141 were stopped by the memory guard, see below, the second part had no warning, the first part the engine's 21) with the sha256 `dc128308...b19e6`, **the same bytes as the program built by hand in another folder**; it asks the system for `GLIBC_2.38` alone and four shared libraries; `check-program --max-glibc 2.38` ok (stripped, no libstdc++ version asked); the archive without game data made twice from two folders, one sha256 `c73647a1...e77` (also the hand-made one's); the fixture level from the unpacked archive exit 0 and a 1280x720 picture of 4 distinct pixel values (37,014 bytes); a start with no game data and no `--root` exit 1, "the game's data was not found"; the tracked tree unchanged. Local substitutions: the floor 2.38 for 2.35, `g++` for `g++-12` |
| the same two jobs, retaken AFTER fix round 1 by the independent verifier (round 2), from a clean clone of its own | `tools/ci/job_script.py` takes the scripts out of the YAML (12 and 17 steps, equal to the YAML's) and runs them in order, with NO workaround of the `ldd` line | `linux`: all 8 script steps rc 0 (the `Runner` step printed `ldd (Ubuntu GLIBC 2.39) 2.39` without dying; `ctest` 80 tests, 0 failed, 36 skipped; the fixture level exit 0, "finished: clean", a 1280x720 picture of 4 distinct pixel values, 37,014 bytes; the tracked tree untouched). `linux-release`: all 12 script steps rc 0 (the release program has the sha256 `dc128308...b19e6` again; `check-program` ok with the floor 2.38; the archive twice one sha256; the fixture level from the unpacked archive exit 0 with the same picture; a start with no game data exit 1, "the game's data was not found"; the tracked tree untouched). Local substitutions as above (`-j1`, `g++`, the floor 2.38, no `ccache`, `dpkg -s` for `apt-get`, the actions skipped and listed). The first build attempt of `linux` was stopped by the verifier's own memory watcher at 1.44 GiB free (step 145 of 311) and resumed at `-j1` in a capped control group |
| the Python tests | `python -m unittest discover` | `tools/package` 344 (Windows, 4 skipped; Linux subsystem, 2 skipped); `tools/ci` 194 (both; 1 skipped on Windows, none in the Linux subsystem); `tools/apple` 139 (both; 1 skipped on Windows; in the Linux subsystem none where `actionlint` is on `PATH` and 1 where it is not) |
| a Windows run of the checker's tests | Windows Python 3.14 (which has PyYAML on this PC) | the same 194 |
| a mutation of each rule the step added | 77 edits of copies (the checker's rules and the ties around them 26, the packager's refusals 19, the smoke script's rules 23, `apple.yml`'s pins and call trigger 3, `build_linux.sh`'s options 2, the fixture builder 2, the job-script extractor 2), each required to turn a named test red | 77 of 77 red, every file restored byte for byte |

**What differed from a runner, said in each log:** one compiler job (`-j1`, not `-j4`, because of this PC's memory); no `ccache` (not installed here; the cache steps and the launcher are a runner's);
`apt-get` replaced by `dpkg -s` of the same package list (all present); the actions (`checkout`, `cache`, `upload-artifact`) skipped and listed; the clone local (`git clone` of this checkout's `HEAD`,
with the step's own uncommitted files committed inside the clone, the engine cloned from the local module repository and verified to be the pinned commit). The build of the first job ran in two
parts: the line for this PC's free physical memory is 1,500 MiB, and at step 308 of 311 (one test file, a compiler process of about 2 GB) the readings fell from 1,134 MiB to 862 MiB before the build was
killed by hand, so the line was crossed and the stop came after it; the same script was run again over the same build folder (ninja carries on), and the first part's log is kept beside the second.
The second job's build was stopped once, by a guard script, at step 141 of 232 on a reading of 1,153 MiB (the guard's own line was 1,400 MiB, below the 1,500 of the brief: a reading of 1,431 MiB had passed it); the guard was set to 1,500 MiB, the page cache of the Ubuntu VM was dropped (as the VM's root, `echo 3 > /proc/sys/vm/drop_caches`: no file changes) and the rest ran in a second part whose lowest reading was 1,786 MiB. **Fix round 1 (after a review and a first independent verification of this step).** Eleven defects were found and are fixed here or declined with a reason; the Step's deviation 12 lists them. The ones that touch this document: (D9) `ldd --version | head -1` in `ci.yml`'s `linux-release` job and `release.yml`'s `linux` job died of SIGPIPE under `bash -eo pipefail` (a verifier measured 33 failures in 40 runs on Ubuntu 24.04; the control here gave 29 of 40, and the same line with `sed -n 1p` 0 of 40), so every `| head` in the three new workflows is `sed -n` now and the checker has a rule against the form (W16); (D2) the `tools` job ran tests that configure the whole project and installed none of its packages: it installs them now; (D10) the Windows job built the default target, which includes the stale spike: it builds `MagicPortals` and `test_mp`; (D1) the smoke script read the game's log, which is the run before's until the glue's dialog is closed, as proof of a new start (`docs/android.md` section 11.7). **What was re-run after the fix, and what was not.** The two rehearsals above are of the jobs as they were before it; the verifier's round 2 retook both after it (the row above). The changed `Runner` steps of the two rehearsed jobs and the `Versions` step of `release.yml`'s Linux job were taken out of the YAML again with `tools/ci/job_script.py` (`g++` for `g++-12` as before) and run 40 times each under `bash -eo pipefail`: 0 of 120 runs failed. Not re-run by the implementer (the verifier's round 2 did, above): the two full builds (232 and 311 steps; no C++ and no build script changed in the round, and the rehearsed jobs' other steps are the text they were), the `tools` job's new `Packages` step (a runner's `apt-get`), the Windows job's `--target`, and the three `sed -n` lines of `android-emulator.yml` and the Android lines of `release.yml` and `ci.yml` (`adb`, `emulator` and `java -version` are not on this PC's Linux subsystem); the checker, `actionlint` with `shellcheck` and the Python suites were run on the final files.

**NOT RUN:** a dispatch of any of the four files; a rehearsal of the `tools` job's new `Packages` step; the `windows` job in any part (no Windows program was built, linked or started in this step); the `android` job (the SDK and the NDK were not used);
the `tools` job (its virtual environment needs `pip` from the network; the same test commands were run directly, above); `release.yml` and `android-emulator.yml` in any part; `apple.yml` since its edit;
an Ubuntu 22.04 build (this PC has none, so the glibc floor 2.35 was replaced by 2.38 in the rehearsal and g++ 12 by g++ 13.3); an emulator or a phone: none was started and no adb command was issued (the
guarded script's 6 GiB floor, `tools/android_emulator.py`, is untouched; this PC had about 2.2 GiB available when the smoke script was reached). The smoke flows have only ever read invented text.

## 7. The owner's path from these workflows to a release

1. Dispatch `release.yml` with the version (it must be the project's; the first job refuses another).
2. Download the artifact `release-programs`: `linux/MagicPortals`, `windows/MagicPortals.exe`, `macos/MagicPortals-macOS-shell.zip`, `ios/MagicPortals-iOS-shell.ipa`, `android/*.apk` (a debug-signed
   shell: a test file, never the release), `SHA256SUMS.txt` and `VERSIONS.txt`. `sha256sum -c SHA256SUMS.txt` before using anything.
3. On his PC, with his content package (`package.py content`, from his own data): `release.py assemble --platform windows|linux --program <the program> --content <content> --out <release folder>` for
   Windows and Linux; `release.py assemble --platform macos|ios --shell <the shell> --content <content> --out <release folder>` for the Apple platforms (the seal text says what is then invalid); and the release
   APK built on his PC with his own key (`tools/build_android.sh --release ...`, then `release.py android`). `docs/packaging.md` section 8 is the whole recipe.
4. `release.py sums <release folder> --check`, then the upload to a GitHub Release, which is his and which no workflow here does.

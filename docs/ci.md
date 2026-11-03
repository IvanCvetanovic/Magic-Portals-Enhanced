# The workflows: what each does, what it costs, and what has and has not run

Planning step 209 (PORT-11), the eleventh step of the platform program; section 9 was added at planning step 211 (2026-11-03). **Status, plainly: step 209 wrote three workflows (`ci.yml`, `release.yml`,
`android-emulator.yml`) and edited `apple.yml`. On 2026-11-03 `release.yml` was dispatched twice on GitHub (section 9): the first run failed in its `windows` job, and the second ran all six jobs to success.
`ci.yml` and `android-emulator.yml` HAVE NEVER BEEN DISPATCHED, and no sentence in this repository says that a step of either of them works on GitHub.** `apple.yml` ran twice, green, on 2026-10-25 in the form it
had then (`docs/apple.md` section 4b), and its two jobs that `release.yml` calls succeeded in both of that workflow's runs. What step 209 did instead of a dispatch is listed in section 6, with the tool that did it:
a checker for the rules every workflow keeps, `actionlint` (with `shellcheck`) over the four files, the commands of two Linux jobs rehearsed in the Linux subsystem from a clean clone, and tests that edit copies
of the files and require each rule to refuse them. Sections 1 to 8 were written before the first dispatch; where a later run changed a sentence it is changed, and section 6 stays the record of 2026-10-27.

Every claim below is one of three kinds, and says which: **RUN** (a tool printed it; the tool is named), **READ** (taken from a source) or **NOT KNOWN** (nobody has run it).

## 1. The four files

| File | Inputs | Jobs (runner, timeout) | What it proves | What it cannot |
|---|---|---|---|---|
| `ci.yml` | `job`: `linux`, `windows`, `android`, `tools`, `all` (default) | `linux` (Ubuntu 24.04, 90 min); `linux-release` (Ubuntu 22.04, 90); `windows` (`windows-2025-vs2026`, 120); `android` (Ubuntu 24.04, 120); `tools` (Ubuntu 24.04, 45) | **NOT KNOWN: never dispatched.** It is written to prove that the project builds off the owner's PC; that every suite passes or SKIPS (exit 77) with none of his data; that the fixture level starts and draws on the software Vulkan driver; that the release programs are what `release.py check-program` accepts; that the shell APK packages and the release check refuses a debug APK; that the Python tools and the workflows keep their rules, and that the tracked files are what a stranger may read (section 8) | that the game plays; that a real GPU, phone or person is happy; the Windows fixture level (NOT RUN, section 4) |
| `release.yml` | `version` (a string; checked first) | `version` (10); `linux` (Ubuntu 22.04), `windows` (`windows-2025-vs2026`), `android` (Ubuntu 24.04) (90, 90, 120); `apple-macos`, `apple-ios` (calls of `apple.yml`); `collect` (30) | **RUN twice on 2026-11-03 (section 9); the second run succeeded in all six jobs.** That every platform's release PROGRAM is built the way the owner's release is, passes `check-program`, and is uploaded with a `SHA256SUMS.txt` and a `VERSIONS.txt` | anything about a program with the game in it: no job holds game data; and that any program it builds starts (section 9) |
| `android-emulator.yml` | `api_level` (default 33), `abi` (`x86_64`) | `smoke` (Ubuntu 24.04, 150 min) | **NOT KNOWN: never dispatched.** It is written to make the engine's crash-report dialog show itself on an emulator and read it from the UI hierarchy | everything, until it is dispatched |
| `apple.yml` | `job`: `macos`, `ios-device`, `ios-sim`, `all` (dispatch) or the same as a string (called by `release.yml`) | `macos` (macOS 15, 90), `ios-device` (60), `ios-sim` (60) | `docs/apple.md` section 4 and 4b; and, as the two calls of `release.yml`, section 9 | `docs/apple.md` section 12 |

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

Actions, the workflow's name, *Run workflow*, the branch, the inputs. **Dispatching any of them is the owner's decision.** `release.yml` was dispatched twice on 2026-11-03 (section 9); `ci.yml` and `android-emulator.yml` have not been dispatched. A dispatch needs nothing but the repository: no
secret, no key, no game data. The results are the run's artifacts (7 days; the release programs 30) and its logs.

**Cost.** The repository is private, so a dispatch spends the account's Actions minutes. The usual published multipliers are 1 for a Linux minute, 2 for a Windows minute and 10 for a macOS minute; **the account's
own rule (its included minutes, whether it applies those multipliers) was not read here.** Nothing in `ci.yml` or `android-emulator.yml` has been timed, and the wall-clock minutes of the jobs of
`release.yml` were not collected from the API. The runs that were timed: `apple.yml`'s (`docs/apple.md` section 4b), 6 min 42 s for `macos` on a cold cache and 3 min 51 s on a warm one, about 30 s for each iOS job warm; and
the first run of `release.yml`, which took 15 min 8 s in total and ended with its `windows` job failed, so that figure is not the time of a successful release run (section 9). `release.yml` calls `apple.yml` for `macos` and
`ios-device` only (not `ios-sim`), and the macOS job also runs the suites, so a release run costs what those two cost plus the Linux, Windows and Android builds, whose times are NOT KNOWN: neither the minutes of
each job nor the second run's total is recorded here. The timeouts above are ceilings, not predictions.

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
  this step knew of that has a g++ the engine's README accepts (GCC 11 or newer); its glibc is 2.35, which becomes the floor of the program (section 5). **RUN on GitHub, by `release.yml`'s `linux` job (the same recipe and the same image; 2026-11-03, section 9):** the image was offered, the project builds with g++ 12.3
  on it, CMake and the other tools of the image were what the build wanted, and `check-program --max-glibc 2.35` passed. **NOT KNOWN:** this job of `ci.yml` itself (it has not been dispatched; what it adds is the archive without game data and the fixture level started from it), and whether GitHub still
  offers the image on a later day.
* **`Windows - build, test, release program`** (`windows-2025-vs2026`: Windows Server 2025 with Visual Studio 2026, MSVC 19.51; the image first written here was `windows-2022`, which failed the first dispatch of `release.yml`: section 9). MSVC's environment from the newest Visual Studio on the image (`vswhere`, then `vcvars64.bat`); a configure like `tools\parity\port_build.bat`
  (Ninja, Release, `cl`) with validation off and no shader compiler; the two targets `MagicPortals` and `test_mp` (not the default target, which also builds `MagicPortalsSpike`, the stale spike CLAUDE.md says is never built); every suite under `ctest`; then the release program as `docs/packaging.md` section 6 builds it (no baked paths, no developer tools,
  `MAGICPORTALS_WIN_GUI=ON`, the icon `tools/art/make_app_icon.py` draws); `check-program`; an archive without game data. **The fixture level is NOT RUN on Windows**: see section 4. **What ran on GitHub:** only the release-program part of this list,
  in `release.yml`'s `windows` job (the same options; it builds the one target `MagicPortals` and runs `check-program`, no suite): it passed in the second dispatch. This job of `ci.yml` (the developer's build, `test_mp`, every suite under `ctest`) has not run.
* **`Android - shell APK, inspect, release refusal`** (Ubuntu 24.04). The SDK parts of `docs/android.md` section 1 (NDK 28.2.13676358, build-tools 35.0.0, platform 35) installed with `sdkmanager` if the image
  lacks them; `tools/build_android.sh --no-data --abi all`; `android_package.py inspect`; and `release.py android` on the debug APK, which must REFUSE it (exit 2, the sentence of the refusal, nothing placed). **What ran on GitHub:** `release.yml`'s `android` job, which does the build and `inspect` and then reads each of the three libraries with `check-program` (not the refusal step): in the second dispatch it built the shell APK without game data, debug-signed, with all three ABIs (`arm64-v8a`, `armeabi-v7a`, `x86_64`), and its libraries passed (section 9). This job of `ci.yml`, with the refusal step, has not run.
* **`Tools - Python tests, workflow checks`** (Ubuntu 24.04). The packages `tools/build_linux.sh` names in its header (the tests of `tools/ci` run that script's configure four times, which is a real CMake configure of the project and of the engine's GLFW, and GLFW stops without the X11 development headers; a test ties this job's list to the header's, and the configure test also skips, loudly, where `pkg-config` does not find them); a virtual environment with pytest, ruff and PyYAML (never on the owner's PC); the unittest suites of `tools/package`, `tools/ci` and `tools/apple`; the
  converter's pytest and ruff; `tools/package/check_version.py`; the public-readiness check (section 8); the workflow checker; and `actionlint`.

## 4. What the Windows job does not do, and why

The runner image has no Vulkan driver that this step checked, and the Windows fixture run needs one. A software driver for Windows would have to be downloaded, and this step was allowed no download but one (the
linter's, section 6), so none was fetched, checked or pinned. That is a fact about this step and not about software drivers: nobody here looked for one that can be pinned. So `ci.yml` builds the Windows program, runs the suites, builds the
release program and reads it as bytes, and **does not start the game on Windows**: its step `Run the fixture (NOT RUN on Windows)` says so in the log. What would settle it is a pinned software driver
(by commit, with its sha256) or a runner with a GPU; both are the owner's to choose. **What the first dispatch added (RUN, section 9):** the image first used, `windows-2022`, has no Vulkan SDK, and the CMake configure of the Windows program stopped with "Vulkan loader not found at C:\Windows/System32/vulkan-1.dll"
(`engine/CMakeLists.txt:150` requires that file, the Vulkan loader, when no SDK is installed); the image now used, `windows-2025-vs2026`, has `vulkan-1.dll`, and the program configures and builds there. That is a loader and not a driver: the fixture level has still not been started on a Windows runner, and
the Windows program that `release.yml` built was read as bytes and has not been started on any PC.

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
header redirects to under C23-aware compilers). They belong to the build machine, not to the code: glibc 2.35 has neither version (READ from the version maps; NOT RUN on a 22.04 machine by this PC, which has none, so the
table above is a rehearsal and what a 22.04 build names was a prediction when it was written; it was then built on GitHub's Ubuntu 22.04 image, and `check-program --max-glibc 2.35` passed on that program: RUN, section 9. The program was not started on a 22.04 system.) `--portable` removes the `GLIBCXX`, `CXXABI` and `GCC` needs and two of the libraries; it cannot remove the glibc one (the game opens the Vulkan loader, ALSA
and X11 at run time, which need the system's C library), so **the floor is whatever system built the program**, and the release is built on the oldest one it should run on: the CI jobs build on Ubuntu 22.04 and
pass `--max-glibc 2.35`. Whether to strip a release is the owner's. **The build does not depend on its folder (RUN):** the program built again in a clean local clone, in another checkout folder and another build folder (the rehearsal of the `linux-release` job, section 6), has the sha256 `dc128308aca2da4ecf41b178a4c24d926c88ee0e7ae3a4275249a0a54d4b19e6`, the same as the one above, and so the same archive.

## 6. What was run in this step, and what was not

All on the owner's PC, 2026-10-27, in the Linux subsystem (Ubuntu 24.04, g++ 13.3, CMake 3.28, Ninja 1.11, Python 3.12) unless the row says Windows. The Step in
`docs/planning/2026-09-11-magic-portals-remaster.md` has the gates, the deviations and the mutation table; the logs are under `out/`, which is not in the repository. **This section is the record of step 209 as it stood on 2026-10-27 and is not edited; what ran on GitHub afterwards, and so which of its NOT RUN items are no longer true, is section 9.**

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

**NOT RUN (on 2026-10-27; section 9 lists what ran afterwards):** a dispatch of any of the four files; a rehearsal of the `tools` job's new `Packages` step; the `windows` job in any part (no Windows program was built, linked or started in this step); the `android` job (the SDK and the NDK were not used);
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
4. `release.py sums <release folder> --check`, then the upload to a GitHub Release, which is his and which no workflow here does. (Steps 1 and 2, the Windows, Linux and Android parts of step 3, and step 4 for those three files were done for the first release in November 2026; the macOS and iOS archives were not made. The Release's file names and its sums are not the packager's: section 9.)

`LICENSE` and `NOTICE.md` are at the root of the repository, and every archive the packager writes carries them (the jobs of `ci.yml` and `release.yml` that assemble an archive take them from the checkout; until planning step 210 they were invented one-line files).

## 8. The public-readiness check (planning step 210, PORT-12)

`python tools/ci/check_public.py` reads the files git tracks (`git ls-files`, so it gives the same answer in a clean clone, where `out/`, `reference/` and `build/` do not exist) and prints a table of findings with a class, a rule, a place and a sentence. It exits 1 when there is a finding of class ERROR and 0 otherwise. It changes nothing and fetches nothing; standard library and git only. `ci.yml`'s `tools` job runs it. **Whether that step works on a runner is NOT KNOWN:** `ci.yml` has not been dispatched (sections 2 and 9).

| Class | Meaning | Exit |
|---|---|---|
| ERROR | a defect of the files a stranger reads | 1 |
| RECORD-PATH | a path of the maintainer's PC inside a record file (`DEVLOG.md`, `docs/parity-backlog.md`, `docs/planning/`, `CLAUDE.md`): counted per file and listed by line; a record says what was true when it was written | 0 |
| OWNER-FILE | a finding in a file only the owner changes (`OWNER_FILES` in the tool: `CLAUDE.md`, one file of `docs/`, `.gitignore`, `.gitmodules`): listed with its lines, never edited by the tool | 0 |
| NOTE | a fact the owner should see: the ten largest files, an e-mail address, an allow-listed file, the one allowed placeholder, a record file's wording | 0 |
| HISTORY | something in a past commit (only with `--history`); nothing is removed by anyone for it | 1 |

**The rules** (each has a test that passes a synthetic repository and fails an edited copy of it, `tools/ci/test_check_public.py`): H1 no tracked file with the extension of a picture, sound, binary, package, key or original-game data file; H2 no NUL byte (a binary) and no file over 8 MiB; H3 no key or token shaped text (a PEM private key, GitHub and AWS tokens, a Slack token, a keystore password written as a literal; Android's published debug keystore password is allow-listed by name, with its reason printed); H4 no path of the maintainer's PC (a Windows user folder, `/home/<name>`, `/Users/<name>`, a drive mounted under `/mnt`) and not the name of the account that runs the check, which is read from the system when it runs and is never printed (a name of five letters or more is looked for anywhere, even glued to letters or digits, as the program check does; one of four letters as a whole word; a match that lies inside an e-mail address of a person is reported by the e-mail note instead, so an ordinary address of the maintainer that starts with the name is a note and not an error; a placeholder address and a GitHub noreply address are not addresses of a person for this rule and are hits); H5 none of a list of phrases the project keeps out of its files (`KEPT_OUT_PHRASES` in the tool), and the one placeholder line of `SECURITY.md` nowhere else; H6 `.gitignore` still has its `reference/` line, the comment block above it (at least 25 comment lines, the file has 30, and the block still says "DELIBERATELY EXCLUDED" and "DELIBERATELY UNANCHORED") and no line that unignores anything under it; H7 `LICENSE`, `NOTICE.md`, `README.md`, `CONTRIBUTING.md`, `SECURITY.md`, `docs/platforms.md` and `docs/building.md` exist and are not empty; H8 `.gitmodules` has only https URLs without credentials and each submodule path is a gitlink; H9 every `.sh` file is mode 100755 and nothing else is executable; H10 every relative link of every tracked `.md` file resolves to a tracked file or folder, and every `#fragment` to a heading; H11 every `README, <Section>` style reference (the title between asterisks, in straight or curly double quotes, or bare; not the engine's README) names a heading that exists in `README.md`; H12 `tools/package/check_version.py` passes.

`--history` reads every commit reachable from any ref: H1 on every name ever added, H2 on every blob, H3 and H4 on the content of every blob (H4 not in the record files' blobs, which are counted), each pattern timed; a pattern that is not finished within `--history-budget` seconds is reported NOT RUN with its time and the number of blobs it had read, never silently. `--include-untracked` adds the files not yet added, for a run before a commit. `tools/ci/test_notice.py` checks that every licence text in `NOTICE.md` is the text of the file it says it comes from, and that `LICENSE` is the engine's licence text.

**What it cannot see:** a sentence that is false, a secret of a shape the patterns do not know, a picture saved under a name with another extension, or anything in the history that was never a tracked name. A rule is a floor and not a review.

## 9. The first dispatches of release.yml (2026-11-03, planning step 211)

All of this is from 2026-11-03. **RUN** here means GitHub ran it and the result was read from the run; **NOT KNOWN** means nobody has run it. Section 6 is the record of what was run before, on the owner's PC; apart from two pointers to this section it is as it was on 2026-10-27.

**The first dispatch failed in one job (RUN).** Run 41636396401, on commit `176d0a3`, took 15 min 8 s in total. The jobs `version`, `linux`, `android` (the shell APK), `apple-macos` and `apple-ios` succeeded. `windows` FAILED at the CMake configure of the Windows program,
with "Vulkan loader not found at C:\Windows/System32/vulkan-1.dll": `engine/CMakeLists.txt:150` requires `vulkan-1.dll` in `System32` when no Vulkan SDK is installed, and the `windows-2022` image has neither. `collect` then failed, by design (its last check says "not successful: windows"): a release run is complete only
when every job succeeded.

**The fix (commit `9224dc7`).** The `windows` jobs of `ci.yml` and `release.yml` run on `windows-2025-vs2026` (Windows Server 2025 with Visual Studio 2026, MSVC 19.51), which has `vulkan-1.dll`; it is the image that the maker's sibling game, Penumbra, runs its Windows CI on. The `RUNNERS` list of
`tools/ci/check_workflows.py` pins that name instead of `windows-2022`.

**The second dispatch succeeded in all six jobs (RUN).** Run 41677490291, on commit `f87b860`, with the engine pinned at `4261485`:

* `version` and `collect` passed. `collect` includes its check of the two Apple shells with `release.py assemble`: `check-program`'s `/Users/` rule did NOT fire on this game's real CI-built Mach-O files (the table of `docs/packaging.md` section 8 had it as not known).
* `linux`: Ubuntu 22.04, g++ 12.3, glibc 2.35; `check-program --max-glibc 2.35` passed.
* `windows`: `windows-2025-vs2026`, MSVC 19.51; `check-program` passed; `ProductVersion` and `FileVersion` are the project's version in its four-field form; `vulkan-1.dll` is delay-loaded.
* `android`: the shell APK without game data, debug-signed, with all THREE ABIs (`arm64-v8a`, `armeabi-v7a`, `x86_64`) built and their libraries checked.
* `apple-macos` and `apple-ios`: the two calls of `apple.yml`. That workflow therefore ran green on 2026-10-25 (twice) and again now, in both dispatches of `release.yml`.

The artifact `release-programs` (30 days) holds `windows/MagicPortals.exe` (sha256 `306651b47b1c387c7950a8cdc1cb31d8ceabbc5e65b8f67be506741c95997556`), `linux/MagicPortals` (sha256 `0d88887b16c933619c177a20e8305ed01e660df4a3b6afdf2223a887a93c2a8f`),
`android/MagicPortals-<version>-nodata-debug.apk`, `macos/MagicPortals-macOS-shell.zip`, `ios/MagicPortals-iOS-shell.ipa`, `SHA256SUMS.txt` and `VERSIONS.txt`; `sha256sum -c SHA256SUMS.txt` over them passed.

**What the release files are, and who made them (RUN on the owner's PC, not on GitHub).** The Windows and Linux archives were assembled there with `release.py assemble` from those two CI programs and his content package: `MagicPortals-<version>-windows-x64.zip` (24,800,932 bytes, sha256
`09d38e46604d57185bb637ab2a3715347c0d3c3880b9241166e241c245f5b2b6`) and `MagicPortals-<version>-linux-x86_64.tar.gz` (26,285,851 bytes, sha256 `ac1440c6468e0ef31c36f4aa1d4e4fb8fbbfe8637b50e68c03a144c1b806bda0`); a different assembly of the same inputs gave the same bytes. The Android release APK is not a CI product
(`docs/packaging.md` section 8). For the GitHub Release the three files are published under fixed names, so that the README's `releases/latest/download/<name>` links work: `MagicPortals-Windows.zip`, `MagicPortals-Linux.tar.gz` and `MagicPortals-Android.apk` (a copy of the release APK under a new name; its contents are unchanged),
beside `LICENSE`, `NOTICE.md` and a `SHA256SUMS.txt` made with `sha256sum`, because `release.py sums` only knows the versioned names. The repository is still private. The macOS and iOS archives with the game in them were not made.

**The engine moved from `2b5ed8b` to `4261485`** (the engine's main, with its 32-bit fix and hardening pass). RUN in the Linux subsystem, on a copy of the repository on the Linux disk (GCC 13): the game and its suites build with no warning and all 80 `ctest` tests pass at 64 bits (0 skipped, with the owner's data); the same sources built with
`-m32 -msse2 -mfpmath=sse` (a 32-bit x86 process, NOT an ARM one) also build with no warning and all 80 pass, the 128 win routes among them. The game also compiles and links for `armeabi-v7a`, `arm64-v8a` and `x86_64` with NDK r28 with no warning. The Python tests (RUN): `tools/package` 390 (4 skipped on Windows), `tools/ci` 317 (1 skipped).

**NOT KNOWN, after these runs.** `ci.yml` and `android-emulator.yml` have never been dispatched, so nothing about their steps is known on GitHub. Nobody has started the Windows program that CI built, on any PC: it was read as bytes. The minutes of each job of `release.yml` were not collected from the API, and the second run's total is not recorded here (the first run's 15 min 8 s ended in a failure).
Nothing has run on a 32-bit ARM device. The Linux program of the artifact was built on Ubuntu 22.04 and not started on it (its glibc ceiling, 2.35, was read with `objdump -T`); where the Linux archive of the release was started, and what nobody has started, is in `docs/platforms.md`.

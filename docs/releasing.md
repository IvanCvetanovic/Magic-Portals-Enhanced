# From this repository to a release: the path, and the list for going public

Planning step 210 (PORT-12). One page. It gathers what [ci.md](ci.md) section 7 and [packaging.md](packaging.md) section 8 say, and adds what the maintainer does once, before the repository is public. **Nothing on this page has been done**: no workflow has been dispatched except the Apple one, no release exists, and no archive has been tried on a player's PC. Each step names the file that has its commands.

## The numbered path

1. **The version.** `project(MagicPortals VERSION x.y.z ...)` in the top-level `CMakeLists.txt` is the one definition (the current number is a placeholder). `python tools/package/check_version.py` must say `ok`; it names every place that disagrees.
2. **The repository as a stranger sees it.** `python tools/ci/check_public.py` must print no ERROR ([ci.md](ci.md) section 8). Read its OWNER-FILE and RECORD-PATH lines too.
3. **The release programs.** Dispatch `release.yml` with the version (Actions, *release.yml*, *Run workflow*; [ci.md](ci.md) section 2 says what it costs). It builds the Windows, Linux, Android (a debug-signed shell) and Apple programs and uploads them with `SHA256SUMS.txt` and `VERSIONS.txt`. Never dispatched.
4. **Download and check.** The artifact `release-programs`; `sha256sum -c SHA256SUMS.txt` before using anything in it.
5. **The content package**, on the maintainer's PC, from the maintainer's own copy of the original: `python tools/package/package.py content --repo <repository> --reference <reference folder> --out <content package>` ([packaging.md](packaging.md) section 3).
6. **Check, then assemble, one platform at a time** ([packaging.md](packaging.md) section 8 has every command):
   `release.py check-program <program> --platform windows|linux|macos|ios`, then `release.py assemble --platform windows|linux --program <program> --content <content package> --out <release folder>` and, for the Apple platforms, `--shell <the shell> --content <content package>`. The macOS seal is invalid once the data is inside; [apple.md](apple.md) section 7 has the three routes.
7. **The Android release APK**, built on the maintainer's PC with the maintainer's own key (`tools/build_android.sh --release --keystore ... --data <content package>`), then `release.py android --apk ... --sdk ... --out <release folder>` ([android.md](android.md)). No key exists in the repository.
8. **The sums.** `release.py sums <release folder> --check`.
9. **The GitHub Release**, made by hand: the archives, `SHA256SUMS.txt`, and `LICENSE` and `NOTICE.md` beside the APK. No workflow does this.
10. **A try on a PC that is not the maintainer's.** Never done for any platform.

## Once, before the repository is public

* **Visibility** (Settings, *General*). Nothing here changes it.
* **The triggers.** The workflows run on a dispatch and on nothing else. A push and pull-request trigger for `ci.yml` is a change of the file and of rule W01 in `tools/ci/check_workflows.py` ([ci.md](ci.md) section 2).
* **The licence.** `LICENSE` is the MIT No Attribution text, the same as the engine's, proposed in planning step 210 until the maintainer confirms or replaces it. `NOTICE.md` carries the licence texts of what the game contains.
* **The owner's files.** `python tools/ci/check_public.py` prints every finding in the files that only the maintainer changes (`OWNER_FILES` in the tool) as OWNER-FILE, with its lines. They are the maintainer's to read and decide; no tool or step here edits them.
* **The records.** The same output counts the paths of the maintainer's PC in `DEVLOG.md`, `docs/parity-backlog.md`, `docs/planning/` and `CLAUDE.md` (RECORD-PATH). Keep them, rewrite them, or squash the history: the maintainer's choice, and nothing here rewrites history. `python tools/ci/check_public.py --history` lists what past commits hold (it takes a few minutes) and removes nothing.
* **The contact for security reports.** `SECURITY.md` has one placeholder line for it; switch on private vulnerability reporting for the repository.
* **The repository's description and topics** (Settings).
* **The README.** Read the front once more as a stranger would; it says there is no release yet, and that has to change when there is one.

## What has never been done

A dispatch of `ci.yml`, `release.yml` or `android-emulator.yml`; a Windows, Linux or Android program built by a runner; the Android crash-report dialog on any device; a release archive on a player's PC; a program on a real Mac, iPhone or iPad; a phone's speed, memory and heat; real two-finger play; sound off Windows; a Vulkan 1.1-only driver; the Android release key and the Apple signing identities; the choice of the Windows C++ runtime (carried or asked of the player), the application ids, the version number and the icon.

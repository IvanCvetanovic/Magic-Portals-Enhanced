# Parity rig — the original next to the remake

How to run the original Magic Portals in an emulator, capture any screen of it, capture the same
screen of the remake, and compare them. Set up 2026-09-14; every step below was run, not assumed.

**Everything captured from the original is Asantee-origin imagery.** It lives under `out/parity/`
(gitignored) and nowhere else — not in a commit, not in `DEVLOG.md`, not in anything published.
Only measurements taken *from* captures may leave that directory.

---

## 1. Which emulator image, and why only that one

The APK's only native library is `lib/armeabi/libApplication.so` — 32-bit ARM. On an x86 host that
code must be translated, and of the images on this machine exactly one both translates ARM and
keeps a 32-bit userspace:

| Image | `ro.product.cpu.abilist` | Runs the game |
|---|---|---|
| `android-30 / google_apis_playstore / x86` | `x86,armeabi-v7a,armeabi` | **yes** |
| `android-33+ / x86_64` (all variants) | `x86_64[,arm64-v8a]`, `abilist32` empty | no — no 32-bit at all |
| `android-21`, `android-26` x86 | `x86` | no — no translation |

AVD `MP_Original_API30`, `config.ini` overrides that matter:

```
hw.lcd.width=720          hw.lcd.height=1280        hw.lcd.density=320
hw.initialOrientation=landscape
hw.mainKeys=yes           # no software nav bar: the game gets the full 1280x720
hw.gpu.mode=swiftshader_indirect
```

Boot: `%ANDROID_HOME%\emulator\emulator.exe -avd MP_Original_API30 -no-snapshot -no-boot-anim -no-audio -gpu swiftshader_indirect -port 5554`

## 2. Three things that stop the original from starting, and the fixes

1. **Broken signature.** `adb install` fails with `INSTALL_PARSE_FAILED_NO_CERTIFICATES`: the
   archive's `META-INF/MANIFEST.MF` lists 3251 entries, and 128 of them — every
   `*.angelscript` source — were stripped out after signing. The compiled `android_game.bin` is
   intact, so the game does not need them. Fix: copy every entry except `META-INF/`, `zipalign -p 4`,
   `apksigner sign` with a local keystore.
2. **No EGL config under host GPU.** With `-gpu host` it crashes in
   `GLContextFactory.createContext` with `IllegalArgumentException`. Cause:
   `net/asantee/gs2d/ConfigChooser.java:48-66` accepts only an *exact* RGB565 config and returns
   `null` otherwise, and `null` goes straight into `eglCreateContext`. SwiftShader offers a 565
   config; the host GPU path does not. Fix: `-gpu swiftshader_indirect`. No code patch needed.
3. **Zero-length taps are missed.** `adb shell input tap` is shorter than one frame under software
   GL + ARM translation, so the engine never sees it. Fix: a held press,
   `input swipe x y x y 250`. `tools/parity/emu.py` does this for every tap.

The locally rebuilt APK also carries `android:debuggable="true"` (apktool, manifest only). That
turned out to be unnecessary — progress is not in app-private storage, see §3 — but it is harmless.
All rebuilt APKs and the keystore live in `reference/apk_work/`, gitignored.

The Google Play licence check (`check_license` in `classes.dex`) did **not** block startup.

## 3. Reaching any of the 128 levels

Progress is a plain ENML file the shell can write:

```
/sdcard/.ethanon/net.asantee.magicportals/files/scores.enml
```

That is `GetGlobalExternalStorageDirectory()`, built in `GS2DActivity.java:219-220`. (The other
root, `/sdcard/Android/data/<pkg>/files/`, is unreadable to `adb` and to `run-as` on Android 11.)
After finishing level 1-1 with a gold medal it contained:

```
version { number = 1_4; }
world0 { level0 = 3; }
```

Writing `levelN = 1` for all 32 levels of all 4 worlds unlocked every chapter-1 level (bronze
medals, chapter at 33%) but chapter 2 stayed locked with the caption **"requires 60%"**. Writing
`= 3` for all of them showed every chapter at 100% and unlocked all four. The medal value is what
gates chapters; see `docs/ethanon-formats.md`, 2026-09-14 block.

Saves for specific states are kept in `out/parity/saves/`. Force-stop the app before pushing one.

## 4. Capturing the remake

```
tools\godot.bat --path game --resolution 1280x720 -- --parity-level=1-1 --parity-out=C:/abs/out.png --parity-delay=6
```

`game/scripts/dev/parity_capture.gd`. Must run with a window: `--headless` has no renderer. It
unlocks the level in memory only and disables saving for the run.

## 5. Screen map of the original (1280x720)

Main menu → **Tap Start** → chapter select (2 chapters per page, 2 pages) → level grid (4x4,
16 per page, 2 pages; opens on the page holding the last unlocked level) → level. In a level:
restart and pause top-right, left/right movement pads in the two bottom corners, a centred
"Part N" caption that fades out within ~6 s. Pause shows level-list and resume buttons, a
golden-score plaque, sound/music toggles and Achievements. Finishing shows "level finished",
the medal with the portal count, and restart / next / level-list buttons.

Tap coordinates are in `tools/parity/emu.py` → `Screen`.

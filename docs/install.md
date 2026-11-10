# How to install

Free, with no ads and no accounts. There is no installer: you unpack a folder and start the game from it. (On an iPhone or iPad you instead need a sideloading tool on a computer and your own Apple ID: see [below](#iphone-and-ipad).)

Pick your device: [Windows PC](#windows-pc), [Android phone or tablet](#android-phone-or-tablet), [Linux PC](#linux-pc), [Mac](#mac) or [iPhone and iPad](#iphone-and-ipad). The Mac and iPhone files are new, carry no developer's signature, and have never been run on a real Mac or iPhone (see [Platforms](../README.md#platforms)).

| | File | Download |
|---|---|---|
| Windows | `MagicPortals-Windows.zip` | [Download](https://github.com/IvanCvetanovic/Magic-Portals-Enhanced/releases/latest/download/MagicPortals-Windows.zip) |
| Android | `MagicPortals-Android.apk` | [Download](https://github.com/IvanCvetanovic/Magic-Portals-Enhanced/releases/latest/download/MagicPortals-Android.apk) |
| Linux | `MagicPortals-Linux.tar.gz` | [Download](https://github.com/IvanCvetanovic/Magic-Portals-Enhanced/releases/latest/download/MagicPortals-Linux.tar.gz) |
| Mac | `MagicPortals-macOS.zip` | [Download](https://github.com/IvanCvetanovic/Magic-Portals-Enhanced/releases/latest/download/MagicPortals-macOS.zip) |
| iPhone and iPad | `MagicPortals-iOS.ipa` | [Download](https://github.com/IvanCvetanovic/Magic-Portals-Enhanced/releases/latest/download/MagicPortals-iOS.ipa) |

Every release also has a [`SHA256SUMS.txt`](https://github.com/IvanCvetanovic/Magic-Portals-Enhanced/releases/latest/download/SHA256SUMS.txt) with the checksum of each file, and the [Releases page](https://github.com/IvanCvetanovic/Magic-Portals-Enhanced/releases) lists them all. Each download holds the whole game. The Windows and Linux archives hold the program, a `content` folder with the levels, pictures and sounds, a `PLAYING.txt` with a shorter version of this help, and the licence and notices; the Android file is the app itself, with the game inside.

The download is **not code-signed** (it has no digital signature from a publisher), so Windows asks you to confirm before it runs it. That is a fact about the file and not a warning about its contents; the checksums let you check that you have the file that was published.

## Windows PC

**You need:** a 64-bit Windows PC and a graphics driver that supports Vulkan 1.2 (most PCs from the last eight years or so have one; update the driver through Windows Update, or from NVIDIA, AMD or Intel). The game has been run on Windows 11; whether it starts on Windows 10 was not tried.

> **Check this first on Windows 11:** because the download is not code-signed, a PC with **Smart App Control** turned on may refuse to run it, and then there is no *Run anyway* button. To see whether it is on, open **Settings**, **Privacy & security**, **Windows Security**, **App & browser control**, **Smart App Control settings**. If it says *On*, the game will probably not start; turning it off is a choice about your whole PC, so decide for yourself.

**[Download for Windows](https://github.com/IvanCvetanovic/Magic-Portals-Enhanced/releases/latest/download/MagicPortals-Windows.zip)**

1. **Keep the download.** If the browser says the file *isn't commonly downloaded*: in Edge, click the **···** next to it, then **Keep**, then **Show more**, then **Keep anyway**. In Chrome, choose **Keep**. Browsers say this about new files that few people have downloaded yet.
2. **Extract the zip.** In your *Downloads* folder, right-click *MagicPortals-Windows*, choose **Extract All**, then **Extract**. Do not play from inside the zip: the game needs all of its files.
3. **Open the new folder**, then the folder inside it. There you will find **MagicPortals** (with file extensions shown it is *MagicPortals.exe*) beside the folders `assets` and `content`. Keep it there: the game finds its files next to itself.
4. **Double-click MagicPortals** to start the game.
5. **If a blue window says “Windows protected your PC”**, click **More info**, then **Run anyway**. Windows shows this because the game is new and not signed.

Have fun!

<details>
<summary>It didn't start?</summary>

- **“VCRUNTIME140.dll”, “MSVCP140.dll” or “VCRUNTIME140_1.dll was not found”, or a procedure entry point that was not found** (Windows' own message, before the game opens): the PC lacks Microsoft's C++ runtime, or has an older one than the game was built with. Install the **latest** *Microsoft Visual C++ Redistributable (x64)* from [Microsoft](https://learn.microsoft.com/cpp/windows/latest-supported-vc-redist), even if you already have one of 2015-2022, then start the game again. The download does not carry it.
- **A message about “MFPlat.DLL”** on a Windows *N* edition: install the *Media Feature Pack* from Windows' optional features (the game decodes its sounds with Windows Media Foundation).
- **“No Vulkan graphics driver was found”**: update the graphics driver. A very old PC may not be able to run the game.
- **The game says its data was not found:** it was started from inside the zip, or `MagicPortals.exe` was moved out of its folder. Extract the whole zip and start the program where it is.
- **Nothing happens, or a Smart App Control message:** see *Check this first on Windows 11* above.
- **Anything else:** read the log, `%TEMP%\magicportals.log`, and [report a problem](#reporting-a-problem) with it.

</details>

## Android phone or tablet

**You need:** Android 8.0 or newer and a phone or tablet whose graphics support Vulkan 1.1 (many phones from about 2018 on do, not all; a phone with only 1.1 has not been tried). The same file installs on a phone whose Android is 32-bit (some cheap phones are, mainly ones with Android Go) and on one whose Android is 64-bit; the installer picks the right program inside it. The 32-bit program has been compiled, and the game's tests have run as a 32-bit x86 stand-in on a PC; it **has not been started on a 32-bit phone**: if yours is one, please say how it went ([report a problem](#reporting-a-problem)).

**[Download for Android](https://github.com/IvanCvetanovic/Magic-Portals-Enhanced/releases/latest/download/MagicPortals-Android.apk)**

1. **Open the downloaded file** (in the notification, or in your *Downloads* or *Files* app).
2. **If Android says it can't install apps from this source**, tap **Settings**, switch on **Allow from this source** for the app you opened the file with (your browser or *Files*), go back, and tap **Install**. Android asks this for every app that does not come from a store.
3. **If Google Play Protect says the app is unrecognised**, tap **More details**, then **Install anyway**. The app is not published in a store, so Google has not seen it before.
4. **Open the game.** The first start copies the game's files into the app, which takes a moment on a black screen; the next starts are quicker. The game plays in landscape.

<details>
<summary>It didn't install or start?</summary>

- **“App not installed” or “not compatible”:** your phone is older than Android 8 (or an older version of this app, signed with another key, is installed: uninstall it first). The game cannot run there.
- **A message about Vulkan when the game starts:** the phone's graphics do not support Vulkan 1.1; the message is the graphics system's own words, and it helps a report.
- **The game says it has no space:** it needs about 26 MB free for its copy of the files, besides the app.
- **Anything else:** [report a problem](#reporting-a-problem) with your phone model and its Android version.

</details>

## Linux PC

**You need:** a 64-bit Linux on an x86-64 CPU, from about 2022 on (glibc 2.35 or newer, which Ubuntu 22.04 has), in a desktop session with an X11 display (on a Wayland desktop it needs XWayland, which was not tried), a Vulkan 1.2 driver with the Vulkan loader, and ALSA for sound. On Debian and Ubuntu:

```
sudo apt install libvulkan1 mesa-vulkan-drivers libasound2
```

(On Ubuntu 24.04 and newer the last package is called `libasound2t64`. NVIDIA's own driver brings its own Vulkan driver, so `mesa-vulkan-drivers` is for AMD and Intel graphics.)

**[Download for Linux](https://github.com/IvanCvetanovic/Magic-Portals-Enhanced/releases/latest/download/MagicPortals-Linux.tar.gz)**

1. **Unpack it with `tar`**, which keeps the program's executable bit, in a folder you may write to:
   ```
   tar xzf MagicPortals-Linux.tar.gz
   cd MagicPortals-*-linux-x86_64
   ```
2. **Start the game:**
   ```
   ./MagicPortals
   ```
   in a desktop session (some file managers do not start a program of this kind by a double-click). It must stay next to the folders `assets` and `content`. If you unpacked it some other way and it will not run, `chmod +x MagicPortals` restores the executable bit.

<details>
<summary>It didn't start?</summary>

- **“error while loading shared libraries: libvulkan.so.1”**: install the Vulkan loader (`libvulkan1`).
- **“GLFW could not report the required Vulkan instance extensions”**: the loader is there but no Vulkan driver is. Install your graphics card's driver (`mesa-vulkan-drivers` for AMD and Intel).
- **“version `GLIBC_...' not found”**: the system's C library is older than the one the program needs (2.35).
- **The game says its data was not found:** the program was moved out of its folder. Start it where it was unpacked.
- **Anything else:** read the log, `magicportals.log` in your temporary folder (usually `/tmp`), and [report a problem](#reporting-a-problem) with it.

</details>

## Mac

**You need:** macOS 13.3 or newer, and a Mac whose graphics give the program Vulkan 1.2 through MoltenVK (only a virtual Apple-silicon graphics card was tried). One app holds code for both Apple-silicon and Intel Macs; the one test Mac was Apple-silicon, so only that half is thought to have started with the game. **What is known:** nobody has started it on a real Mac. A GitHub test Mac signed the app again with the game inside it and started it in five languages on a virtual graphics card; how it looks, plays and sounds on a real Mac is not known, and the Intel half has not run with the game in it at all.

**[Download for Mac](https://github.com/IvanCvetanovic/Magic-Portals-Enhanced/releases/latest/download/MagicPortals-macOS.zip)**

1. **Check the download first:** `shasum -a 256 MagicPortals-macOS.zip` in the Terminal must print the line for it in [`SHA256SUMS.txt`](https://github.com/IvanCvetanovic/Magic-Portals-Enhanced/releases/latest/download/SHA256SUMS.txt). Do this before you override any of macOS's checks.
2. **Open the zip** (double-click it in Finder). It holds a folder `MagicPortals` with `MagicPortals.app`, a `READ ME FIRST.txt`, and the licence and notices. Put the app wherever you like.
3. **Open the app.** macOS is expected to stop it the first time with a message that it cannot check the app for malicious software (the wording depends on the macOS version; nothing was tried). The app is signed only "ad hoc", which names no developer, and Apple has not notarised it. That is a fact about the file and not a finding about its contents; the checksums let you check that you have the file that was published.
4. **To allow it,** on macOS 15: dismiss the message, open *System Settings*, *Privacy & Security*, find the line about MagicPortals and choose **Open Anyway**. On macOS 13 and 14: hold **Control**, click the app, choose **Open**, then **Open** again. These are the usual steps for an app from outside the App Store; none has been tried with this app. Later versions (macOS 26 and newer) are not described here and nothing was tried on them: follow the prompt on screen and Apple's own page on opening an app from outside the App Store.
5. **Some people instead run, in the Terminal,** `xattr -dr com.apple.quarantine ` followed by the app (type the command, then drag the app into the window to fill in its path). That is a shortcut from the community rather than one of Apple's own steps (inferred), it removes the mark that makes macOS check the app, and it is not tried with this app: use it only after step 1 matched.

<details>
<summary>It didn't open or start?</summary>

- **"Damaged and can't be opened":** the file may have been changed on the way. Download it again, check it against `SHA256SUMS.txt` (`shasum -a 256 MagicPortals-macOS.zip`) and open the zip with Finder.
- **Anything else:** [report a problem](#reporting-a-problem) with your Mac's model, its processor (Apple silicon or Intel) and its macOS version. The game's log is in the temporary folder (`magicportals.log`).

</details>

## iPhone and iPad

**You need:** iOS or iPadOS 16.3 or newer on a 64-bit iPhone or iPad with Metal, a computer with a **sideloading tool** (Sideloadly, AltStore or SideStore), and an Apple ID. Those tools are made by other people: the maker of this game has not used, reviewed or vouched for any of them, and they ask you to sign in with your Apple ID, so read each tool's own site and privacy statement first (some people use a separate Apple ID for this). **What is known:** nothing beyond the file itself. It has never been installed or started anywhere, not on a device, not in a simulator and not on a test machine; whether it installs, starts and plays is the open question, and the steps below are what those tools document for any app from outside the App Store, not something tried with this one.

**[Download for iPhone and iPad](https://github.com/IvanCvetanovic/Magic-Portals-Enhanced/releases/latest/download/MagicPortals-iOS.ipa)**

1. **Install a sideloading tool** and follow its own instructions. The file cannot be installed as it is: the tool signs it again with your own Apple ID when it puts it on your device.
2. **Give the tool `MagicPortals-iOS.ipa`** and install it on your iPhone or iPad.
3. **Trust it on the device** (*Settings*, *General*, *VPN & Device Management*) and, on iOS 16 and newer, turn on *Developer Mode* (*Settings*, *Privacy & Security*) if the device asks, as the tools describe. Developer Mode is a setting meant for developers that Apple says lowers the device's security; it can be turned off again (from Apple's documentation, not tried here).
4. **A free Apple ID** limits an app installed this way to seven days: after that it stops opening and has to be installed again (that is how the tools and Apple document it; not tried here). The icon is a placeholder.
5. **The app turns on file sharing:** its *Documents* folder shows in the Files app, and a small flags file left there is read at the next start (`docs/apple.md` section 10, APPLE-21). Nothing was installed to see what else is kept there.

## Playing

The modern on-screen controls are the default: the two arrows in the bottom corners walk, a touch or click where you want a portal shoots it, and the buttons at the top restart, pause and clear your portals. On a desktop the mouse stands in for a finger, and `←` `→` or `A` `D` walk, `R` restarts the level and `Esc` pauses. The original's classic layout is one tile away in Options, where *Adjust controls* also changes the size, opacity and place of the modern ones. [The README](../README.md#controls) has the list.

## Where the game keeps things

Your medals, the sound and music switches, your achievements, your choice of controls and your choice of language (`language.json`) are kept between runs, in `%APPDATA%\Magic Portals` on Windows, `~/.local/share/Magic Portals` on Linux, `~/Library/Application Support/Magic Portals` on a Mac, and in the app's own storage on Android and on an iPhone or iPad (uninstalling the app removes them; on an iPhone this is inferred, never seen). On a Mac the game also keeps a copy of its shaders and a pipeline cache in `~/Library/Caches/com.ivancvetanovic.magicportals`, made again at each start (the cache's place is inferred from the log), so delete that folder too when you remove the game; on an iPhone or iPad the same is expected in the app's own `Library/Caches` (inferred, never seen). To start from nothing, delete that folder. The game changes nothing else outside its own folder (and a log and a cache of files it can make again in the temporary folder). To remove the game, delete its folder and, if you like, the one above.

## Reporting a problem

[Report a problem](https://github.com/IvanCvetanovic/Magic-Portals-Enhanced/issues/new/choose): the form asks for what a fix needs (your system and its version, your graphics card and the game version) and for the log. Do not attach the game's files.

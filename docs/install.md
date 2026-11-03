# How to install

Free, with no ads and no accounts. There is no installer: you unpack a folder and start the game from it.

Pick your system: [Windows PC](#windows-pc) or [Linux PC](#linux-pc). Android, Mac, iPhone and iPad are not released yet (see [Platforms](../README.md#platforms)).

| | File | Download |
|---|---|---|
| Windows | `MagicPortals-Windows.zip` | [Download](https://github.com/IvanCvetanovic/Magic-Portals-Enhanced/releases/latest/download/MagicPortals-Windows.zip) |
| Linux | `MagicPortals-Linux.tar.gz` | [Download](https://github.com/IvanCvetanovic/Magic-Portals-Enhanced/releases/latest/download/MagicPortals-Linux.tar.gz) |

Every release also has a [`SHA256SUMS.txt`](https://github.com/IvanCvetanovic/Magic-Portals-Enhanced/releases/latest/download/SHA256SUMS.txt) with the checksum of each file, and the [Releases page](https://github.com/IvanCvetanovic/Magic-Portals-Enhanced/releases) lists them all. Each download holds the whole game: the program, a `content` folder with the levels, pictures and sounds, a `PLAYING.txt` with a shorter version of this help, and the licence and notices.

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

- **“VCRUNTIME140.dll was not found” or “MSVCP140.dll was not found”** (Windows' own message, before the game opens): the PC lacks Microsoft's C++ runtime. Install the *Microsoft Visual C++ Redistributable for Visual Studio 2015-2022 (x64)* from [Microsoft](https://learn.microsoft.com/cpp/windows/latest-supported-vc-redist), then start the game again. The download does not carry it.
- **A message about “MFPlat.DLL”** on a Windows *N* edition: install the *Media Feature Pack* from Windows' optional features (the game decodes its sounds with Windows Media Foundation).
- **“No Vulkan graphics driver was found”**: update the graphics driver. A very old PC may not be able to run the game.
- **The game says its data was not found:** it was started from inside the zip, or `MagicPortals.exe` was moved out of its folder. Extract the whole zip and start the program where it is.
- **Nothing happens, or a Smart App Control message:** see *Check this first on Windows 11* above.
- **Anything else:** read the log, `%TEMP%\magicportals.log`, and [report a problem](#reporting-a-problem) with it.

</details>

## Linux PC

**You need:** a 64-bit Linux on an x86-64 CPU, from about 2022 on (glibc 2.35 or newer, which Ubuntu 22.04 has), in a desktop session with an X11 display (on a Wayland desktop it needs XWayland, which was not tried), a Vulkan 1.2 driver with the Vulkan loader, and ALSA for sound. On Debian and Ubuntu:

```
sudo apt install libvulkan1 mesa-vulkan-drivers libasound2t64
```

(`libasound2t64` is `libasound2` on systems older than Ubuntu 24.04. NVIDIA's own driver brings its own Vulkan driver, so `mesa-vulkan-drivers` is for AMD and Intel graphics.)

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
   (or double-click it in your file manager, in a desktop session). It must stay next to the folders `assets` and `content`. If you unpacked it some other way and it will not run, `chmod +x MagicPortals` restores the executable bit.

<details>
<summary>It didn't start?</summary>

- **“error while loading shared libraries: libvulkan.so.1”**: install the Vulkan loader (`libvulkan1`).
- **“GLFW could not report the required Vulkan instance extensions”**: the loader is there but no Vulkan driver is. Install your graphics card's driver (`mesa-vulkan-drivers` for AMD and Intel).
- **“version `GLIBC_...' not found”**: the system's C library is older than the one the program needs (2.35).
- **The game says its data was not found:** the program was moved out of its folder. Start it where it was unpacked.
- **Anything else:** read the log, `magicportals.log` in your temporary folder (usually `/tmp`), and [report a problem](#reporting-a-problem) with it.

</details>

## Playing

The modern on-screen controls are the default: the two arrows in the bottom corners walk, a touch or click where you want a portal shoots it, and the buttons at the top restart, pause and clear your portals. On a desktop the mouse stands in for a finger, and `←` `→` or `A` `D` walk, `R` restarts the level and `Esc` pauses. The original's classic layout is one tile away in Options, where *Adjust controls* also changes the size, opacity and place of the modern ones. [The README](../README.md#controls) has the list.

## Where the game keeps things

Your medals, the sound and music switches, your achievements and your choice of controls are kept between runs, in `%APPDATA%\Magic Portals` on Windows and `~/.local/share/Magic Portals` on Linux. To start from nothing, delete that folder. The game changes nothing else outside its own folder (and a log and a cache of files it can make again in the temporary folder). To remove the game, delete its folder and, if you like, the one above.

## Reporting a problem

[Report a problem](https://github.com/IvanCvetanovic/Magic-Portals-Enhanced/issues/new/choose): the form asks for what a fix needs (your system and its version, your graphics card and the game version) and for the log. Do not attach the game's files.

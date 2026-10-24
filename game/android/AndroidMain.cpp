// Android's way into the game (planning step 203, PORT-5; made thin in step 204). The engine's android_main (engine/src/platform/android/AndroidApp.cpp) calls
// SupersonicMain once the activity is resumed and has a window; this does what a desktop's folder layout and command line did before main():
//
//  - THE FILES. The APK carries the game's data and the engine's shaders as assets (tools/package/android_package.py), but the game reads files by
//    path and an APK's assets have none. So they are unpacked once into the app's private files folder, and again only when the APK's list says
//    they changed. EVERY DECISION OF THAT is sim/Unpack.hpp's (pure, and pinned by test_mp_unpack): this file only hands it the APK as a Source
//    (AAssetManager) and the folder as a Sink (Unpack::FsSink), and shows the player a dialog when it fails. The game is pointed at the copy with
//    --root; the working directory becomes <files>/engine, which is where the engine looks for assets/shaders.
//  - DEVELOPMENT FLAGS. A phone has no command line, so a scripted run leaves its flags in <files>/magicportals_args.txt (whitespace-separated, one flag
//    per line or several on a line, double quotes around a token with spaces). Unpack::TakeFlagsFile reads, parses and consumes it (renamed to
//    magicportals_args.used, so a scripted run does not become every later launch). adb writes the private file through run-as, which only a debuggable
//    APK allows: a release APK has no way to receive flags. A scripted run never meets the unpack's failure dialog (nobody would close it).
//  - NOTHING ELSE. The saves go where the engine puts a user's data on Android, <files>/Magic Portals/ (internal storage: they survive the app being
//    stopped and are removed only by uninstalling); the log and the generated files go to <files>/Magic Portals/scratch (sim/Scratch.hpp, main.cpp).
//    No --saves is passed: it is a developer's flag, and it would make the run "not a player's", which suppresses the dialog a start that cannot
//    find a Vulkan driver needs (RunKind.hpp).
//
// What is NOT an Android call is not here: the rules for the flags file, for the dialog, for the exit code after the unpack, for the argument list and for
// the log of a start that stopped in the unpack are Unpack's (planning step 204, D3), where a host compiler builds them and a suite runs them.

#include <android/asset_manager.h>
#include <android/native_activity.h>
#include <android_native_app_glue.h>

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#include "BuildProfile.hpp"
#include "MagicPortalsMain.hpp"
#include "core/Log.hpp"
#include "platform/ExecutablePath.hpp"
#include "platform/android/AndroidApp.hpp"
#include "sim/Scratch.hpp"
#include "sim/Unpack.hpp"

// An Android program is a phone's build, whatever else was configured: its lowest Vulkan version (1.1) is the phone profile's, and the engine's own
// default (1.2) refuses many phones' stock drivers. CMake refuses a configure that says otherwise (game/CMakeLists.txt); this is the compiler's word.
static_assert(MagicPortals::kMobileBuild, "the Android program must be configured with MAGICPORTALS_MOBILE (it is, unless a configure turned it OFF)");

namespace {

namespace fs = std::filesystem;
namespace Unpack = MagicPortals::Unpack;

// The names that tools/package/android_package.py writes and this reads. A text tie in tools/package/test_android_package.py reads these lines out of
// this file and refuses a packager that spells any of them differently.
constexpr const char* kListAsset = "gamedata.list";
constexpr const char* kStampFile = "magicportals_gamedata.stamp";
constexpr const char* kContentFolder = "GameData";
constexpr const char* kEngineFolder = "engine";
constexpr const char* kArgsFile = "magicportals_args.txt";
constexpr const char* kArgsUsed = "magicportals_args.used";
constexpr const char* kTitle = "Magic Portals";
constexpr const char* kLogTag = "Magic Portals";

// The APK's assets, read through the platform's asset manager.
class AssetSource : public Unpack::Source {
public:
    explicit AssetSource(AAssetManager* manager) : manager_(manager) {}

    bool ReadList(std::string& text, std::string& why) override {
        text.clear();
        std::string refusal;
        const bool ok = ReadAsset(
            kListAsset,
            [&text, &refusal](const char* data, std::size_t size) { return Unpack::AppendListChunk(text, data, size, refusal); },
            why);
        if (!ok && !refusal.empty()) why = refusal; // the cap stopped the read
        return ok;
    }

    bool ReadAsset(const std::string& path, const Unpack::ChunkFn& chunk, std::string& why) override {
        if (manager_ == nullptr) {
            why = "the app has no asset manager";
            return false;
        }
        AAsset* asset = AAssetManager_open(manager_, path.c_str(), AASSET_MODE_STREAMING);
        if (asset == nullptr) {
            why = "the APK has no asset called " + path;
            return false;
        }
        char buffer[Unpack::kReadChunkBytes];
        int read = 0;
        bool ok = true;
        while ((read = AAsset_read(asset, buffer, sizeof buffer)) > 0) {
            if (!chunk(buffer, static_cast<std::size_t>(read))) {
                ok = false; // the caller said why
                break;
            }
        }
        if (ok && read < 0) {
            why = "reading " + path + " from the APK failed";
            ok = false;
        }
        AAsset_close(asset);
        return ok;
    }

private:
    AAssetManager* manager_;
};

// The game's own log file is opened (and truncated) by MagicPortalsMain, which a start that stopped in the unpack never reaches. So the words of that stop go
// into the same file here, by the same rule (the last log kept beside it), and the report Android shows at the next start (supersonic.reportLog in the
// manifest) has something to show (PORT5-10 (a)).
void RecordInLogFile(const std::string& text) {
    try {
        const fs::path base = Supersonic::UserDataDirectory(kTitle);
        if (base.empty()) return;
        const fs::path scratch = base / MagicPortals::Scratch::kUserScratchFolder;
        std::string why;
        if (!Unpack::RecordFailure((scratch / MagicPortals::Scratch::kLogFileName).string(), (scratch / MagicPortals::Scratch::kPreviousLogFileName).string(), text, why)) {
            SUPERSONIC_LOG_WARN(kLogTag) << "The log file could not be written: " << why;
        }
    } catch (const std::exception& e) {
        // Said where it can be: a failure to record a failure must not end the dialog and the exit code that tell the player.
        SUPERSONIC_LOG_WARN(kLogTag) << "The log file could not be written: " << e.what();
    }
}

// What stopped the start, said in every place: the system's log, the game's log file, and (for a player's own launch only: Unpack::ShowsFailureDialog)
// a dialog.
void Tell(const std::string& text, bool scripted) {
    SUPERSONIC_LOG_ERROR(kLogTag) << text;
    RecordInLogFile(text);
    if (Unpack::ShowsFailureDialog(scripted)) Supersonic::Android::ShowMessage(kTitle, text);
}

} // namespace

int SupersonicMain(int argc, char** argv) {
    bool scripted = false;
    try {
        const fs::path files = Supersonic::Android::InternalDataPath();
        if (files.empty()) {
            Tell("Magic Portals cannot start: the phone gave the app no place to keep its files.", false);
            return EXIT_FAILURE;
        }
        Unpack::FlagsFile devFlags = Unpack::TakeFlagsFile(files.string(), kArgsFile, kArgsUsed);
        scripted = devFlags.present;
        if (!devFlags.info.empty()) SUPERSONIC_LOG_INFO(kLogTag) << devFlags.info;
        if (!devFlags.error.empty()) SUPERSONIC_LOG_ERROR(kLogTag) << devFlags.error;
        if (!devFlags.warning.empty()) SUPERSONIC_LOG_WARN(kLogTag) << devFlags.warning;

        // THE UNPACK. The pump keeps the activity answering: its UI thread waits for this one to read its lifecycle commands, and a thread that
        // does not for about five seconds is "not responding". There is no window to draw progress in (the engine's surface does not exist yet,
        // and drawing on the native window first would clash with the Vulkan surface made later), so a first run is a black screen for a few
        // seconds and the progress is a log line.
        android_app* app = Supersonic::Android::App();
        AssetSource source(app != nullptr && app->activity != nullptr ? app->activity->assetManager : nullptr);
        Unpack::FsSink sink(files.string(), kStampFile);
        Unpack::Options options;
        options.ownedRoots = {kContentFolder, kEngineFolder};
        options.pump = [] { return Supersonic::Android::PumpEvents(); };
        options.progress = [](std::size_t done, std::size_t total, std::uint64_t bytes) {
            SUPERSONIC_LOG_INFO(kLogTag) << "Unpacking the game: " << done << " of " << total << " files, " << bytes << " bytes.";
        };
        const Unpack::Result unpacked = Unpack::Run(source, sink, options);
        switch (Unpack::NextAfter(unpacked.outcome)) {
        case Unpack::Next::Continue:
            SUPERSONIC_LOG_INFO(kLogTag) << Unpack::Summary(unpacked);
            break;
        case Unpack::Next::ExitSuccess:
            // The app was asked to finish while it unpacked: not a failure, and nothing to tell. The next start unpacks again from the top.
            SUPERSONIC_LOG_INFO(kLogTag) << Unpack::Summary(unpacked);
            return EXIT_SUCCESS;
        case Unpack::Next::ExitFailure:
            Tell(unpacked.error, scripted);
            return EXIT_FAILURE;
        }

        // The window may have gone while the files were unpacked (the player pressed Home on the black screen of a first run): wait for it to come
        // back, or end quietly if the app was closed. Without it the engine's window would throw "no window to draw on" and the player would be told
        // their graphics are at fault.
        Supersonic::Android::WaitUntilDrawable();
        if (Supersonic::Android::DestroyRequested()) return EXIT_SUCCESS;

        // Where the engine finds assets/shaders (its asset root is the working directory when that holds them), and where its relative writes land.
        std::error_code ec;
        fs::current_path(files / kEngineFolder, ec);
        if (ec) SUPERSONIC_LOG_ERROR(kLogTag) << "Cannot enter " << (files / kEngineFolder).string() << ": " << ec.message();

        std::vector<std::string> args = Unpack::LaunchArguments(argc > 0 && argv != nullptr ? argv[0] : nullptr, (files / kContentFolder).string(), devFlags.args);
        std::vector<char*> pointers;
        for (std::string& arg : args) pointers.push_back(arg.data());
        pointers.push_back(nullptr);
        return MagicPortalsMain(static_cast<int>(args.size()), pointers.data());
    } catch (const std::exception& e) {
        // MagicPortalsMain never throws; this is for the unpack and the shim, whose exceptions would otherwise end the game's thread with
        // std::terminate, a black screen and the launcher with nothing said.
        Tell(std::string("Magic Portals stopped because of an error:\n\n") + e.what(), scripted);
    } catch (...) {
        Tell("Magic Portals stopped because of an error that is not a std::exception.", scripted);
    }
    return EXIT_FAILURE;
}

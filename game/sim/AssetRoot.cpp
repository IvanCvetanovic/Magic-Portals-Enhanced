#include "sim/AssetRoot.hpp"

#include <algorithm>
#include <filesystem>
#include <system_error>

namespace MagicPortals::AssetRoot {

namespace {

// A drive letter is an ASCII letter, always: std::isalpha asks the C locale, which a module that calls itself host-independent must not (planning step
// 203, PORT2-3 (c); neither the game nor the engine calls setlocale, so this changes no answer, it removes the dependence).
bool IsLetter(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }

std::string WithForwardSlashes(std::string text) {
    std::replace(text.begin(), text.end(), '\\', '/');
    return text;
}

Paths PathsUnder(const std::string& root, const Layout& layout) {
    Paths paths;
    paths.levels = Join(root, layout.levels);
    paths.art = Join(root, layout.art);
    paths.chapters = Join(root, layout.chapters);
    paths.achievements = Join(root, layout.achievements);
    paths.data = Join(root, layout.data);
    paths.portData = Join(root, layout.portData);
    paths.original = Join(root, layout.original);
    paths.padArt = Join(root, layout.padArt);
    return paths;
}

std::string Quoted(const std::string& text) { return "'" + text + "'"; }

} // namespace

Layout Layout::Repo() {
    Layout layout;
    layout.name = "repo";
    layout.levels = "out/levels";
    layout.art = "out";
    layout.chapters = "out/data/chapters.json";
    layout.achievements = "out/data/achievements.json";
    layout.data = "data";
    layout.portData = "game/data";
    layout.original = "reference/extracted/assets";
    layout.padArt = "reference/magic-rampage/controls";
    return layout;
}

Layout Layout::Packaged() {
    Layout layout;
    layout.name = "packaged";
    layout.levels = "levels";
    layout.art = ".";
    layout.chapters = "tables/chapters.json";
    layout.achievements = "tables/achievements.json";
    layout.data = "data";
    layout.portData = "port-data";
    layout.original = "original";
    layout.padArt = "pad-art/controls";
    return layout;
}

bool Paths::Empty() const {
    return levels.empty() && art.empty() && chapters.empty() && achievements.empty() && data.empty() && portData.empty() &&
           original.empty() && padArt.empty();
}

const char* SourceName(Source source) {
    switch (source) {
    case Source::Flag: return "--root";
    case Source::BesideExecutable: return "beside the executable";
    case Source::Bundle: return "the application bundle";
    case Source::UserData: return "the user's data folder";
    case Source::CompiledIn: return "compiled in";
    case Source::None: return "none";
    }
    return "none";
}

bool IsAbsolute(const std::string& text) {
    const std::string s = WithForwardSlashes(text);
    if (s.empty()) return false;
    if (s[0] == '/') return true;
    return s.size() >= 3 && IsLetter(s[0]) && s[1] == ':' && s[2] == '/';
}

std::string Normalize(const std::string& text) {
    if (text.empty()) return {};
    const std::string s = WithForwardSlashes(text);

    // The root part, which a ".." can never climb out of: "//" of a network name, "C:/", "C:" (a drive's own current folder) or "/".
    std::string prefix;
    std::size_t at = 0;
    if (s.size() >= 2 && s[0] == '/' && s[1] == '/' && (s.size() == 2 || s[2] != '/')) {
        prefix = "//";
        at = 2;
    } else if (s.size() >= 2 && IsLetter(s[0]) && s[1] == ':') {
        prefix = s.substr(0, 2);
        at = 2;
        if (at < s.size() && s[at] == '/') prefix += '/';
    } else if (s[0] == '/') {
        prefix = "/";
        at = 1;
    }
    const bool rooted = !prefix.empty() && prefix.back() == '/';

    std::vector<std::string> kept;
    while (at <= s.size()) {
        const std::size_t end = std::min(s.find('/', at), s.size());
        const std::string segment = s.substr(at, end - at);
        at = end + 1;
        if (segment.empty() || segment == ".") continue;
        if (segment == "..") {
            if (!kept.empty() && kept.back() != "..") {
                kept.pop_back();
            } else if (!rooted) {
                kept.push_back(segment);
            }
            continue;
        }
        kept.push_back(segment);
    }

    std::string out = prefix;
    for (std::size_t i = 0; i < kept.size(); ++i) {
        if (i > 0) out += '/';
        out += kept[i];
    }
    return out.empty() ? std::string(".") : out;
}

std::string Join(const std::string& dir, const std::string& relative) {
    if (dir.empty()) return {};
    if (relative.empty() || relative == ".") return Normalize(dir);
    // A folder that is only slashes is the file system's root, however many it is spelled with: it takes the separator from its own text, or
    // "/" + "/" + "content" is "//content", which Normalize reads as a network name (planning step 203, PORT2-3 (b)). "//" has always joined as "/".
    const std::string head = WithForwardSlashes(dir);
    if (head.find_first_not_of('/') == std::string::npos) return Normalize("/" + relative);
    return Normalize(dir + "/" + relative);
}

Resolved Resolve(const Inputs& inputs, const HoldsGame& holdsGame) {
    Resolved resolved;
    const Layout shapes[] = {Layout::Packaged(), Layout::Repo()};

    const auto found = [&](const std::string& dir, Source source) {
        for (const Layout& shape : shapes) {
            resolved.tried.push_back(dir + " (" + shape.name + ")");
            if (!holdsGame(dir, shape)) continue;
            resolved.ok = true;
            resolved.source = source;
            resolved.root = dir;
            resolved.layout = shape.name;
            resolved.paths = PathsUnder(dir, shape);
            resolved.why = "asset root: " + dir + " (tier: " + SourceName(source) + ", layout: " + shape.name + ")";
            return true;
        }
        return false;
    };
    const auto looked = [&shapes] {
        std::string text;
        for (const Layout& shape : shapes) {
            if (!text.empty()) text += " or ";
            text += shape.levels + "/ and " + shape.chapters + " (" + shape.name + ")";
        }
        return text;
    };

    // 1. --root, strict.
    if (!inputs.flagRoot.empty()) {
        if (!IsAbsolute(inputs.flagRoot)) {
            // Absolute, because the game makes the engine's folder its working directory before it opens anything: a relative root
            // would be looked for there and found nowhere (--padart is strict for the same reason).
            resolved.error = "--root needs an absolute folder, got " + Quoted(inputs.flagRoot);
            return resolved;
        }
        const std::string dir = Normalize(inputs.flagRoot);
        if (found(dir, Source::Flag)) return resolved;
        resolved.error = "--root " + Quoted(dir) + " does not hold the game; it must hold " + looked();
        return resolved;
    }

    // 2. next to the executable.
    if (!inputs.exeDir.empty()) {
        const std::string exe = Normalize(inputs.exeDir);
        if (found(Join(exe, "content"), Source::BesideExecutable)) return resolved;
        if (found(exe, Source::BesideExecutable)) return resolved;
    }
    // 3. the application bundle.
    if (!inputs.bundleContent.empty() && found(Normalize(inputs.bundleContent), Source::Bundle)) return resolved;
    // 4. the user's data folder, asked for only now.
    if (inputs.userDataContent) {
        const std::string userData = inputs.userDataContent();
        if (!userData.empty() && found(Normalize(userData), Source::UserData)) return resolved;
    }

    // 5. what this build was configured with, taken as it is.
    if (!inputs.compiledIn.Empty()) {
        resolved.ok = true;
        resolved.source = Source::CompiledIn;
        resolved.paths.levels = Normalize(inputs.compiledIn.levels);
        resolved.paths.art = Normalize(inputs.compiledIn.art);
        resolved.paths.chapters = Normalize(inputs.compiledIn.chapters);
        resolved.paths.achievements = Normalize(inputs.compiledIn.achievements);
        resolved.paths.data = Normalize(inputs.compiledIn.data);
        resolved.paths.portData = Normalize(inputs.compiledIn.portData);
        resolved.paths.original = Normalize(inputs.compiledIn.original);
        resolved.paths.padArt = Normalize(inputs.compiledIn.padArt);
        resolved.why = std::string("asset root: the paths this build was configured with (tier: ") + SourceName(Source::CompiledIn) + ")";
        return resolved;
    }

    std::string where;
    for (const std::string& tried : resolved.tried) where += (where.empty() ? "" : ", ") + tried;
    resolved.error = "the game's data was not found" + (where.empty() ? std::string() : " (looked in " + where + ")") +
                     ". It must hold " + looked() + ". Give the folder with --root <absolute folder>; a released game keeps it in a "
                     "'content' folder next to the program, or in the 'content' folder of the user's data folder for Magic Portals.";
    return resolved;
}

bool HoldsGameOnDisk(const std::string& dir, const Layout& layout) {
    std::error_code levelsError;
    std::error_code chaptersError;
    // Looked at by the error_code overloads, which answer "no" for a folder that cannot be asked about instead of throwing.
    return std::filesystem::is_directory(Join(dir, layout.levels), levelsError) && !levelsError &&
           std::filesystem::is_regular_file(Join(dir, layout.chapters), chaptersError) && !chaptersError;
}

Paths Apply(const Paths& base, const Overrides& overrides) {
    Paths paths = base;
    if (overrides.levels) {
        paths.levels = *overrides.levels;
        // The converter writes the art beside the levels, so levels somewhere else bring their art with them unless --art says
        // otherwise (what --levels has always done).
        if (!overrides.levels->empty()) paths.art = Normalize(*overrides.levels + "/..");
    }
    if (overrides.art) paths.art = *overrides.art;
    if (overrides.chapters) paths.chapters = *overrides.chapters;
    if (overrides.achievements) paths.achievements = *overrides.achievements;
    if (overrides.data) paths.data = *overrides.data;
    if (overrides.portData) paths.portData = *overrides.portData;
    if (overrides.original) paths.original = *overrides.original;
    if (overrides.padArt) paths.padArt = *overrides.padArt;
    return paths;
}

} // namespace MagicPortals::AssetRoot

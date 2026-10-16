#pragma once

// The five on-screen controls, and the one place that says where each is drawn and
// where a touch counts as on it.
//
// WHY A MODULE. Before this the pads' geometry was asked of sim/Hud in six places of the
// layer: the draw, the claim, the camera's `onPad`, the lift gate, the tutorial hand's gate
// and the ring - each calling Hud::PadRect, PadCorner or OnPad with the same four
// arguments, and the corner buttons told apart from clear-portals by the address of their
// rules. A second layout (the modern one, docs/planning/2026-10-15-modern-controls.md) would
// have had to be threaded through all six, and one left behind is a pad that draws in one
// place and presses in another. Here they all ask once.
//
// WHAT IS HERE. Two layouts, one module. The classic functions (planning step 179, `Classic*`):
// each CALLS the Hud function it stands for and returns what that returned, so the original's
// arithmetic is not written a second time and cannot drift by an ulp from the one the 128
// sweeps and the claim-map digest pin; `Hud.*` and ui.json are untouched. And, beside them
// (planning steps 180 and 181, the section "the modern controls" below), the modern layout of
// docs/planning/2026-10-15-modern-controls.md: its data (`Rules`, game/data/controls.json), what
// a player changed (`Tuning`), `LayOut` for a `Style`, and the policy that says which style a
// run plays (`Policy`, `ResolvePolicy`, `ResolveStyle`). The names say which a function is.
//
// Pure: no registry, no window, no file but the one `LoadRules` is given. Units are the HUD's
// design units, in VIEW space, (0, 0) the view's top-left, +y down.

#include <array>
#include <cstddef>
#include <map>
#include <optional>
#include <string>

#include <glm/glm.hpp>

#include "core/Json.hpp"

#include "sim/Hud.hpp"

namespace MagicPortals::Controls {

// The controls, in the order a touch is claimed: where two of them could both be under a
// point, the earlier wins (a walk pad over a corner button, restart over pause over clear).
enum class Id { Left, Right, Reset, Menu, Clear };

inline constexpr std::array<Id, 5> kClaimOrder = {Id::Left, Id::Right, Id::Reset, Id::Menu, Id::Clear};

// The two walk pads. Everything else is a corner button, which a press works and which is
// placed by a Hud::Placement rather than hung from a screen corner.
constexpr bool IsWalk(Id id) {
    return id == Id::Left || id == Id::Right;
}

// Where a touch counts as on a control. A pad's is a disc about its screen corner - strictly
// inside, as the original's isPointInSphere - and a corner button's is its drawn rectangle,
// closed on every edge, as Hud::Rect::Contains. A rectangle may also be half-open
// ([min, max): a point on a shared edge belongs to the later control), which the classic
// controls never use.
//
// The default shape is an empty disc, so a control that has not been laid out yet claims
// nothing, not the one point (0, 0) a zero-sized closed rectangle would.
struct HitShape {
    enum class Kind { Rect, Disc };

    Kind kind = Kind::Disc;
    Hud::Rect rect;               // Rect: the area
    bool halfOpen = false;        // Rect: the max edges are outside
    glm::dvec2 centre{0.0};       // Disc
    double radius = 0.0;          // Disc: points strictly closer than this are inside

    static HitShape Closed(const Hud::Rect& rect);
    static HitShape HalfOpen(const Hud::Rect& rect);
    static HitShape Disc(const glm::dvec2& centre, double radius);

    bool Contains(const glm::dvec2& point) const;
};

// One control as laid out for a view and a level's age: the rectangle it is drawn in and the
// area a touch on it counts in. Whether it is shown, and at what alpha, are the layer's to
// say - they depend on the level's state, not on where a control sits.
struct Frame {
    Hud::Rect rect;
    HitShape hit;
};

// ---- the classic controls ---------------------------------------------------------------

// A walk pad, slid in as far as the level's age says: Hud::PadRect for the drawn rectangle
// and a disc of Hud::Rules::Pads::hitRadiusUnits about Hud::PadCorner for the claim. For
// an id that is not a walk pad, an empty frame.
Frame ClassicWalkFrame(const Hud::Rules& rules, Id walk, const glm::dvec2& viewUnits, double ageMs);

// A corner button's frame wherever it is drawn - flat in its corner, coming in, or being
// dismissed: the claim is the rectangle it is drawn in.
Frame ClassicCornerFrame(const Hud::Rect& drawn);

// How far outside its corner a pad still is: Hud::PadSlideUnits.
double ClassicSlideUnits(const Hud::Rules& rules, double ageMs);

// The point a walk pad's ring is centred on: the pad's screen corner, sliding with it
// (Hud::PadCorner). Not the middle of the drawn square. A zero point for any other id.
glm::dvec2 ClassicRingCentre(const Hud::Rules& rules, Id walk, const glm::dvec2& viewUnits, double ageMs);

// Whether a point is on EITHER walk pad, whether or not the pads are shown: Hud::OnPad
// for Left, then for Right. The camera's `onPad`, the lift gate and the tutorial hand's gate
// all read this, and it is not gated by the pads being drawn - a touch resting in the corner
// of a weightless level, which has no pads, still counts as on one.
bool ClassicOnWalkControl(const Hud::Rules& rules, const glm::dvec2& viewUnits, double ageMs, const glm::dvec2& point);

// The pads' alpha as a fraction and as the byte it is made from (Hud::PadOpacity and
// Hud::PadAlphaByte). The byte is what a level's end decays the pads from, so the pulse and
// the decay share one function and cannot start a byte apart.
double ClassicWalkOpacity(const Hud::Rules& rules, double ageMs, bool tutorial);
int ClassicWalkAlphaByte(const Hud::Rules& rules, double ageMs, bool tutorial);

// A corner button's placement: restart, pause and clear-portals as ui.json has them. An
// empty placement for a walk pad, which has none.
const Hud::Placement& ClassicPlacement(const Hud::Rules& rules, Id corner);

// The rectangle a corner button sits in once it is in place (Hud::Place of its placement).
Hud::Rect ClassicPlace(const Hud::Rules& rules, Id corner, const glm::dvec2& viewUnits);

// ---- the modern controls ----------------------------------------------------------------------
//
// Planning step 180 (docs/planning/2026-10-15-modern-controls.md): the layout of Magic Rampage's
// inset controls as data and arithmetic. The classic functions above are not touched by it. The layer
// reads the policy and the data (planning step 181) and, from planning step 182 on, plays the layout.
// Everything below is pure and takes the data it needs as an
// argument: the modern Rules (game/data/controls.json), the classic Hud::Rules (the slide, the
// pulse and the ring are the classic's own numbers, read from ui.json so they cannot drift apart),
// a Tuning (what a player changed) and the view's size and safe-area insets.
//
// Units are the HUD's design units, in VIEW space, as above. The BOX is what the layout, the hit
// rule and the clamp are measured on; a picture is drawn into a rectangle derived from it (DrawRect).

// The two layouts. A run plays one of them.
enum class Style { Classic, Modern };

// What the layout is built on: the layout's version, which every saved move is stamped with. Whoever
// changes a default place bumps it, here and in controls.json, and TheDefaultPlacesArePinned... fails
// until they do. 1 was the pair grouped at the bottom left (planning step 180); 2 (planning step 186, the
// owner's decision of 2026-10-16 after he saw it on his screen) puts walk_right in the bottom-right corner.
inline constexpr int kLayoutVersion = 2;

// The three looks (design 1.1): a size and a bottom margin each, chosen from the census.
enum class Preset { Uniform, Balanced, Mr };
inline constexpr std::size_t kPresetCount = 3;
inline constexpr std::array<Preset, kPresetCount> kPresets = {Preset::Uniform, Preset::Balanced, Preset::Mr};

// How the two walk arrows stand (the owner's decision of 2026-10-16, planning step 186): `Split` is walk_left in the
// bottom-left corner and walk_right in the bottom-right one, each inset by the margin, as the classic pair stands but
// inset; `Grouped` is the arrangement of planning step 180, both at the bottom left. Split ships. Grouped stays as an
// alternative a capture or a test can ask for (`--controls modern,arrows=grouped`), never saved, so every pin written
// for the grouped pair is kept and aimed at it.
enum class Arrows { Split, Grouped };
inline constexpr std::array<Arrows, 2> kArrangements = {Arrows::Split, Arrows::Grouped};

// What restart, pause and clear-portals act on in the modern style: the press (as the classic style
// does, a recorded deviation) or the release inside the hit rectangle after a press inside it.
enum class ActOn { Release, Press };

// Where a control's picture comes from, in the order they are tried (design 4.2).
enum class ArtTier { MagicRampage, Original, Generated };
inline constexpr std::size_t kArtTierCount = 3;

// The names the data, the flag and the saved file call things by - one place, so no other file
// spells them. A name that is not one gives an empty optional.
const char* KeyOf(Id id);                              // walk_left, walk_right, restart, pause, clear_portals
std::optional<Id> IdOfKey(const std::string& key);
const char* NameOf(Style style);                       // classic, modern
std::optional<Style> StyleOfName(const std::string& name);
const char* NameOf(Preset preset);                     // uniform, balanced, mr
std::optional<Preset> PresetOfName(const std::string& name);
const char* NameOf(Arrows arrows);                     // split, grouped
std::optional<Arrows> ArrowsOfName(const std::string& name);
const char* NameOf(ActOn actOn);                       // release, press
const char* NameOf(ArtTier tier);                      // mr, original, generated

// How far inside the view the safe area starts, per side, in units (a notch, a rounded corner). All
// zero on a desktop. The margin is measured from the safe rectangle's edges, boxes are clamped inside
// it, and a hit rectangle's edge reach goes to the VIEW's edge.
struct Insets {
    double left = 0.0;
    double top = 0.0;
    double right = 0.0;
    double bottom = 0.0;
};

// A safe area given in WINDOW PIXELS (what Supersonic::SafeArea::Get answers), as the insets the layout takes, in design units. The view is
// the window stretched to it, so one pixel is `viewHeightUnits / windowHeightPx` units along both axes - the factor the pointer is converted
// by (MagicPortalsLayer::touchThisTick) - and a control held clear of the notch in pixels is held clear of it in units. A window with no
// height has no safe area to speak of, and a side that is negative or not a number is zero: a notch cannot cover less than nothing.
Insets InsetsFromWindowPx(double left, double top, double right, double bottom, double windowHeightPx, double viewHeightUnits);

// A grid the tuning moves on: whole steps, so a chain of them ends exactly on its limits and a step at
// a limit returns the same value. The value of step n is n / divisor, by ONE division.
struct Grid {
    double min = 0.0;
    double max = 0.0;
    double step = 0.0;
    int divisor = 1;     // 1 / step, whole
    int lowSteps = 0;    // min * divisor
    int highSteps = 0;   // max * divisor

    double ValueOf(int steps) const { return static_cast<double>(steps) / static_cast<double>(divisor); }
};

// The largest transparent margin, as a fraction of the picture on each side, that a picture the port DRAWS ITSELF (the generated
// tier, sim/PadArt) can be asked for: a quarter on each side keeps half the picture. LoadRules refuses a generated entry above it,
// so a value that loads is a value that draws; PadArt::kMaxBodyInset is this number. A picture read from a file (Magic Rampage's, the
// original's) carries whatever margin it carries, up to the loader's own bound.
inline constexpr double kMaxGeneratedBodyInset = 0.25;

struct ArtEntry {
    std::string file;
    double bodyInset = 0.0; // the fraction of the picture that is transparent on each side
};

// game/data/controls.json, read strictly (LoadRules).
struct Rules {
    double referenceHeightPx = 0.0; // Magic Rampage's 720: every px below is in it, converted by height

    int layoutVersion = 0;
    double marginSidePx = 0.0;
    double marginTopPx = 0.0;
    double marginBottomPx = 0.0; // layout.margin_px.bottom: the shared margin M. Each look names its own (required), which the layout uses
    double gapPx = 0.0;
    double boxPx = 0.0;
    double cornerBoxFactor = 0.0;
    double cornerMinSize = 0.0;
    double faceInset = 0.0;      // the solid face is the box shrunk by this fraction of it on each side

    struct PresetRule {
        int sizeSteps = 0;           // on the size grid
        double marginBottomPx = 0.0;
    };
    std::array<PresetRule, kPresetCount> presets;
    Preset defaultPreset = Preset::Uniform;
    Arrows defaultArrows = Arrows::Split; // layout.default_arrows: how the walk arrows stand when nothing asks otherwise

    struct Hit {
        std::array<double, 5> growPx{}; // by Id: growth toward the world, px
        double seamPx = 0.0;
        double edgeReachPx = 0.0;
    } hit;

    ActOn cornerActOn = ActOn::Release;
    double clearancePx = 0.0;

    Grid size;
    Grid opacity;
    int opacityDefaultSteps = 0;
    int baseAlphaByte = 0;
    double idleScale = 0.0;
    int pressedBoostByte = 0;
    double ringSizeOverBox = 0.0;
    double hintMarginUnits = 0.0;

    // The picture each control is drawn with, per tier, by Id; an empty optional where the tier has none.
    std::array<std::array<std::optional<ArtEntry>, 5>, kArtTierCount> art;

    struct Census {
        double emUnits = 0.0;
        double slackUnits = 0.0;
        double characterRowUnits = 0.0;
    } census;

    Style defaultMode = Style::Classic;
};

// controls.json, strictly: a number that is missing, not a number, or out of its range is refused with
// `error` naming it, because a control a unit off its corner looks almost right. Keys that begin with an
// underscore and keys this reader does not know are ignored; every key it DOES read is required, a
// preset's `margin_bottom_px` included (a misspelt one is a missing one, and is refused naming it).
bool LoadRules(const std::string& path, Rules& out, std::string& error);

// ---- the pictures a modern run draws (planning step 184) -----------------------------------------------

// A file name as controls.json may name a picture: ONE file in the folder of its tier, which the layer joins to that folder
// (Magic Rampage's, the original's sprites, the scratch directory the generated pictures are written to). Not empty, not "." or
// "..", no separator of either kind, no colon (a drive, or an NTFS stream), no control character (DEL included), none of the
// characters Windows refuses in a name (< > " | ? *), no ".." anywhere, not ending in a dot or a space (Windows would read it as another
// name), and not one of Windows' device names (CON, PRN, AUX, NUL, COM1 to COM9, LPT1 to LPT9, with or without an extension, in any case).
// LoadRules refuses an `art` entry that is not one, naming it, so that a name from the data can never climb out of the folder it is
// joined to - the generated tier WRITES there. LoadRules also refuses a data file in which two controls name one generated file.
bool IsBareFileName(const std::string& name);

// The sizes, in pixels, that the Magic Rampage tool recorded for its pictures in the manifest.json beside them
// (`outputs.<file>.size` = [width, height]), by file name.
struct ArtManifest {
    std::map<std::string, glm::ivec2> sizes;
};

// Reads `path`. A file that is not there is no manifest and no error: an empty ArtManifest and true (a folder of pictures dropped by
// hand has none). A file that is there and cannot be read, or whose `outputs` hold a size that is not two whole numbers from 1 to
// kMaxManifestPx, gives false and `why`, naming it, with `out` empty: the caller holds every picture to the default size.
inline constexpr int kMaxManifestPx = 16384;
bool ReadArtManifest(const std::string& path, ArtManifest& out, std::string& why);

// The size a Magic Rampage picture called `file` must have: the manifest's where it names the file, else `defaultPx` square
// (layout.box_px, the size every one of the five is made at). A picture of any other size would be drawn mis-scaled and say nothing.
glm::ivec2 ExpectedArtSize(const ArtManifest& manifest, const std::string& file, int defaultPx);

// A control's picture came from one of the tiers, or from none: a flat quad is drawn.
using ArtSource = std::optional<ArtTier>;

// The five tiers a run resolved, in the order of Id (walk_left, walk_right, restart, pause, clear_portals).
using ArtSources = std::array<ArtSource, 5>;

// "mr", "original", "generated" or "flat".
const char* NameOf(const ArtSource& source);

// The INFO line a run says once: `modern controls art: walk_left=mr walk_right=mr restart=mr pause=mr clear_portals=mr`, in the
// order of kClaimOrder's keys as the data spells them (walk_left, walk_right, restart, pause, clear_portals).
std::string ArtLine(const ArtSources& sources);

// True when the five do not all come from one tier: a mixed look (Magic Rampage's grey faces beside the original's white
// corner buttons, or the drawn glyphs beside either).
bool ArtMixed(const ArtSources& sources);

// A mixed look with a Magic Rampage picture IN it: a folder that is half there, or one with a file the loader had to refuse. That is
// worth a WARN (planning step 185, the owner's question 2 of step 184 taken as recommended). A mixed look WITHOUT one is the ordinary
// no-Magic-Rampage machine - the arrows drawn beside the original's own corner sprites - and is an INFO, once.
bool ArtHalfDressed(const ArtSources& sources);

// ---- the tuning ---------------------------------------------------------------------------------

// The keys a tuning is spelt with, in the flag (`--controls modern,size=1.2`) and in the saved file's `controls` object
// (`{"size":1.2,...}`): one place, so the parser, the reader and the writer cannot spell one two ways. `kMode`, `kVersion` and
// `kControls` are the saved file's own members around the tuning (the options step writes `{"version":1,"controls":{"mode":...,
// "layout":2,...}}`), and `kLayout` is the number of the arrangement the moves were made against. Not the keys of controls.json,
// which has its own objects of the same names.
namespace TuningKey {
inline constexpr const char* kPreset = "preset";
inline constexpr const char* kArrows = "arrows";
inline constexpr const char* kSize = "size";
inline constexpr const char* kOpacity = "opacity";
inline constexpr const char* kMove = "move";
inline constexpr const char* kLayout = "layout";
inline constexpr const char* kMode = "mode";
inline constexpr const char* kVersion = "version";
inline constexpr const char* kControls = "controls";
} // namespace TuningKey

// What `--controls modern,key=value,...` carried, as it was written: syntax and names are checked when
// it is parsed (before the data is read, so a typo exits at once), the grids when it is applied.
struct FlagItems {
    std::optional<Preset> preset;
    std::optional<Arrows> arrows;
    std::optional<double> size;
    std::optional<double> opacity;
    std::array<std::optional<glm::dvec2>, 5> move; // by Id

    bool Any() const;
    // A key of the TUNING was given: a size, an opacity or a move. `preset=` and `arrows=` are the look and the arrangement, a capture's
    // choice of where the controls stand, not something a player kept. A flag that Tunes() REPLACES the tuning a player kept (the layer
    // lays out with the flag's tuning alone, never a merge); one that does not leaves the kept tuning in force.
    bool Tunes() const;
};

// What a player changed from the look in force: a size, an opacity and a move of each control. Kept as
// WHOLE STEPS on the data's grids (the values are n / divisor by one division), unset where the player
// did not change it, so a later change to a default reaches a player who never touched the slider.
struct Tuning {
    // The look: a capture's (`preset=` of the flag), never saved. Unset is the data's default_preset.
    std::optional<Preset> preset;
    // How the walk arrows stand: a capture's or a test's (`arrows=` of the flag), never saved, so a saved file cannot strand a player on
    // an arrangement. Unset is the data's default_arrows.
    std::optional<Arrows> arrows;
    std::optional<int> sizeSteps;
    std::optional<int> opacitySteps;
    // Design units, screen direction (+x right, +y down), from the control's default place, NOT scaled by
    // size, bound to +-kMoveLimit and snapped to kMoveSnap. By Id.
    std::array<glm::dvec2, 5> move{};

    static constexpr double kMoveLimit = 1024.0;
    static constexpr double kMoveSnap = 0.1;

    // The look in force, and the size and opacity in force: the player's, or the look's and the data's.
    Preset PresetIn(const Rules& rules) const;
    Arrows ArrowsIn(const Rules& rules) const;
    int SizeStepsIn(const Rules& rules) const;
    int OpacityStepsIn(const Rules& rules) const;
    double Size(const Rules& rules) const;
    double Opacity(const Rules& rules) const;

    // Nothing differs from the look's own size and opacity and no control is moved.
    bool IsDefault(const Rules& rules) const;

    // The tuning made valid: a size and an opacity on their grids and inside them, an unset where one is
    // what the default is, a move finite, bound and snapped.
    Tuning Clamped(const Rules& rules) const;

    // One step of the size (up: +1, down: -1) or of the opacity; at a limit the same tuning comes back.
    Tuning SizeStepped(const Rules& rules, int direction) const;
    Tuning OpacityStepped(const Rules& rules, int direction) const;

    // The saved file's `controls` object, read tolerantly: a field that is missing or the wrong type is
    // the default, a size or an opacity off its grid is snapped to it, a number that is not finite is
    // the default; a `layout` that is not kLayoutVersion keeps the size and the opacity and DROPS the
    // moves, which were made against another arrangement. Unknown keys are ignored.
    static Tuning ReadFrom(const Supersonic::Json::Value& controls, const Rules& rules);

    // The same object, on one line, and only what differs from the default: `{}` for a default tuning.
    // Written from the clamped tuning, so a number that is not finite is never written. `move` holds the
    // non-zero entries only.
    std::string ToJson(const Rules& rules) const;

    // The members of that object, without its braces: `"size":1.2,"move":{...}`, empty for a default
    // tuning. For a writer that has members of its own to put in the same object (the saved file's `mode`
    // and `layout`), which splices these and does not strip a closed object. ToJson is `{` + this + `}`.
    std::string ToJsonMembers(const Rules& rules) const;

    // `key=value,key=value` (the part of the flag after the style): preset, size, opacity, and a move
    // as dx:dy under a control's key. Replaces what was saved rather than merging with it (the caller
    // does that). False with `error` for a bad item, an unknown key or a key given twice.
    static bool ParseFlag(const std::string& items, FlagItems& out, std::string& error);

    // The parsed flag applied to the data's grids: a size or an opacity not on its grid is refused.
    static bool FromFlag(const FlagItems& items, const Rules& rules, Tuning& out, std::string& error);
};

// The flag's usage, for the message that goes with a refusal.
const char* FlagUsage();

// ---- the saved choice (planning step 185) --------------------------------------------------------------------

// The file, inside whichever directory the game was handed: its own file and its own store, beside scores.json, audio.json and
// achievements.json, not a third key of audio.json (which Switches::Store rewrites from the two members it knows).
inline constexpr const char* kStoreFileName = "controls.json";

// The style a player chose in Options, and the tuning they made, kept between runs:
//
//   {"version":1,"controls":{"mode":"modern","layout":2}}
//
// `mode` is written once a player has chosen (and only then: a file of a player who never did has none, so the shipped default
// reaches them); `layout` is always kLayoutVersion, the arrangement a saved move was made against; and the tuning's own members
// (size, opacity, move) follow, DELTAS ONLY - Tuning::ToJsonMembers writes nothing that equals the default. A file saved against another
// layout number keeps its size and opacity and loses its moves (an expected migration); a move that no longer fits the view is pulled in
// by the layout and stays as it was stored here. CTL-9 edited the mode and wrote the tuning back unchanged; CTL-10 applies the stored
// tuning (the layer lays out with SavedTuning()) and gives the store its setter, which the adjustment screen uses and the tests exercise.
//
// Free of the engine, like Switches::Store, so a test can exercise all of it with a temporary directory and no window.
class Store {
public:
    // Reads `directory`/controls.json if there is one. An EMPTY directory is memory-only, as Scores::Store's and Switches::Store's are
    // (every suite builds the layer bare, and none of them may write a file) - and so is the directory the layer passes empty when
    // the policy forbids saving (`--controls modern` is a capture's flag: it writes nothing and ignores the file).
    //
    // The tolerant reading of design 2.4: a field that is missing or the wrong type is the default (Tuning::ReadFrom), a `mode` that is
    // not classic or modern is none. False, with `error`, only for a file that exists and is not a JSON object: the defaults are kept and
    // the file is written over at the next Save, as audio.json is.
    bool Open(const std::string& directory, const Rules& rules, std::string& error);

    // The style the player chose, or none: no file, a file without one, or a file that would not read.
    std::optional<Style> Mode() const { return m_mode; }
    // What the file held for the tuning (the default where it held none), as the player kept it: the layer plays it when no flag asks
    // otherwise, and Save writes it back (deltas only).
    const Tuning& SavedTuning() const { return m_tuning; }

    // The choice, in memory. A memory-only store keeps it for the run and writes nothing.
    void SetMode(Style style) { m_mode = style; }
    // The tuning to keep, in memory: made valid (Clamped) and without the look and the arrangement, which are a capture's and never
    // saved. A flag's tuning is never handed to it by the layer: the store holds what the player kept, so a choice made in Options while
    // a capture's flag is in force cannot write the flag's numbers. A memory-only store keeps it for the run and writes nothing.
    void SetTuning(const Tuning& tuning);

    // The bytes Save writes for what the store holds now: one line, no trailing newline.
    std::string Text() const;

    // Writes controls.json by a temporary and a rename, as Switches::Store::Save does and for its reason (a crash cannot leave half a file
    // where the choice was). Nothing is written when these bytes are the ones this store wrote last: a gesture repeated is one write.
    // A memory-only store writes nothing and succeeds.
    //
    // The live file is NEVER removed: the replace is the rename (which replaces in one step on Windows and on POSIX), and where the
    // rename is refused - another handle holds the file, a filesystem will not rename onto an existing name - Save says so and the file
    // that was there stays whole, the old bytes, for the next Save to try again. The temporary has a name of its own for every Save, so
    // two windows saving at once, or a leftover of an earlier run, cannot share one, and it is removed on every failure path.
    bool Save(std::string& error);

    // Whether this store has somewhere to write, and whether Open read a file.
    bool Persists() const { return !m_path.empty(); }
    bool FromFile() const { return m_fromFile; }
    const std::string& Path() const { return m_path; }

private:
    Rules m_rules;
    std::optional<Style> m_mode;
    Tuning m_tuning;
    std::string m_path;
    bool m_fromFile = false;
    std::string m_written; // the bytes this store last wrote, empty before the first Save
};

// ---- the layout ----------------------------------------------------------------------------------

// The five controls laid out for one view at one level age. For the modern style `rect` is the BOX and
// `hit` a half-open rectangle; for the classic style they are the Classic* functions' own.
struct Layout {
    Style style = Style::Classic;
    std::array<Frame, 5> frames;

    const Frame& At(Id id) const { return frames[static_cast<std::size_t>(id)]; }
};

// Where each control is and where a touch on it counts, `ageMs` into a level. In the classic style:
// exactly the Classic* functions. In the modern style: the five boxes from the data, the tuning's size and
// moves and the insets (Boxes), the hit rectangles from them (HitRects), and the pair slid in as far as
// the classic pads are at this age (the slide is Hud::Rules', so it cannot drift from the classic one).
// A corner button is laid out in its place; a button coming in or going out is moved by the layer
// (FollowDrawn).
Layout LayOut(Style style, const Rules& rules, const Hud::Rules& hud, const Tuning& tuning, const glm::dvec2& viewUnits,
              const Insets& insets, double ageMs);

// The five boxes, settled (no slide), in the order of Id: the margin from the safe rectangle's edges, the
// gap, the size, then the tuning's moves, then the shared clamp that keeps every box wholly inside the
// safe rectangle. A corner box is Hud::Place of its PlacementFor, so a button at rest and the end of its
// entrance are the same rectangle.
std::array<Hud::Rect, 5> Boxes(const Rules& rules, const Tuning& tuning, const glm::dvec2& viewUnits, const Insets& insets);

// The five hit rectangles of these boxes (design 1.4): each box grown by the control's growth, a side that
// faces another box across at most `seam` goes to the middle of the gap, a side within the edge reach of
// the safe rectangle's edge goes to the view's edge, and the whole is kept inside the view. A rectangle
// is read half-open ([min, max)), so a point on a shared edge is the later control's. Each is computed
// from the boxes alone, never from another's hit, so it does not depend on the order they are asked in.
std::array<Hud::Rect, 5> HitRects(const Rules& rules, const std::array<Hud::Rect, 5>& boxes, const glm::dvec2& viewUnits,
                                  const Insets& insets);

// A frame whose picture is drawn at `drawn` instead of where it was laid out: the hit rectangle moves with
// it. For the pair's slide and for a corner button's entrance and dismissal.
Frame FollowDrawn(const Frame& settled, const Hud::Rect& drawn);

// The rectangle a picture is drawn into so that its solid body fills the box's solid face (design 1.3): the
// face is the box shrunk by `faceInset` of the box on each side, and a picture with a transparent margin of
// `bodyInset` of itself on each side is drawn that much larger than the face.
Hud::Rect DrawRect(const Hud::Rect& box, double faceInset, double bodyInset);

// A corner button as a Hud::Placement (anchor, inset from that anchor's two edges, size), so the layer's
// HudEntered and HudDismissed run on it unchanged. The sprite is the classic placement's. Empty for a pad.
Hud::Placement PlacementFor(const Rules& rules, const Hud::Rules& hud, const Tuning& tuning, Id corner,
                            const glm::dvec2& viewUnits, const Insets& insets);

// The tuning's move for a control whose box's top-left is wanted at `wantedBoxMin`: the shared clamp, then
// the move from the default place, bound and snapped to a tenth. The inverse of the placement in Boxes, for a
// drag. For a wish that was a placement that fitted, MoveFor gives its move back exactly; for any other it
// gives the move of the clamped wish to the nearest tenth, so a box pulled in against an edge may stand up to
// half a tenth inside it, and a move asked for again is the same move.
glm::dvec2 MoveFor(const Rules& rules, const Tuning& tuning, Id id, const glm::dvec2& wantedBoxMin,
                   const glm::dvec2& viewUnits, const Insets& insets);

// The shared clamp: a box of `size` whose top-left is `min`, moved the least that puts it wholly inside the
// safe rectangle.
glm::dvec2 ClampBox(const glm::dvec2& min, const glm::dvec2& size, const glm::dvec2& viewUnits, const Insets& insets);

// ---- what a layout draws ---------------------------------------------------------------------------

// The alpha byte a modern control is drawn at, from the byte the classic one would be (a pad's pulse or
// the flat 120): min(255, round(byte x idle_scale x opacity)). At the default the byte is unchanged.
int AlphaByte(const Rules& rules, const Tuning& tuning, int classicByte);

// The ring a tutorial pad emits, on a pad's frame: centred on the box's centre and, at its full size, as many
// boxes across as `ring.size_over_box` says (the classic ring over its pad), in the classic ring's own timing
// and alpha (`ring`, from Hud::RingAt): its size grows by the same fraction of the full one.
struct RingPlace {
    bool shown = false;
    glm::dvec2 centre{0.0};
    double sizeUnits = 0.0;
    double alpha = 0.0;
};
RingPlace RingFor(const Rules& rules, const Hud::Rules& hud, const Hud::Ring& ring, const Frame& pad);

// Where 1-21's hint text starts. The classic style leaves it where it is. The modern one moves it right of
// the hit rectangle of every shown walk arrow that stands in the LEFT half of the view (the same nearer-edge
// test the slide uses) when the text's row meets one, and keeps it `hint.margin_units` inside the view's right
// edge: min(max(classicLeft, leftRight), W - extent - margin). An arrow in the right half is not one the text
// can be moved clear of by starting further right, so it is not asked (split: the right arrow stands in the
// bottom-right corner, and a text that cannot clear both is drawn over it as the classic text crosses its
// disc); grouped has both arrows in the left half, so it answers exactly as it did. Where the view's edge and
// the arrow's right edge disagree the view wins (the text stays on the screen and crosses the arrow); the rule
// only ever shifts the text right.
struct HintRow {
    double classicLeft = 0.0;
    double extent = 0.0; // the text's width in units, measured from the font
    double top = 0.0;
    double height = 0.0;
};
double HintLeft(Style style, const Rules& rules, const Layout& layout, const HintRow& row, const glm::dvec2& viewUnits,
                const std::array<bool, 5>& shown);

// ---- which style a run plays -------------------------------------------------------------------------

// What the command line and the kind of run say about the feature (design 2.1). `available` false means
// the feature does not exist in this run: no store, no data read, no entry in the menu.
struct Policy {
    bool valid = true;                // false: the flag was not readable, `error` says why
    std::string error;
    bool available = false;
    std::optional<Style> forced;      // the flag's style: the run's starting one
    bool saveAllowed = false;
    FlagItems flag;                   // the tuning keys that came with `--controls modern`

    // WHAT NO COMMAND LINE SAYS. The layer's `Paths::controls` IS this struct, so what a test or a later step hands the
    // layer beside the resolved policy lives here, in one place, each field defaulting to "classic, nothing" (a bare layer,
    // which is every suite's, meets none of them). ResolvePolicy never sets one of these five: the command line has no word
    // for them (main sets artDir itself, from --padart or the build's folder, after it has the policy). They are the plan's `Paths::controls` fields, added together by planning step 182 (the step that links the
    // layer to the module, and the layer, the routes and the later suites read them from here) rather than one at a time by
    // the step that first reads each, as step 181 had it.
    bool classicClaims = false;       // CTL-6: the modern layout and draw with the CLASSIC claims, a test's variant
    Insets testInsets;                // CTL-6 and CTL-7: a test's safe-area insets, in units; none on a desktop
    std::string artDir;               // CTL-8: where Magic Rampage's pictures are looked for; empty: nowhere
    bool withoutOriginalArt = false;  // CTL-8: a test's variant: the original's own corner sprites (the second tier) are not tried
    // CTL-10: a kept tuning a test injects IN PLACE OF the store's (the layer plays it as the player's file). A flag that carries a size, an
    // opacity or a move (FlagItems::Tunes) replaces it; one that carries only `preset=` / `arrows=`, or none, leaves it in force. Classic
    // never reads it.
    std::optional<Tuning> tuning;
};

// The policy of a run from `--controls <classic|modern>[,key=value...]` (absent: nullopt) and the three rig
// flags that make a run a developer's. Tuning keys on classic are refused.
Policy ResolvePolicy(const std::optional<std::string>& controlsFlag, bool fixedStep, bool route, bool visitLevels);

// The style a run plays: not available, Classic; forced, the forced one; else the saved one; else the shipped one.
Style ResolveStyle(const Policy& policy, const std::optional<Style>& saved, Style shippedDefault);

} // namespace MagicPortals::Controls

#include "sim/Adjust.hpp"

#include "core/Json.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <utility>

namespace MagicPortals::Adjust {

namespace {

namespace Json = Supersonic::Json;
using UiLayer::Read::Byte;
using UiLayer::Read::Positive;
using UiLayer::Read::Text;

// Long enough that every slide is over: the preview shows the controls at rest, as the level settles them.
constexpr double kSettledMs = 1.0e9;

constexpr const char* kBlock = "adjust";

std::string Named(const std::string& where, const char* key) {
    return where.empty() ? std::string(key) : where + "." + key;
}

bool Object(const Json::Value& block, const char* key, std::string& why, const std::string& where) {
    if (!block[key].IsObject()) {
        why = Named(where, key) + " is missing or not an object";
        return false;
    }
    return true;
}

// A number that must be there, finite, within [lo, hi].
bool Number(const Json::Value& block, const char* key, double lo, double hi, double& out, std::string& why, const std::string& where) {
    const Json::Value& value = block[key];
    if (!value.IsNumber() || !std::isfinite(value.AsNumber()) || value.AsNumber() < lo || value.AsNumber() > hi) {
        why = Named(where, key) + " is missing, not a number, or outside " + std::to_string(lo) + " to " + std::to_string(hi);
        return false;
    }
    out = value.AsNumber();
    return true;
}

// A whole number of `count` bytes: [r, g, b] or [r, g, b, a].
bool ReadBytes(const Json::Value& block, const char* key, std::size_t count, int* out, std::string& why, const std::string& where) {
    const Json::Value& value = block[key];
    if (!value.IsArray() || value.AsArray().size() != count) {
        why = Named(where, key) + " is missing or not " + std::to_string(count) + " numbers";
        return false;
    }
    int read[4] = {0, 0, 0, 0};
    for (std::size_t i = 0; i < count; ++i) {
        const Json::Value& channel = value.AsArray()[i];
        if (!channel.IsNumber() || !std::isfinite(channel.AsNumber()) || channel.AsNumber() < 0.0 || channel.AsNumber() > 255.0 ||
            channel.AsNumber() != std::floor(channel.AsNumber())) {
            why = Named(where, key) + " holds something that is not a whole number from 0 to 255";
            return false;
        }
        read[i] = static_cast<int>(channel.AsNumber());
    }
    for (std::size_t i = 0; i < count; ++i) out[i] = read[i];
    return true;
}

bool ReadColour(const Json::Value& block, const char* key, Colour& out, std::string& why, const std::string& where) {
    int bytes[4] = {0, 0, 0, 0};
    if (!ReadBytes(block, key, 4, bytes, why, where)) return false;
    out = Colour{bytes[0], bytes[1], bytes[2], bytes[3]};
    return true;
}

bool ReadRgb(const Json::Value& block, const char* key, std::array<int, 3>& out, std::string& why, const std::string& where) {
    int bytes[3] = {0, 0, 0};
    if (!ReadBytes(block, key, 3, bytes, why, where)) return false;
    out = {bytes[0], bytes[1], bytes[2]};
    return true;
}

// A whole number from 1 up.
bool Whole(const Json::Value& block, const char* key, int& out, std::string& why, const std::string& where) {
    double value = 0.0;
    if (!Number(block, key, 1.0, 1.0e6, value, why, where)) return false;
    if (value != std::floor(value)) {
        why = Named(where, key) + " is not a whole number";
        return false;
    }
    out = static_cast<int>(value);
    return true;
}

bool ReadTile(const Json::Value& block, const char* key, TileSize& out, std::string& why, const std::string& where) {
    if (!Object(block, key, why, where)) return false;
    const Json::Value& tile = block[key];
    const std::string at = Named(where, key);
    TileSize read;
    if (!Positive(tile, "box_px", read.boxPx, why, at) || !Positive(tile, "offset_px", read.offsetPx, why, at) ||
        !Number(tile, "face_inset", 0.0, 0.25, read.faceInset, why, at)) {
        return false;
    }
    out = read;
    return true;
}

std::string Lower(std::string text) {
    for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

// One tier's eight entries. A file is a bare file name; no two of a tier are the same file (in any case: Windows reads them as one).
bool ReadArt(const Json::Value& block, const char* tier, std::array<std::optional<Controls::ArtEntry>, kPictureCount>& out, std::string& why,
             const std::string& where) {
    if (!Object(block, tier, why, where)) return false;
    const Json::Value& entries = block[tier];
    const std::string at = Named(where, tier);
    std::array<std::optional<Controls::ArtEntry>, kPictureCount> read;
    for (const Picture picture : kPictures) {
        const char* key = KeyOf(picture);
        if (!Object(entries, key, why, at)) return false;
        const Json::Value& entry = entries[key];
        const std::string here = Named(at, key);
        Controls::ArtEntry art;
        if (!Text(entry, "file", art.file, why, here) || !Number(entry, "body_inset", 0.0, 0.25, art.bodyInset, why, here)) return false;
        if (!Controls::IsBareFileName(art.file)) {
            why = here + ".file is not a bare file name (no folder, no \"..\", no drive): \"" + art.file + "\"";
            return false;
        }
        for (const Picture other : kPictures) {
            if (other == picture) break;
            if (Lower(read[static_cast<std::size_t>(other)]->file) == Lower(art.file)) {
                why = here + ".file names the same file as " + Named(at, KeyOf(other)) + ".file: each picture is a file of its own";
                return false;
            }
        }
        read[static_cast<std::size_t>(picture)] = std::move(art);
    }
    out = std::move(read);
    return true;
}

std::size_t Index(Widget widget) {
    return static_cast<std::size_t>(widget) - 1u; // None is 0
}

Hud::Rect Shrunk(const Hud::Rect& box, double inset) {
    const glm::dvec2 by = box.size * inset;
    return Hud::Rect{box.min + by, box.size - by * 2.0};
}

} // namespace

const char* NameOf(Widget widget) {
    switch (widget) {
    case Widget::None:
        return "none";
    case Widget::SizeMinus:
        return "size_minus";
    case Widget::SizePlus:
        return "size_plus";
    case Widget::OpacityMinus:
        return "opacity_minus";
    case Widget::OpacityPlus:
        return "opacity_plus";
    case Widget::Lock:
        return "lock";
    case Widget::Restore:
        return "restore";
    case Widget::Back:
        return "back";
    }
    return "?";
}

const char* KeyOf(Picture picture) {
    switch (picture) {
    case Picture::SizeMinus:
        return "size_minus";
    case Picture::SizePlus:
        return "size_plus";
    case Picture::OpacityMinus:
        return "opacity_minus";
    case Picture::OpacityPlus:
        return "opacity_plus";
    case Picture::LockLocked:
        return "lock_locked";
    case Picture::LockUnlocked:
        return "lock_unlocked";
    case Picture::Restore:
        return "restore";
    case Picture::Back:
        return "back";
    }
    return "?";
}

std::optional<Picture> PictureOfKey(const std::string& key) {
    for (const Picture picture : kPictures) {
        if (key == KeyOf(picture)) return picture;
    }
    return std::nullopt;
}

std::optional<Picture> PictureOf(Widget widget, bool unlocked) {
    switch (widget) {
    case Widget::None:
        return std::nullopt;
    case Widget::SizeMinus:
        return Picture::SizeMinus;
    case Widget::SizePlus:
        return Picture::SizePlus;
    case Widget::OpacityMinus:
        return Picture::OpacityMinus;
    case Widget::OpacityPlus:
        return Picture::OpacityPlus;
    case Widget::Lock:
        return unlocked ? Picture::LockUnlocked : Picture::LockLocked;
    case Widget::Restore:
        return Picture::Restore;
    case Widget::Back:
        return Picture::Back;
    }
    return std::nullopt;
}

bool LoadRules(const std::string& path, const Controls::Rules& controls, const Options::Rules& options, Rules& out, std::string& error) {
    Json::Value root;
    if (!UiLayer::Read::File(path, root, error)) return false;
    const Json::Value& block = root[kBlock];
    if (!block.IsObject()) {
        error = path + ": " + kBlock + " is missing or not an object";
        return false;
    }
    Rules read;
    std::string why;
    const auto fail = [&error, &path, &why]() {
        error = path + ": " + why;
        return false;
    };
    const std::string where = kBlock;

    if (!ReadColour(block, "backdrop_rgba", read.backdrop, why, where)) return fail();

    if (!Object(block, "rows", why, where)) return fail();
    {
        const Json::Value& rows = block["rows"];
        const std::string at = Named(where, "rows");
        if (!Number(rows, "size", 0.0, 1.0, read.rowSize, why, at) || !Number(rows, "opacity", 0.0, 1.0, read.rowOpacity, why, at) ||
            !Number(rows, "lock", 0.0, 1.0, read.rowLock, why, at)) {
            return fail();
        }
    }
    if (!ReadTile(block, "tile", read.tile, why, where) || !ReadTile(block, "lock", read.lock, why, where)) return fail();

    if (!Object(block, "back", why, where)) return fail();
    {
        const Json::Value& back = block["back"];
        const std::string at = Named(where, "back");
        if (!Positive(back, "box_px", read.back.boxPx, why, at) || !Number(back, "face_inset", 0.0, 0.25, read.back.faceInset, why, at) ||
            !Number(back, "gap_below_clear_px", 0.0, 1.0e4, read.back.gapBelowClearPx, why, at)) {
            return fail();
        }
    }

    if (!Object(block, "readout", why, where)) return fail();
    {
        const Json::Value& readout = block["readout"];
        const std::string at = Named(where, "readout");
        if (!Text(readout, "font", read.readout.font, why, at) || !Positive(readout, "units_per_font_px", read.readout.unitsPerFontPx, why, at) ||
            !Byte(readout, "alpha_byte", read.readout.alphaByte, why, at)) {
            return fail();
        }
    }

    if (!Object(block, "inert", why, where) || !Byte(block["inert"], "alpha_byte", read.inertAlphaByte, why, Named(where, "inert"))) return fail();

    if (!Object(block, "flat", why, where)) return fail();
    {
        const Json::Value& flat = block["flat"];
        const std::string at = Named(where, "flat");
        if (!ReadColour(flat, "tile_rgba", read.flatTile, why, at) || !ReadColour(flat, "unlocked_rgba", read.flatUnlocked, why, at)) return fail();
    }

    if (!Object(block, "unlocked", why, where)) return fail();
    {
        const Json::Value& unlocked = block["unlocked"];
        const std::string at = Named(where, "unlocked");
        Rules::Unlocked& u = read.unlocked;
        if (!Positive(unlocked, "outline_thickness_units", u.outlineThicknessUnits, why, at) ||
            !Byte(unlocked, "outline_alpha_byte", u.outlineAlphaByte, why, at) || !Byte(unlocked, "outline_swing_byte", u.outlineSwingByte, why, at) ||
            !Whole(unlocked, "outline_period_ticks", u.outlinePeriodTicks, why, at) || !ReadRgb(unlocked, "outline_rgb", u.outlineRgb, why, at) ||
            !Positive(unlocked, "held_outline_thickness_units", u.heldOutlineThicknessUnits, why, at) ||
            !Byte(unlocked, "held_outline_alpha_byte", u.heldOutlineAlphaByte, why, at) ||
            !Byte(unlocked, "hit_frame_alpha_byte", u.hitFrameAlphaByte, why, at) ||
            !Positive(unlocked, "hit_frame_thickness_units", u.hitFrameThicknessUnits, why, at) ||
            !ReadRgb(unlocked, "hit_frame_rgb", u.hitFrameRgb, why, at) ||
            !Positive(unlocked, "guide_thickness_units", u.guideThicknessUnits, why, at) || !ReadColour(unlocked, "guide_rgba", u.guide, why, at)) {
            return fail();
        }
        // The breathing stays within a byte at both ends, so a triangle never wraps.
        if (u.outlineAlphaByte - u.outlineSwingByte < 0 || u.outlineAlphaByte + u.outlineSwingByte > 255) {
            why = Named(at, "outline_swing_byte") + " takes outline_alpha_byte outside 0 to 255";
            return fail();
        }
    }

    if (!Object(block, "drag", why, where)) return fail();
    {
        // A step under twice the tenth a move is kept to would be lost in the rounding (the walk would stand still); and a step over 8 units could
        // be longer than the overlap of the smallest box (18.2 units at size 0.4) with an obstacle's grown one, and a neighbour stepped over.
        const std::string at = Named(where, "drag");
        if (!Number(block["drag"], "step_units", 2.0 * Controls::Tuning::kMoveSnap, 8.0, read.drag.stepUnits, why, at)) return fail();
    }

    if (!Object(block, "art", why, where)) return fail();
    {
        const Json::Value& art = block["art"];
        const std::string at = Named(where, "art");
        if (!ReadArt(art, "mr", read.mr, why, at) || !ReadArt(art, "generated", read.generated, why, at)) return fail();
        // The generated tier WRITES its files into one folder with the five controls' and the Options gear: a picture of this block shares
        // a name with none of them.
        for (const Picture picture : kPictures) {
            const std::string& file = read.generated[static_cast<std::size_t>(picture)]->file;
            const std::string here = Named(Named(at, "generated"), KeyOf(picture));
            if (Lower(file) == Lower(options.entry.sprite)) {
                why = here + ".file names the same file as options.entry.file: each generated picture is a file of its own";
                return fail();
            }
            for (const Controls::Id id : Controls::kClaimOrder) {
                const auto& generated = controls.art[static_cast<std::size_t>(Controls::ArtTier::Generated)][static_cast<std::size_t>(id)];
                if (generated && Lower(generated->file) == Lower(file)) {
                    why = here + ".file names the same file as art.generated." + Controls::KeyOf(id) + ".file: each generated picture is a file of its own";
                    return fail();
                }
            }
        }
    }

    out = std::move(read);
    return true;
}

// ---- the layout -----------------------------------------------------------------------------------------------

const Tile& Layout::At(Widget widget) const {
    return tiles[Index(widget)];
}

Hud::Rect BackBox(const Rules& rules, const Controls::Rules& controls, const glm::dvec2& viewUnits, const Controls::Insets& insets) {
    const double toUnits = viewUnits.y / controls.referenceHeightPx;
    const double box = rules.back.boxPx * toUnits;
    // The largest a corner box ever is (size 1.4), so that Back clears it at every size and never moves as the size is stepped.
    const double corner = controls.cornerBoxFactor * controls.boxPx * std::max(controls.size.max, controls.cornerMinSize) * toUnits;
    const double left = insets.left + controls.marginSidePx * toUnits;
    const double top = insets.top + controls.marginTopPx * toUnits + corner + rules.back.gapBelowClearPx * toUnits;
    return Hud::Rect{glm::dvec2(left, top), glm::dvec2(box)};
}

Layout LayOut(const Rules& rules, const Controls::Rules& controls, const glm::dvec2& viewUnits, const Controls::Insets& insets) {
    Layout layout;
    layout.view = viewUnits;
    const double toUnits = viewUnits.y / controls.referenceHeightPx;
    const double middle = viewUnits.x * 0.5;

    const auto place = [&](Widget widget, const TileSize& size, double side, double row) {
        const double box = size.boxPx * toUnits;
        const glm::dvec2 centre(middle + side * size.offsetPx * toUnits, row * viewUnits.y);
        Tile tile;
        tile.widget = widget;
        tile.box = Hud::Rect{centre - glm::dvec2(box * 0.5), glm::dvec2(box)};
        tile.faceInset = size.faceInset;
        tile.face = Shrunk(tile.box, size.faceInset);
        layout.tiles[Index(widget)] = tile;
    };
    place(Widget::SizeMinus, rules.tile, -1.0, rules.rowSize);
    place(Widget::SizePlus, rules.tile, 1.0, rules.rowSize);
    place(Widget::OpacityMinus, rules.tile, -1.0, rules.rowOpacity);
    place(Widget::OpacityPlus, rules.tile, 1.0, rules.rowOpacity);
    place(Widget::Lock, rules.lock, -1.0, rules.rowLock);
    place(Widget::Restore, rules.lock, 1.0, rules.rowLock);

    Tile back;
    back.widget = Widget::Back;
    back.box = BackBox(rules, controls, viewUnits, insets);
    back.faceInset = rules.back.faceInset;
    back.face = Shrunk(back.box, rules.back.faceInset);
    layout.tiles[Index(Widget::Back)] = back;

    glm::dvec2 low(layout.At(Widget::SizeMinus).box.min);
    glm::dvec2 high(layout.At(Widget::SizeMinus).box.Max());
    for (const Widget widget : kWidgets) {
        if (widget == Widget::Back) continue;
        const Hud::Rect& box = layout.At(widget).box;
        low = glm::min(low, box.min);
        high = glm::max(high, box.Max());
    }
    layout.column = Hud::Rect{low, high - low};
    return layout;
}

bool StrictlyInside(const Hud::Rect& rect, const glm::dvec2& point) {
    const glm::dvec2 max = rect.Max();
    return point.x > rect.min.x && point.x < max.x && point.y > rect.min.y && point.y < max.y;
}

Widget WidgetAt(const Layout& layout, const glm::dvec2& point) {
    for (const Widget widget : kWidgets) {
        if (StrictlyInside(layout.At(widget).face, point)) return widget;
    }
    return Widget::None;
}

// ---- the touch -------------------------------------------------------------------------------------------------

bool StaysDead(const Touch& touch, bool pressed, bool touching) {
    return touch.dead && !pressed && touching;
}

Press Follow(Touch& touch, bool pressed, bool released, bool touching, Widget on) {
    if (touch.dead) {
        // The press that opened the screen, if it is still down, belongs to the screen before: nothing it does here counts. A new press
        // or a finger that is up ends it, and the tick goes on as any other.
        if (!StaysDead(touch, pressed, touching)) {
            touch.dead = false;
        } else {
            touch.down = Widget::None;
            touch.held = Widget::None;
            return Press{};
        }
    }
    if (pressed) touch.down = on;
    touch.held = touch.down != Widget::None && touching && on == touch.down ? touch.down : Widget::None;
    if (!released) return Press{};
    const Press press{touch.down != Widget::None && on == touch.down, touch.down};
    touch.down = Widget::None;
    touch.held = Widget::None;
    return press;
}

// ---- what a press does -----------------------------------------------------------------------------------

bool HasMove(const Controls::Tuning& tuning) {
    for (const glm::dvec2& move : tuning.move) {
        if (move.x != 0.0 || move.y != 0.0) return true;
    }
    return false;
}

bool SameKept(const Controls::Tuning& a, const Controls::Tuning& b) {
    if (a.sizeSteps != b.sizeSteps || a.opacitySteps != b.opacitySteps) return false;
    for (std::size_t i = 0; i < a.move.size(); ++i) {
        if (a.move[i].x != b.move[i].x || a.move[i].y != b.move[i].y) return false;
    }
    return true;
}

bool Enabled(Widget widget, const Controls::Rules& rules, const Controls::Tuning& tuning) {
    switch (widget) {
    case Widget::SizeMinus:
        return tuning.SizeStepsIn(rules) > rules.size.lowSteps;
    case Widget::SizePlus:
        return tuning.SizeStepsIn(rules) < rules.size.highSteps;
    case Widget::OpacityMinus:
        return tuning.OpacityStepsIn(rules) > rules.opacity.lowSteps;
    case Widget::OpacityPlus:
        return tuning.OpacityStepsIn(rules) < rules.opacity.highSteps;
    case Widget::Restore:
        return HasMove(tuning);
    case Widget::Lock:
    case Widget::Back:
        return true;
    case Widget::None:
        return false;
    }
    return false;
}

Outcome Act(Widget widget, const Controls::Rules& rules, const Controls::Tuning& tuning, bool unlocked, const Field* field) {
    Outcome outcome;
    outcome.tuning = tuning;
    outcome.unlocked = unlocked;
    if (widget == Widget::None || !Enabled(widget, rules, tuning)) return outcome;
    switch (widget) {
    case Widget::SizeMinus:
    case Widget::SizePlus: {
        // One grid step, and a size step RE-LOCKS (Magic Rampage l.79): the controls under a drag would be the wrong size.
        outcome.tuning = tuning.SizeStepped(rules, widget == Widget::SizePlus ? 1 : -1);
        // The boxes are bigger or smaller: the ones that now meet go home (Magic Rampage pushes them apart every frame; this is the same guarantee
        // with no state and no animation).
        if (field != nullptr) outcome.tuning = SendHome(rules, outcome.tuning, *field);
        outcome.unlocked = false;
        break;
    }
    case Widget::OpacityMinus:
    case Widget::OpacityPlus:
        // An opacity step does NOT re-lock (Magic Rampage l.92).
        outcome.tuning = tuning.OpacityStepped(rules, widget == Widget::OpacityPlus ? 1 : -1);
        break;
    case Widget::Lock:
        outcome.unlocked = !unlocked;
        outcome.acted = true;
        return outcome;
    case Widget::Restore:
        // Every move cleared, the size and the opacity kept, re-locked.
        outcome.tuning.move = {};
        outcome.tuning = outcome.tuning.Clamped(rules);
        outcome.unlocked = false;
        break;
    case Widget::Back:
        outcome.leave = true;
        outcome.acted = true;
        return outcome;
    case Widget::None:
        return outcome;
    }
    outcome.acted = true;
    outcome.tuningChanged = !SameKept(outcome.tuning, tuning);
    return outcome;
}

// ---- where a control may stand ---------------------------------------------------------------------------------

double ClearanceUnits(const Controls::Rules& rules, const glm::dvec2& viewUnits) {
    return rules.clearancePx * viewUnits.y / rules.referenceHeightPx;
}

double OverlapArea(const Hud::Rect& a, const Hud::Rect& b) {
    const glm::dvec2 amax = a.Max();
    const glm::dvec2 bmax = b.Max();
    const double ox = std::min(amax.x, bmax.x) - std::max(a.min.x, b.min.x);
    const double oy = std::min(amax.y, bmax.y) - std::max(a.min.y, b.min.y);
    return ox > 0.0 && oy > 0.0 ? ox * oy : 0.0;
}

namespace {

Hud::Rect Grown(const Hud::Rect& rect, double by) {
    return Hud::Rect{rect.min - glm::dvec2(by), rect.size + glm::dvec2(2.0 * by)};
}

std::size_t Idx(Controls::Id id) {
    return static_cast<std::size_t>(id);
}

} // namespace

namespace {

// The distance between two intervals along one axis: positive when they are apart, negative when they overlap. The same four numbers whichever
// interval is asked first, so the answer is the same both ways to the last bit.
double Separation(double aMin, double aMax, double bMin, double bMax) {
    return std::max(aMin - bMax, bMin - aMax);
}

// A gap that is the clearance to the last digit is CLEAR. It happens: at size 1.0 an arrow is 128 px and the clearance 16 px, 144 px is 51.2 units - a multiple
// of the tenth a move is kept to - so two arrows can stand exactly that far apart, and a comparison of two different roundings of that number must not call
// it a meeting in one direction and not in the other.
constexpr double kMeetEpsilon = 1.0e-9;

} // namespace

bool Meets(const Hud::Rect& a, const Hud::Rect& b, double clearance) {
    const glm::dvec2 amax = a.Max();
    const glm::dvec2 bmax = b.Max();
    return Separation(a.min.x, amax.x, b.min.x, bmax.x) < clearance - kMeetEpsilon && Separation(a.min.y, amax.y, b.min.y, bmax.y) < clearance - kMeetEpsilon;
}

bool AnyMeet(const std::array<Hud::Rect, 5>& boxes, double clearance) {
    for (std::size_t i = 0; i < boxes.size(); ++i) {
        for (std::size_t j = i + 1; j < boxes.size(); ++j) {
            if (Meets(boxes[i], boxes[j], clearance)) return true;
        }
    }
    return false;
}

Controls::Tuning SendHome(const Controls::Rules& rules, const Controls::Tuning& stepped, const Field& field) {
    const double clearance = ClearanceUnits(rules, field.view);
    const std::array<Hud::Rect, 5> boxes = Controls::Boxes(rules, stepped, field.view, field.insets);
    Controls::Tuning out = stepped;
    // Both of a meeting pair: a control that meets another is flagged, and so is the other, because each meets one.
    for (const Controls::Id id : Controls::kClaimOrder) {
        for (const Controls::Id other : Controls::kClaimOrder) {
            if (other == id || !Meets(boxes[Idx(id)], boxes[Idx(other)], clearance)) continue;
            out.move[Idx(id)] = glm::dvec2(0.0);
            break;
        }
    }
    // A control sent home may now stand where a third, which met nothing, was moved to.
    if (AnyMeet(Controls::Boxes(rules, out, field.view, field.insets), clearance)) out.move = {};
    return out;
}

// ---- the drag --------------------------------------------------------------------------------------------------

namespace {

// What a dragged box may not meet: a rectangle and the clearance to keep from it - another control's box and the layout's 16 px, or a widget's box and none.
struct Obstacle {
    Hud::Rect rect;
    double clearance = 0.0;
};

// How deep a box is in an obstacle: the area it shares with the obstacle grown by its clearance (0 where it does not meet it).
double Depth(const Hud::Rect& box, const Obstacle& obstacle) {
    return OverlapArea(box, Grown(obstacle.rect, obstacle.clearance));
}

// Whether a step to `candidate` may be taken from `current`: it meets no obstacle - or, for one the control already meets, it meets it LESS than it
// did (so a control that starts inside an obstacle can leave it, and none can go deeper in). The test is Meets, the one the five boxes are held to
// everywhere (SendHome, AnyMeet), so what the walk accepts is what they accept.
bool Accepts(const Hud::Rect& candidate, const Hud::Rect& current, const std::vector<Obstacle>& obstacles) {
    for (const Obstacle& obstacle : obstacles) {
        if (!Meets(candidate, obstacle.rect, obstacle.clearance)) continue;
        if (Meets(current, obstacle.rect, obstacle.clearance) && Depth(candidate, obstacle) < Depth(current, obstacle)) continue;
        return false;
    }
    return true;
}

} // namespace

Step Drag::End(const Context& context) {
    Step out;
    out.tuning = context.tuning;
    if (!m_grab) return out;
    out.tuning = m_live;
    // One gesture, one keeping: only a control that ended somewhere else is to be kept (a drag that came back to where it began is not).
    out.commit = !SameKept(m_live, m_base);
    m_grab.reset();
    m_offset = glm::dvec2(0.0);
    return out;
}

void Drag::Cancel() {
    m_use = Use::None;
    m_grab.reset();
    m_offset = glm::dvec2(0.0);
    m_base = Controls::Tuning{};
    m_live = Controls::Tuning{};
}

void Drag::Walk(const Pointer& pointer, const Context& context, Step& out) {
    const Controls::Id id = *m_grab;
    const Field& field = context.field;
    const double stepUnits = context.rules.drag.stepUnits;
    const double clearance = ClearanceUnits(context.controls, field.view);

    const std::array<Hud::Rect, 5> boxes = Controls::Boxes(context.controls, m_live, field.view, field.insets);
    Hud::Rect current = boxes[Idx(id)];

    // What the box may not meet: the other four controls' boxes grown by the clearance, and the seven widgets' boxes (Back included).
    std::vector<Obstacle> obstacles;
    for (const Controls::Id other : Controls::kClaimOrder) {
        if (other != id) obstacles.push_back(Obstacle{boxes[Idx(other)], clearance});
    }
    for (const Widget widget : kWidgets) obstacles.push_back(Obstacle{context.widgets.At(widget).box, 0.0});

    // Where the pointer wants it: the pointer minus the offset it was grabbed with, inside the safe rectangle by the ONE shared clamp.
    const glm::dvec2 target = Controls::ClampBox(pointer.at - m_offset, current.size, field.view, field.insets);

    // One try at standing with `axis` at `value` (the other as it is now). Validated on the LAID-OUT result: the move the placement function gives back
    // (clamped, then kept to a tenth) and the box the layout makes of it - not on the raw wish, which rounding and the clamp can move into a neighbour.
    enum class Try { Same, Refused, Taken };
    const auto attempt = [&](int axis, double value) {
        glm::dvec2 wanted = current.min;
        wanted[axis] = value;
        Controls::Tuning next = m_live;
        next.move[Idx(id)] = Controls::MoveFor(context.controls, m_live, id, wanted, field.view, field.insets);
        const Hud::Rect box = Controls::Boxes(context.controls, next, field.view, field.insets)[Idx(id)];
        if (std::fabs(box.min.x - current.min.x) < 1e-9 && std::fabs(box.min.y - current.min.y) < 1e-9) return Try::Same; // rounds to where it stands
        if (!Accepts(box, current, obstacles)) return Try::Refused;
        m_live = next;
        current = box;
        return Try::Taken;
    };

    const Controls::Tuning before = m_live;
    for (int axis = 0; axis < 2; ++axis) {
        const double from = current.min[axis];
        const double to = target[axis];
        const double distance = std::fabs(to - from);
        if (!(distance > 0.0)) continue;
        const double sign = to > from ? 1.0 : -1.0;
        const long steps = std::min(static_cast<long>(std::ceil(distance / stepUnits)), 1L << 20);
        double last = from;
        for (long k = 1; k <= steps; ++k) {
            // Steps of at most stepUnits, the last landing on the target: every place between here and there is tried, so a neighbour is never jumped.
            const double value = k == steps ? to : from + sign * stepUnits * static_cast<double>(k);
            if (attempt(axis, value) == Try::Refused) {
                // The obstacle lies between the last place taken and this one: bisect to the farthest place a tenth of a unit lets (the same place
                // whatever the length of the steps and the pace of the pointer that got here, so 1 unit a tick and 40 end alike).
                double low = last;
                double high = value;
                for (int round = 0; round < 40 && std::fabs(high - low) > 0.25 * Controls::Tuning::kMoveSnap; ++round) {
                    const double middle = 0.5 * (low + high);
                    if (attempt(axis, middle) == Try::Taken) {
                        low = middle;
                    } else {
                        high = middle;
                    }
                }
                break;
            }
            last = value;
        }
    }
    out.moved = !SameKept(m_live, before);
}

Step Drag::Tick(const Pointer& pointer, bool dead, const Context& context) {
    Step out;
    out.tuning = m_grab ? m_live : context.tuning;
    if (dead) {
        // The press that opened the screen, still down: it belongs to the screen before it and grabs nothing, wherever it slides.
        m_use = Use::Dead;
        out.use = Use::Dead;
        return out;
    }
    const bool finger = pointer.pressed || pointer.touching || pointer.released;
    if (!finger) {
        // No finger this tick: a grab that is still held lost it (the pointer left the window, which the layer sees as no touch at all) and ends
        // where it stands.
        out = End(context);
        m_use = Use::None;
        return out;
    }
    if (pointer.pressed) {
        if (m_grab) {
            // A press with a grab still held: its release was never seen. It ends here and this press takes nothing, as a finger that lands as
            // another lifts is not read twice.
            out = End(context);
            m_use = Use::None;
            return out;
        }
        m_use = Use::None;
        if (pointer.over && WidgetAt(context.widgets, pointer.at) != Widget::None) {
            // A widget wins where it covers a control: its face is a tile's, the control's uncovered part is all a drag can take.
            m_use = Use::Widget;
        } else if (pointer.over && context.unlocked) {
            const std::array<Hud::Rect, 5> boxes = Controls::Boxes(context.controls, context.tuning, context.field.view, context.field.insets);
            // The DRAWN box, not the hit rectangle (which reaches the edge of the view), and the topmost: the preview draws the claim order's last last.
            for (auto id = Controls::kClaimOrder.rbegin(); id != Controls::kClaimOrder.rend(); ++id) {
                if (!StrictlyInside(boxes[Idx(*id)], pointer.at)) continue;
                m_use = Use::Control;
                m_grab = *id;
                m_offset = pointer.at - boxes[Idx(*id)].min;
                m_base = context.tuning;
                m_live = context.tuning;
                break;
            }
        }
    }
    out.use = m_use;
    if (!m_grab) return out;
    if (!context.unlocked) {
        // The lock closed under a grab (it cannot with one pointer: a tile is its own touch): the grab is dropped, nothing kept.
        out.tuning = context.tuning;
        Cancel();
        return out;
    }
    Walk(pointer, context, out);
    out.tuning = m_live;
    out.grabbed = m_grab;
    if (pointer.released || !pointer.touching) {
        const bool moved = out.moved;
        out = End(context);
        out.moved = moved;
        out.use = Use::Control;
        m_use = Use::None;
    }
    return out;
}

// ---- the readouts ----------------------------------------------------------------------------------------------

std::string ReadoutText(const Controls::Rules& rules, const Controls::Tuning& tuning, Row row) {
    const double value = row == Row::Size ? tuning.Size(rules) : tuning.Opacity(rules);
    char text[32];
    std::snprintf(text, sizeof(text), "%.1f", value);
    return text;
}

// ---- the pictures ----------------------------------------------------------------------------------------------

std::array<Hud::Rect, 4> FrameStrips(const Hud::Rect& rect, double thickness, bool outside) {
    // Outside: the frame's inner edge is the rectangle's own. Inside: its outer edge is.
    const Hud::Rect outer = outside ? Hud::Rect{rect.min - glm::dvec2(thickness), rect.size + glm::dvec2(2.0 * thickness)} : rect;
    const glm::dvec2 max = outer.Max();
    const double inner = std::max(0.0, outer.size.y - 2.0 * thickness);
    return {Hud::Rect{outer.min, glm::dvec2(outer.size.x, thickness)},                                  // top
            Hud::Rect{glm::dvec2(outer.min.x, max.y - thickness), glm::dvec2(outer.size.x, thickness)}, // bottom
            Hud::Rect{glm::dvec2(outer.min.x, outer.min.y + thickness), glm::dvec2(thickness, inner)},   // left
            Hud::Rect{glm::dvec2(max.x - thickness, outer.min.y + thickness), glm::dvec2(thickness, inner)}}; // right
}

int OutlineAlphaByte(const Rules::Unlocked& unlocked, int ticks) {
    const int period = std::max(unlocked.outlinePeriodTicks, 1);
    const int phase = ((ticks % period) + period) % period;
    // A triangle in whole quarters: -1 at the start of the breath, +1 half way, back to -1. Integer arithmetic, so the same on every machine.
    const double fraction = static_cast<double>(phase) / static_cast<double>(period);
    const double wave = fraction < 0.5 ? 4.0 * fraction - 1.0 : 3.0 - 4.0 * fraction;
    return static_cast<int>(std::lround(static_cast<double>(unlocked.outlineAlphaByte) + static_cast<double>(unlocked.outlineSwingByte) * wave));
}

double PreviewAlpha(Controls::Id id, const Controls::Rules& controls, const Hud::Rules& hud, const Controls::Tuning& tuning) {
    if (Controls::IsWalk(id)) {
        // A walk arrow, settled and not pulsing: the HUD's own byte through the idle scale and the opacity in force, a whole byte.
        return static_cast<double>(Controls::AlphaByte(controls, tuning, hud.alphaByte)) / 255.0;
    }
    // A corner button's alpha is a fraction (its entrance is a ramp): the HUD's opacity times the same multiplier, capped.
    return std::min(1.0, Hud::Opacity(hud) * controls.idleScale * tuning.Opacity(controls));
}

std::vector<Piece> Pieces(const Rules& rules, const Controls::Rules& controls, const Hud::Rules& hud, const Controls::Tuning& tuning,
                          const glm::dvec2& viewUnits, const Controls::Insets& insets, const Scene& scene) {
    std::vector<Piece> pieces;
    const auto quad = [&pieces](Element element, const Hud::Rect& rect, const Colour& colour) {
        Piece piece;
        piece.element = element;
        piece.rect = rect;
        piece.colour = colour;
        pieces.push_back(std::move(piece));
    };

    quad(Element::Backdrop, Hud::Rect{glm::dvec2(0.0), viewUnits}, rules.backdrop);

    // THE FIVE CONTROLS, from the layout the level plays: the same boxes and the same hit rectangles, settled. Restart, pause and clear are
    // always there (clear forced shown by the scene's flag: in a level it is shown only while a portal lives), whatever the last level had.
    const Controls::Layout layout = Controls::LayOut(Controls::Style::Modern, controls, hud, tuning, viewUnits, insets, kSettledMs);
    for (const Controls::Id id : Controls::kClaimOrder) {
        if (id == Controls::Id::Clear && !scene.previewClearShown) continue;
        Piece piece;
        piece.element = Element::Preview;
        piece.rect = layout.At(id).rect;
        piece.alpha = PreviewAlpha(id, controls, hud, tuning);
        piece.control = id;
        piece.faceInset = controls.faceInset;
        pieces.push_back(std::move(piece));
    }

    if (scene.unlocked) {
        const Rules::Unlocked& u = rules.unlocked;
        // Each control's hit rectangle, as LayOut has it: what a tap on the world cannot be, and what a bigger arrow will cover.
        const Colour hitColour{u.hitFrameRgb[0], u.hitFrameRgb[1], u.hitFrameRgb[2], u.hitFrameAlphaByte};
        const Colour outlineColour{u.outlineRgb[0], u.outlineRgb[1], u.outlineRgb[2], OutlineAlphaByte(u, scene.ticks)};
        for (const Controls::Id id : Controls::kClaimOrder) {
            if (id == Controls::Id::Clear && !scene.previewClearShown) continue;
            const Controls::Frame& frame = layout.At(id);
            const Hud::Rect hit = frame.hit.kind == Controls::HitShape::Kind::Rect ? frame.hit.rect : frame.rect;
            for (const Hud::Rect& strip : FrameStrips(hit, u.hitFrameThicknessUnits, false)) {
                Piece piece;
                piece.element = Element::HitFrame;
                piece.rect = strip;
                piece.colour = hitColour;
                piece.control = id;
                pieces.push_back(std::move(piece));
            }
        }
        for (const Controls::Id id : Controls::kClaimOrder) {
            if (id == Controls::Id::Clear && !scene.previewClearShown) continue;
            // The control a finger holds has the held outline: thicker, solid, not breathing (design 3.3.2).
            const bool held = scene.grabbed && *scene.grabbed == id;
            const double thickness = held ? u.heldOutlineThicknessUnits : u.outlineThicknessUnits;
            const Colour colour = held ? Colour{u.outlineRgb[0], u.outlineRgb[1], u.outlineRgb[2], u.heldOutlineAlphaByte} : outlineColour;
            for (const Hud::Rect& strip : FrameStrips(layout.At(id).rect, thickness, true)) {
                Piece piece;
                piece.element = Element::Outline;
                piece.rect = strip;
                piece.colour = colour;
                piece.control = id;
                pieces.push_back(std::move(piece));
            }
        }
        // The character's row: the one source of that number is the census block.
        const double row = controls.census.characterRowUnits;
        quad(Element::Guide, Hud::Rect{glm::dvec2(0.0, row - u.guideThicknessUnits * 0.5), glm::dvec2(viewUnits.x, u.guideThicknessUnits)}, u.guide);
    }

    // THE SEVEN WIDGETS, over whatever the preview put under them, and the two readouts.
    const Layout column = LayOut(rules, controls, viewUnits, insets);
    for (const Widget widget : kWidgets) {
        const Tile& tile = column.At(widget);
        const bool held = scene.held == widget;
        const bool enabled = Enabled(widget, controls, tuning);
        const std::optional<Picture> picture = PictureOf(widget, scene.unlocked);
        Piece piece;
        piece.element = Element::Tile;
        piece.rect = tile.box;
        piece.widget = widget;
        piece.picture = picture.value_or(Picture::Back);
        piece.faceInset = tile.faceInset;
        piece.alpha = enabled ? 1.0 : static_cast<double>(rules.inertAlphaByte) / 255.0;
        piece.tintByte = held ? scene.pressTintByte : 255;
        piece.colour = widget == Widget::Lock && scene.unlocked ? rules.flatUnlocked : rules.flatTile;
        for (int i = 0; i < 3; ++i) piece.colour[static_cast<std::size_t>(i)] = piece.colour[static_cast<std::size_t>(i)] * piece.tintByte / 255;
        pieces.push_back(std::move(piece));
    }
    for (const Row row : {Row::Size, Row::Opacity}) {
        Piece piece;
        piece.element = Element::Readout;
        piece.rect = Hud::Rect{glm::dvec2(viewUnits.x * 0.5, (row == Row::Size ? rules.rowSize : rules.rowOpacity) * viewUnits.y), glm::dvec2(0.0)};
        piece.colour = Colour{255, 255, 255, rules.readout.alphaByte};
        piece.words = ReadoutText(controls, tuning, row);
        piece.font = rules.readout.font;
        piece.unitsPerFontPx = rules.readout.unitsPerFontPx;
        pieces.push_back(std::move(piece));
    }
    return pieces;
}

} // namespace MagicPortals::Adjust

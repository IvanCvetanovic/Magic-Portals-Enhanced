#include "sim/Picker.hpp"

#include "core/Json.hpp"
#include "sim/Controls.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace MagicPortals::Picker {

namespace {

namespace Json = Supersonic::Json;
using UiLayer::Read::Byte;
using UiLayer::Read::Fraction;
using UiLayer::Read::Pair;
using UiLayer::Read::Positive;
using UiLayer::Read::Size;
using UiLayer::Read::Text;

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

bool Number(const Json::Value& block, const char* key, double lo, double hi, double& out, std::string& why, const std::string& where) {
    const Json::Value& value = block[key];
    if (!value.IsNumber() || !std::isfinite(value.AsNumber()) || value.AsNumber() < lo || value.AsNumber() > hi) {
        why = Named(where, key) + " is missing, not a number, or outside " + std::to_string(lo) + " to " + std::to_string(hi);
        return false;
    }
    out = value.AsNumber();
    return true;
}

bool Whole(const Json::Value& block, const char* key, int lo, int hi, int& out, std::string& why, const std::string& where) {
    double value = 0.0;
    if (!Number(block, key, static_cast<double>(lo), static_cast<double>(hi), value, why, where)) return false;
    if (value != std::floor(value)) {
        why = Named(where, key) + " is not a whole number";
        return false;
    }
    out = static_cast<int>(value);
    return true;
}

// [r, g, b, a], four whole numbers, each a byte.
bool ReadColour(const Json::Value& block, const char* key, Colour& out, std::string& why, const std::string& where) {
    const Json::Value& value = block[key];
    if (!value.IsArray() || value.AsArray().size() != 4) {
        why = Named(where, key) + " is missing or not four numbers (r, g, b, a)";
        return false;
    }
    Colour read{};
    for (std::size_t i = 0; i < 4; ++i) {
        const Json::Value& channel = value.AsArray()[i];
        if (!channel.IsNumber() || !std::isfinite(channel.AsNumber()) || channel.AsNumber() < 0.0 || channel.AsNumber() > 255.0 ||
            channel.AsNumber() != std::floor(channel.AsNumber())) {
            why = Named(where, key) + " holds something that is not a whole number from 0 to 255";
            return false;
        }
        read[i] = static_cast<int>(channel.AsNumber());
    }
    out = read;
    return true;
}

// at_screen, origin (fractions), offset_units (any pair) and size_units (a size).
bool ReadSpot(const Json::Value& block, Rules::Spot& out, std::string& why, const std::string& where) {
    Rules::Spot read;
    if (!Fraction(block, "at_screen", read.atScreen, why, where) || !Fraction(block, "origin", read.origin, why, where) ||
        !Pair(block, "offset_units", read.offsetUnits, why, where) || !Size(block, "size_units", read.sizeUnits, why, where)) {
        return false;
    }
    out = read;
    return true;
}

// A stem is a bare name: letters, digits and underscores, so that the file the layer writes stays inside its own folder and names no picture.
bool IsStem(const std::string& stem) {
    if (stem.empty() || stem.size() > 64) return false;
    return std::all_of(stem.begin(), stem.end(), [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
    });
}

Hud::Rect SpotRect(const Rules::Spot& spot, const glm::dvec2& anchor) {
    return Hud::Rect{anchor - spot.sizeUnits * spot.origin, spot.sizeUnits};
}

glm::dvec2 SpotAnchor(const Rules::Spot& spot, const glm::dvec2& viewUnits) {
    return spot.atScreen * viewUnits + spot.offsetUnits;
}

Colour Tinted(Colour colour, int tintByte) {
    for (int i = 0; i < 3; ++i) colour[static_cast<std::size_t>(i)] = colour[static_cast<std::size_t>(i)] * tintByte / 255;
    return colour;
}

} // namespace

bool LoadRules(const std::string& path, Rules& out, std::string& error) {
    Json::Value root;
    if (!UiLayer::Read::File(path, root, error)) return false;
    Rules read;
    std::string why;
    const auto fail = [&error, &path, &why]() {
        error = path + ": " + why;
        return false;
    };

    if (!Object(root, "entry", why, "")) return fail();
    {
        const Json::Value& entry = root["entry"];
        if (!ReadSpot(entry, read.entry, why, "entry") || !Text(entry, "glyph", read.entry.glyph, why, "entry") ||
            !Whole(entry, "glyph_px", 16, 1024, read.entry.glyphPx, why, "entry") ||
            !Number(entry, "body_inset", 0.0, Controls::kMaxGeneratedBodyInset, read.entry.bodyInset, why, "entry")) {
            return fail();
        }
        // The file the layer writes is named by this stem and goes into its own scratch folder: a stem with a dot, a slash or a drive in it is refused, and
        // so is one that already says what kind of file it is (a table that names a .png is resolved as an original asset by the package's closure).
        if (!IsStem(read.entry.glyph)) {
            why = "entry.glyph is not a bare stem (letters, digits and underscores, no extension): \"" + read.entry.glyph + "\"";
            return fail();
        }
    }
    if (!Object(root, "corner", why, "") || !ReadSpot(root["corner"], read.corner, why, "corner")) return fail();

    if (!Object(root, "title", why, "")) return fail();
    {
        const Json::Value& title = root["title"];
        if (!Text(title, "font", read.title.font, why, "title") || !Positive(title, "units_per_font_px", read.title.unitsPerFontPx, why, "title") ||
            !Number(title, "y_at_screen", 0.0, 1.0, read.title.yAtScreen, why, "title") || !Byte(title, "alpha_byte", read.title.alphaByte, why, "title") ||
            !UiLayer::Read::MaxWidth(title, read.title.maxWidthUnits, why, "title")) {
            return fail();
        }
    }

    if (!Object(root, "grid", why, "")) return fail();
    {
        const Json::Value& grid = root["grid"];
        if (!Whole(grid, "columns", 1, 11, read.grid.columns, why, "grid") || !Size(grid, "plate_size_units", read.grid.plateSizeUnits, why, "grid") ||
            !Pair(grid, "gap_units", read.grid.gapUnits, why, "grid") || !Number(grid, "top_units", 0.0, 256.0, read.grid.topUnits, why, "grid") ||
            !Number(grid, "clear_left_units", 0.0, 256.0, read.grid.clearLeftUnits, why, "grid")) {
            return fail();
        }
        if (read.grid.gapUnits.x < 0.0 || read.grid.gapUnits.y < 0.0) {
            why = "grid.gap_units is negative";
            return fail();
        }
    }

    if (!Object(root, "plate", why, "")) return fail();
    {
        const Json::Value& plate = root["plate"];
        if (!ReadColour(plate, "idle_frame_rgba", read.plate.idleFrame, why, "plate") || !ReadColour(plate, "chosen_frame_rgba", read.plate.chosenFrame, why, "plate") ||
            !Positive(plate, "word_units_per_font_px", read.plate.wordUnitsPerFontPx, why, "plate") ||
            !Byte(plate, "idle_word_alpha_byte", read.plate.idleWordAlphaByte, why, "plate") ||
            !Byte(plate, "chosen_word_alpha_byte", read.plate.chosenWordAlphaByte, why, "plate") ||
            !UiLayer::Read::MaxWidth(plate, read.plate.maxWidthUnits, why, "plate")) {
            return fail();
        }
    }

    out = std::move(read);
    return true;
}

Entry EntryAt(const Rules& rules, const UiLayer::Rules& layer, const MenuState::Rules& state, const glm::dvec2& viewUnits, double stateMs, bool held) {
    Entry entry;
    const glm::dvec2 at = UiLayer::ButtonAnchorAt(layer, SpotAnchor(rules.entry, viewUnits), viewUnits, stateMs);
    entry.rect = SpotRect(rules.entry, at);
    entry.picture = Controls::DrawRect(entry.rect, 0.0, rules.entry.bodyInset);
    entry.rgbByte = held ? state.pressTintByte : 255;
    entry.alphaByte = UiLayer::ButtonAlphaByte(layer, stateMs);
    return entry;
}

Hud::Rect CornerRect(const Rules& rules, const glm::dvec2& viewUnits) {
    return SpotRect(rules.corner, SpotAnchor(rules.corner, viewUnits));
}

Entry CornerAt(const Rules& rules, const MenuState::Rules& state, const glm::dvec2& viewUnits, bool held) {
    Entry entry;
    entry.rect = CornerRect(rules, viewUnits);
    entry.picture = Controls::DrawRect(entry.rect, 0.0, rules.entry.bodyInset);
    entry.rgbByte = held ? state.pressTintByte : 255;
    entry.alphaByte = 255;
    return entry;
}

Press Follow(Touch& touch, bool pressed, bool released, bool touching, const Target& on) {
    if (pressed) touch.down = on;
    touch.held = touch.down.kind != Target::Kind::None && touching && on == touch.down ? touch.down : Target{};
    if (!released) return Press{};
    Press press;
    press.happened = touch.down.kind != Target::Kind::None && on == touch.down;
    press.target = touch.down;
    touch = Touch{};
    return press;
}

namespace {

int RowsOf(const Rules& rules, std::size_t count) {
    const auto columns = static_cast<std::size_t>(rules.grid.columns);
    return static_cast<int>((count + columns - 1) / columns);
}

double GridWidth(const Rules& rules) {
    const auto columns = static_cast<double>(rules.grid.columns);
    return columns * rules.grid.plateSizeUnits.x + (columns - 1.0) * rules.grid.gapUnits.x;
}

// The grid's left edge: centred on the screen, and no nearer the left edge than the back button allows.
double GridLeft(const Rules& rules, const glm::dvec2& viewUnits) {
    return std::max(viewUnits.x * 0.5 - GridWidth(rules) * 0.5, rules.grid.clearLeftUnits);
}

} // namespace

Hud::Rect PlateRect(const Rules& rules, const glm::dvec2& viewUnits, std::size_t index, std::size_t count) {
    const auto columns = static_cast<std::size_t>(rules.grid.columns);
    const std::size_t row = index / columns;
    const std::size_t column = index % columns;
    // A row that is not full is as wide as its plates and is centred in the grid's width.
    const std::size_t inRow = std::min(columns, count - row * columns);
    const double stepX = rules.grid.plateSizeUnits.x + rules.grid.gapUnits.x;
    const double rowWidth = static_cast<double>(inRow) * rules.grid.plateSizeUnits.x + static_cast<double>(inRow - 1) * rules.grid.gapUnits.x;
    const double left = GridLeft(rules, viewUnits) + (GridWidth(rules) - rowWidth) * 0.5;
    const double top = rules.grid.topUnits + static_cast<double>(row) * (rules.grid.plateSizeUnits.y + rules.grid.gapUnits.y);
    return Hud::Rect{glm::dvec2(left + static_cast<double>(column) * stepX, top), rules.grid.plateSizeUnits};
}

Hud::Rect GridRect(const Rules& rules, const glm::dvec2& viewUnits, std::size_t count) {
    const int rows = RowsOf(rules, count);
    const double height = static_cast<double>(rows) * rules.grid.plateSizeUnits.y + static_cast<double>(rows - 1) * rules.grid.gapUnits.y;
    return Hud::Rect{glm::dvec2(GridLeft(rules, viewUnits), rules.grid.topUnits), glm::dvec2(GridWidth(rules), height)};
}

glm::dvec2 TitleAt(const Rules& rules, const glm::dvec2& viewUnits, std::size_t count) {
    const Hud::Rect grid = GridRect(rules, viewUnits, count);
    return glm::dvec2(grid.min.x + grid.size.x * 0.5, rules.title.yAtScreen * viewUnits.y);
}

Target TargetAt(const Rules& rules, const Credits::Rules& credits, const glm::dvec2& viewUnits, double stateMs, const glm::dvec2& point, std::size_t count) {
    if (Credits::BackHitRect(credits, viewUnits, stateMs).Contains(point)) return Target{Target::Kind::Back, 0};
    for (std::size_t i = 0; i < count; ++i) {
        if (PlateRect(rules, viewUnits, i, count).Contains(point)) return Target{Target::Kind::Plate, static_cast<int>(i)};
    }
    return Target{};
}

std::vector<Piece> Pieces(const Rules& rules, const Credits::Rules& credits, const glm::dvec2& viewUnits, double stateMs, std::size_t chosen,
                          const Target& held, const std::string& title, const std::vector<std::string>& names) {
    std::vector<Piece> pieces;
    // The credits' background and back button, as the credits draw them: its first two pieces. (Its papyrus and its strip are the credits'.)
    for (const Credits::Piece& piece : Credits::Pieces(credits, viewUnits, stateMs, Credits::Scroll{}, held.kind == Target::Kind::Back)) {
        if (piece.element != Credits::Element::Background && piece.element != Credits::Element::Back) continue;
        Piece out;
        out.element = piece.element == Credits::Element::Background ? Element::Background : Element::Back;
        out.file = piece.file;
        out.rect = piece.rect;
        out.colour = Colour{piece.rgbByte, piece.rgbByte, piece.rgbByte, piece.alphaByte};
        pieces.push_back(std::move(out));
    }

    {
        Piece out;
        out.element = Element::Title;
        out.words = title;
        out.font = rules.title.font;
        out.unitsPerFontPx = rules.title.unitsPerFontPx;
        out.maxWidthUnits = rules.title.maxWidthUnits;
        out.rect = Hud::Rect{TitleAt(rules, viewUnits, names.size()), glm::dvec2(0.0)};
        out.colour = Colour{255, 255, 255, rules.title.alphaByte};
        pieces.push_back(std::move(out));
    }

    const int pressTint = held.kind == Target::Kind::None ? 255 : credits.state.pressTintByte;
    for (std::size_t i = 0; i < names.size(); ++i) {
        const Hud::Rect rect = PlateRect(rules, viewUnits, i, names.size());
        const bool isChosen = i == chosen;
        const bool isHeld = held.kind == Target::Kind::Plate && static_cast<std::size_t>(held.index) == i;
        // A held plate is multiplied by the press tint, as every menu button is (Button::draw).
        {
            Piece frame;
            frame.element = Element::PlateFrame;
            frame.rect = rect;
            frame.colour = isHeld ? Tinted(isChosen ? rules.plate.chosenFrame : rules.plate.idleFrame, pressTint)
                                  : (isChosen ? rules.plate.chosenFrame : rules.plate.idleFrame);
            frame.plate = static_cast<int>(i);
            pieces.push_back(std::move(frame));
        }
        {
            Piece word;
            word.element = Element::PlateWord;
            word.words = names[i];
            word.font = kNamesFont;
            word.unitsPerFontPx = rules.plate.wordUnitsPerFontPx;
            word.maxWidthUnits = rules.plate.maxWidthUnits;
            word.rect = Hud::Rect{rect.Centre(), glm::dvec2(0.0)};
            const Colour white{255, 255, 255, isChosen ? rules.plate.chosenWordAlphaByte : rules.plate.idleWordAlphaByte};
            word.colour = isHeld ? Tinted(white, pressTint) : white;
            word.plate = static_cast<int>(i);
            pieces.push_back(std::move(word));
        }
    }
    return pieces;
}

} // namespace MagicPortals::Picker

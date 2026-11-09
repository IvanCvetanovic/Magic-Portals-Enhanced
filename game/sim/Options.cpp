#include "sim/Options.hpp"

#include "core/Json.hpp"
#include "sim/Strings.hpp"

#include <cctype>
#include <cmath>
#include <utility>

namespace MagicPortals::Options {

namespace {

namespace Json = Supersonic::Json;
using UiLayer::Read::Byte;
using UiLayer::Read::Fraction;
using UiLayer::Read::Positive;
using UiLayer::Read::ReadPlaced;
using UiLayer::Read::Size;
using UiLayer::Read::Text;

// Long enough that every entrance and bounce is over: a layout "at rest", as the layer's own kSettledMs.
constexpr double kSettledMs = 1.0e9;

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

bool ReadWords(const Json::Value& block, Rules::Words& out, std::string& why, const std::string& where) {
    Rules::Words read;
    if (!Text(block, "words", read.words, why, where) || !Text(block, "font", read.font, why, where) ||
        !Positive(block, "units_per_font_px", read.unitsPerFontPx, why, where) ||
        !Fraction(block, "at_screen", read.atScreen, why, where) || !Byte(block, "alpha_byte", read.alphaByte, why, where) ||
        !UiLayer::Read::MaxWidth(block, read.maxWidthUnits, why, where)) {
        return false;
    }
    out = std::move(read);
    return true;
}

std::string Lower(std::string text) {
    for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

} // namespace

bool LoadRules(const std::string& path, const Controls::Rules& controls, Rules& out, std::string& error) {
    Json::Value root;
    if (!UiLayer::Read::File(path, root, error)) return false;
    const Json::Value& options = root["options"];
    if (!options.IsObject()) {
        error = path + ": options is missing or not an object";
        return false;
    }
    Rules read;
    std::string why;
    const auto fail = [&error, &path, &why]() {
        error = path + ": " + why;
        return false;
    };

    // The entry: a placed button whose sprite is the glyph's file name.
    if (!Object(options, "entry", why, "options")) return fail();
    const Json::Value& entry = options["entry"];
    if (!ReadPlaced(entry, "file", read.entry, why, "options.entry") ||
        !Number(entry, "body_inset", 0.0, Controls::kMaxGeneratedBodyInset, read.entryBodyInset, why, "options.entry")) {
        return fail();
    }
    // The generated tier WRITES this file into the folder the others are written to: one file in it, which is none of theirs.
    if (!Controls::IsBareFileName(read.entry.sprite)) {
        why = "options.entry.file is not a bare file name (no folder, no \"..\", no drive): \"" + read.entry.sprite + "\"";
        return fail();
    }
    for (const Controls::Id id : Controls::kClaimOrder) {
        const auto& generated = controls.art[static_cast<std::size_t>(Controls::ArtTier::Generated)][static_cast<std::size_t>(id)];
        if (generated && Lower(generated->file) == Lower(read.entry.sprite)) {
            why = std::string("options.entry.file names the same file as art.generated.") + Controls::KeyOf(id) +
                  ".file: each generated picture is a file of its own";
            return fail();
        }
    }

    if (!Object(options, "title", why, "options") || !ReadWords(options["title"], read.title, why, "options.title")) return fail();

    // The tiles.
    if (!Object(options, "tile", why, "options")) return fail();
    const Json::Value& tile = options["tile"];
    if (!Size(tile, "size_units", read.tileSizeUnits, why, "options.tile") || !Text(tile, "font", read.wordFont, why, "options.tile") ||
        !Positive(tile, "units_per_font_px", read.wordUnitsPerFontPx, why, "options.tile") ||
        !UiLayer::Read::Pair(tile, "word_at_units", read.wordAtUnits, why, "options.tile") ||
        !ReadColour(tile, "idle_frame_rgba", read.idleFrame, why, "options.tile") ||
        !ReadColour(tile, "chosen_frame_rgba", read.chosenFrame, why, "options.tile") ||
        !Byte(tile, "idle_word_alpha_byte", read.idleWordAlphaByte, why, "options.tile") ||
        !Byte(tile, "chosen_word_alpha_byte", read.chosenWordAlphaByte, why, "options.tile") ||
        !UiLayer::Read::MaxWidth(tile, read.wordMaxWidthUnits, why, "options.tile")) {
        return fail();
    }
    for (std::size_t i = 0; i < kTileStyles.size(); ++i) {
        const std::string name = Controls::NameOf(kTileStyles[i]);
        if (!Object(tile, name.c_str(), why, "options.tile")) return fail();
        const std::string where = "options.tile." + name;
        if (!Fraction(tile[name.c_str()], "at_screen", read.tiles[i].atScreen, why, where) ||
            !Text(tile[name.c_str()], "word", read.tiles[i].word, why, where)) {
            return fail();
        }
    }

    // The thumbnail.
    if (!Object(tile, "thumbnail", why, "options.tile")) return fail();
    const Json::Value& thumbnail = tile["thumbnail"];
    if (!Size(thumbnail, "size_units", read.thumbnail.sizeUnits, why, "options.tile.thumbnail") ||
        !UiLayer::Read::Pair(thumbnail, "at_units", read.thumbnail.atUnits, why, "options.tile.thumbnail") ||
        !Positive(thumbnail, "reference_height_units", read.thumbnail.referenceHeightUnits, why, "options.tile.thumbnail") ||
        !ReadColour(thumbnail, "background_rgba", read.thumbnail.background, why, "options.tile.thumbnail") ||
        !ReadColour(thumbnail, "walk_rgba", read.thumbnail.walk, why, "options.tile.thumbnail") ||
        !ReadColour(thumbnail, "corner_rgba", read.thumbnail.corner, why, "options.tile.thumbnail")) {
        return fail();
    }

    // The adjustment button.
    if (!Object(options, "adjust_button", why, "options")) return fail();
    const Json::Value& adjust = options["adjust_button"];
    if (!Fraction(adjust, "at_screen", read.adjust.atScreen, why, "options.adjust_button") ||
        !Size(adjust, "size_units", read.adjust.sizeUnits, why, "options.adjust_button") ||
        !Text(adjust, "word", read.adjust.word, why, "options.adjust_button") ||
        !Text(adjust, "font", read.adjust.font, why, "options.adjust_button") ||
        !Positive(adjust, "units_per_font_px", read.adjust.unitsPerFontPx, why, "options.adjust_button") ||
        !UiLayer::Read::Pair(adjust, "word_at_units", read.adjust.wordAtUnits, why, "options.adjust_button") ||
        !ReadColour(adjust, "frame_rgba", read.adjust.frame, why, "options.adjust_button") ||
        !Byte(adjust, "word_alpha_byte", read.adjust.wordAlphaByte, why, "options.adjust_button") ||
        !UiLayer::Read::MaxWidth(adjust, read.adjust.maxWidthUnits, why, "options.adjust_button")) {
        return fail();
    }

    out = std::move(read);
    return true;
}

Entry EntryAt(const Rules& rules, const UiLayer::Rules& layer, const MenuState::Rules& state, const glm::dvec2& viewUnits, double stateMs,
              bool held) {
    Entry entry;
    const glm::dvec2 at = UiLayer::ButtonAnchorAt(layer, UiLayer::Anchor(rules.entry, viewUnits), viewUnits, stateMs);
    entry.rect = UiLayer::RectAt(rules.entry, at);
    entry.picture = Controls::DrawRect(entry.rect, 0.0, rules.entryBodyInset);
    entry.rgbByte = held ? state.pressTintByte : 255;
    entry.alphaByte = UiLayer::ButtonAlphaByte(layer, stateMs);
    return entry;
}

Widget WidgetOf(Controls::Style style) {
    return style == Controls::Style::Modern ? Widget::ModernTile : Widget::ClassicTile;
}

std::optional<Controls::Style> StyleOf(Widget widget) {
    switch (widget) {
    case Widget::ModernTile:
        return Controls::Style::Modern;
    case Widget::ClassicTile:
        return Controls::Style::Classic;
    case Widget::None:
    case Widget::Back:
    case Widget::Adjust: // the button that opens the adjustment screen chooses no style
        break;
    }
    return std::nullopt;
}

Press Follow(Touch& touch, bool pressed, bool released, bool touching, Widget on) {
    if (pressed) touch.down = on;
    touch.held = touch.down != Widget::None && touching && on == touch.down ? touch.down : Widget::None;
    if (!released) return Press{};
    const Press press{touch.down != Widget::None && on == touch.down, touch.down};
    touch = Touch{};
    return press;
}

namespace {

std::size_t TileIndex(Controls::Style style) {
    return style == Controls::Style::Modern ? 0u : 1u;
}

} // namespace

Hud::Rect TileRect(const Rules& rules, Controls::Style style, const glm::dvec2& viewUnits) {
    const glm::dvec2 centre = rules.tiles[TileIndex(style)].atScreen * viewUnits;
    return Hud::Rect{centre - rules.tileSizeUnits * 0.5, rules.tileSizeUnits};
}

Hud::Rect AdjustRect(const Rules& rules, const glm::dvec2& viewUnits) {
    const glm::dvec2 centre = rules.adjust.atScreen * viewUnits;
    return Hud::Rect{centre - rules.adjust.sizeUnits * 0.5, rules.adjust.sizeUnits};
}

Widget WidgetAt(const Rules& rules, const Credits::Rules& credits, const glm::dvec2& viewUnits, double stateMs, const glm::dvec2& point,
                bool adjustShown) {
    if (Credits::BackHitRect(credits, viewUnits, stateMs).Contains(point)) return Widget::Back;
    for (const Controls::Style style : kTileStyles) {
        if (TileRect(rules, style, viewUnits).Contains(point)) return WidgetOf(style);
    }
    if (adjustShown && AdjustRect(rules, viewUnits).Contains(point)) return Widget::Adjust;
    return Widget::None;
}

std::array<Hud::Rect, 5> ThumbnailRects(const Rules& rules, const Controls::Rules& controls, const Hud::Rules& hud,
                                        const Controls::Tuning& tuning, Controls::Style style, const Hud::Rect& tile) {
    const Rules::Thumbnail& thumb = rules.thumbnail;
    // The layout on a view the thumbnail's own shape, as tall as the real view is (so the same numbers in units), then scaled to fit it.
    const double scale = thumb.sizeUnits.y / thumb.referenceHeightUnits;
    const glm::dvec2 reference(thumb.referenceHeightUnits * thumb.sizeUnits.x / thumb.sizeUnits.y, thumb.referenceHeightUnits);
    const Controls::Layout layout = Controls::LayOut(style, controls, hud, tuning, reference, Controls::Insets{}, kSettledMs);
    const glm::dvec2 origin = tile.min + thumb.atUnits;
    std::array<Hud::Rect, 5> rects;
    for (const Controls::Id id : Controls::kClaimOrder) {
        const Hud::Rect& box = layout.At(id).rect;
        rects[static_cast<std::size_t>(id)] = Hud::Rect{origin + box.min * scale, box.size * scale};
    }
    return rects;
}

std::vector<Piece> Pieces(const Rules& rules, const Credits::Rules& credits, const Controls::Rules& controls, const Hud::Rules& hud,
                          const Controls::Tuning& tuning, const glm::dvec2& viewUnits, double stateMs, Controls::Style chosen, Widget held,
                          bool adjustShown, const Strings::Table* strings) {
    std::vector<Piece> pieces;
    // The credits' background and back button, as the credits draw them: its first two pieces. (Its papyrus and its strip are the credits'.)
    for (const Credits::Piece& piece : Credits::Pieces(credits, viewUnits, stateMs, Credits::Scroll{}, held == Widget::Back)) {
        if (piece.element != Credits::Element::Background && piece.element != Credits::Element::Back) continue;
        Piece out;
        out.element = piece.element == Credits::Element::Background ? Element::Background : Element::Back;
        out.file = piece.file;
        out.rect = piece.rect;
        out.colour = Colour{piece.rgbByte, piece.rgbByte, piece.rgbByte, piece.alphaByte};
        pieces.push_back(std::move(out));
    }

    {
        Piece title;
        title.element = Element::Title;
        title.words = strings != nullptr ? strings->Show(Strings::Key::kOptionsTitle) : rules.title.words;
        title.font = rules.title.font;
        title.unitsPerFontPx = rules.title.unitsPerFontPx;
        title.maxWidthUnits = rules.title.maxWidthUnits;
        title.rect = Hud::Rect{rules.title.atScreen * viewUnits, glm::dvec2(0.0)};
        title.colour = Colour{255, 255, 255, rules.title.alphaByte};
        pieces.push_back(std::move(title));
    }

    const int pressTint = held == Widget::None ? 255 : credits.state.pressTintByte;
    for (const Controls::Style style : kTileStyles) {
        const Hud::Rect tile = TileRect(rules, style, viewUnits);
        const bool isChosen = style == chosen;
        const bool isHeld = held == WidgetOf(style);
        // A held tile is multiplied by the press tint, as every menu button is (Button::draw).
        const auto tinted = [&](Colour colour) {
            if (!isHeld) return colour;
            for (int i = 0; i < 3; ++i) colour[static_cast<std::size_t>(i)] = colour[static_cast<std::size_t>(i)] * pressTint / 255;
            return colour;
        };
        {
            Piece frame;
            frame.element = Element::TileFrame;
            frame.rect = tile;
            frame.colour = tinted(isChosen ? rules.chosenFrame : rules.idleFrame);
            frame.tile = style;
            pieces.push_back(std::move(frame));
        }
        {
            Piece base;
            base.element = Element::ThumbnailBase;
            base.rect = Hud::Rect{tile.min + rules.thumbnail.atUnits, rules.thumbnail.sizeUnits};
            base.colour = rules.thumbnail.background;
            base.tile = style;
            pieces.push_back(std::move(base));
        }
        const std::array<Hud::Rect, 5> rects = ThumbnailRects(rules, controls, hud, tuning, style, tile);
        for (const Controls::Id id : Controls::kClaimOrder) {
            Piece control;
            control.element = Element::ThumbnailControl;
            control.rect = rects[static_cast<std::size_t>(id)];
            control.colour = Controls::IsWalk(id) ? rules.thumbnail.walk : rules.thumbnail.corner;
            control.tile = style;
            control.control = id;
            pieces.push_back(std::move(control));
        }
        {
            Piece word;
            word.element = Element::Word;
            word.words = strings != nullptr
                             ? strings->Show(style == Controls::Style::Modern ? Strings::Key::kOptionsModern : Strings::Key::kOptionsClassic)
                             : rules.tiles[TileIndex(style)].word;
            word.font = rules.wordFont;
            word.unitsPerFontPx = rules.wordUnitsPerFontPx;
            word.maxWidthUnits = rules.wordMaxWidthUnits;
            word.rect = Hud::Rect{tile.min + rules.wordAtUnits, glm::dvec2(0.0)};
            word.colour = tinted(Colour{255, 255, 255, isChosen ? rules.chosenWordAlphaByte : rules.idleWordAlphaByte});
            word.tile = style;
            pieces.push_back(std::move(word));
        }
    }
    if (adjustShown) {
        const Hud::Rect button = AdjustRect(rules, viewUnits);
        const bool isHeld = held == Widget::Adjust;
        const auto tinted = [&](Colour colour) {
            if (!isHeld) return colour;
            for (int i = 0; i < 3; ++i) colour[static_cast<std::size_t>(i)] = colour[static_cast<std::size_t>(i)] * pressTint / 255;
            return colour;
        };
        Piece frame;
        frame.element = Element::AdjustFrame;
        frame.rect = button;
        frame.colour = tinted(rules.adjust.frame);
        pieces.push_back(std::move(frame));
        Piece word;
        word.element = Element::Word;
        word.words = strings != nullptr ? strings->Show(Strings::Key::kOptionsAdjust) : rules.adjust.word;
        word.font = rules.adjust.font;
        word.unitsPerFontPx = rules.adjust.unitsPerFontPx;
        word.maxWidthUnits = rules.adjust.maxWidthUnits;
        word.rect = Hud::Rect{button.min + rules.adjust.wordAtUnits, glm::dvec2(0.0)};
        word.colour = tinted(Colour{255, 255, 255, rules.adjust.wordAlphaByte});
        pieces.push_back(std::move(word));
    }
    return pieces;
}

} // namespace MagicPortals::Options

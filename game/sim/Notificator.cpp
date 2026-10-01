#include "sim/Notificator.hpp"

#include "core/Json.hpp"
#include "sim/Ease.hpp"
#include "sim/GravityWell.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <utility>

namespace MagicPortals::Notificator {

namespace {

namespace Json = Supersonic::Json;

bool ReadRoot(const std::string& path, Json::Value& root, std::string& error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = path + ": cannot open";
        return false;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string text = buffer.str();
    Json::Parser parser(text);
    if (!parser.Parse(root)) {
        error = path + ": " + parser.Error();
        return false;
    }
    return true;
}

bool Pair(const Json::Value& value, glm::dvec2& out) {
    if (!value.IsArray() || value.AsArray().size() != 2 || !value.AsArray()[0].IsNumber() ||
        !value.AsArray()[1].IsNumber()) {
        return false;
    }
    out = glm::dvec2(value.AsArray()[0].AsNumber(), value.AsArray()[1].AsNumber());
    return true;
}

bool Number(const Json::Value& object, const char* key, double& out) {
    if (!object[key].IsNumber()) return false;
    out = object[key].AsNumber();
    return true;
}

} // namespace

bool LoadRules(const std::string& achievementsPath, const std::string& wellsPath, Rules& out, std::string& error) {
    Json::Value root;
    if (!ReadRoot(achievementsPath, root, error)) return false;
    const Json::Value& block = root["notificator"];
    Rules read;
    double columns = 0.0;
    if (!Pair(block["normalized_pos"], read.normalizedPos) || !Pair(block["origin"], read.origin) ||
        !block["font"].IsString() || block["font"].AsString().empty() ||
        !Number(block, "units_per_font_px", read.unitsPerFontPx) ||
        !Pair(block["text_offset_units"], read.textOffsetUnits) || !Pair(block["icon_units"], read.iconUnits) ||
        !Pair(block["background_units"], read.backgroundUnits) || !Number(block, "background_columns", columns) ||
        !Number(block, "effect_ms_cap", read.effectMsCap) || !Number(block, "fade_ms_cap", read.fadeMsCap) ||
        !Number(block, "music_volume_shown", read.musicVolumeShown) ||
        !Number(block, "music_volume_default_first_track", read.musicVolumeDefaultFirstTrack) ||
        !Number(block, "music_volume_default_other", read.musicVolumeDefaultOther) ||
        !block["music_first_track"].IsString() || columns < 1.0 || !(read.unitsPerFontPx > 0.0)) {
        error = achievementsPath + ": notificator lacks a place, origin, font, scale, text offset, icon size, "
                                   "background size and columns, the two strides' caps or the music's volumes";
        return false;
    }
    read.font = block["font"].AsString();
    read.backgroundColumns = static_cast<int>(columns);
    read.musicFirstTrack = block["music_first_track"].AsString();

    Json::Value wells;
    if (!ReadRoot(wellsPath, wells, error)) return false;
    const Json::Value& lap = wells["roundabout"];
    if (!Number(lap, "elastic_period", read.elasticPeriod) ||
        !Number(lap, "elastic_shift_divisor", read.elasticShiftDivisor) ||
        !Number(lap, "elastic_decay", read.elasticDecay) || !(read.elasticPeriod > 0.0) ||
        !(read.elasticShiftDivisor > 0.0)) {
        error = wellsPath + ": roundabout lacks the elastic's three numbers";
        return false;
    }
    out = std::move(read);
    return true;
}

void Queue::Notify(Card card) {
    // notify (48475..48571): insertionTime = m_elapsedTime, onto the stack.
    card.insertionMs = m_elapsedMs;
    m_stack.push_back(std::move(card));
}

std::vector<Event> Queue::Update(double dtMs) {
    std::vector<Event> events;
    // update #3-8.
    m_elapsedMs += dtMs;
    // updateStack (47981..48475): the first whose delay has passed, strictly
    // (CMPu, JNP past), and no more this step (#112 JMP out of the loop).
    for (std::size_t t = 0; t < m_stack.size(); ++t) {
        if (!(m_elapsedMs - m_stack[t].insertionMs > m_stack[t].delayMs)) continue;
        Card card = std::move(m_stack[t]);
        m_stack.erase(m_stack.begin() + static_cast<std::ptrdiff_t>(t));
        card.insertionMs = m_elapsedMs;
        m_shown.push_back(card);
        events.push_back(Event{Event::Kind::Shown, std::move(card)});
        break;
    }
    // update #13-98, over the count as updateStack left it: the first goes once its
    // time is strictly past its duration; every one behind starts its time again.
    const std::size_t count = m_shown.size();
    for (std::size_t t = 0; t < count; ++t) {
        if (t != 0) {
            m_shown[t].insertionMs = m_elapsedMs;
            continue;
        }
        if (m_elapsedMs - m_shown[0].insertionMs > m_shown[0].durationMs) {
            events.push_back(Event{Event::Kind::Dismissed, m_shown.front()});
            m_shown.erase(m_shown.begin());
            break;
        }
    }
    return events;
}

double Elastic(const Rules& rules, double n) {
    GravityWell::Roundabout elastic;
    elastic.elasticPeriod = rules.elasticPeriod;
    elastic.elasticShiftDivisor = rules.elasticShiftDivisor;
    elastic.elasticDecay = rules.elasticDecay;
    return static_cast<double>(GravityWell::Elastic(elastic, static_cast<float>(n)));
}

Effect EffectOf(const Rules& rules, const Card& card, double ageMs) {
    // computeEffectScale (47404..47981), in the script's floats.
    Effect effect;
    const float duration = static_cast<float>(card.durationMs);
    const float effectStride = std::min(duration * 0.5f, static_cast<float>(rules.effectMsCap));
    const float fadeStride = std::min(duration * 0.5f, static_cast<float>(rules.fadeMsCap));
    const float elapsed = static_cast<float>(ageMs);
    const float fadeElapsed = elapsed - (duration - fadeStride);
    if (elapsed < effectStride) {
        const float bias = std::max(std::min(elapsed / effectStride, 1.0f), 0.0f);
        effect.scale = Elastic(rules, static_cast<double>(bias));
        return effect;
    }
    if (elapsed > duration - fadeStride) {
        const float alpha = 1.0f - std::max(std::min(fadeElapsed / fadeStride, 1.0f), 0.0f);
        effect.alpha = static_cast<double>(alpha);
        effect.scale = Ease::SmoothEnd(static_cast<double>(alpha));
    }
    return effect;
}

Layout LayOut(const Rules& rules, const Card& card, double ageMs, const glm::dvec2& viewUnits,
              const glm::dvec2& textFontPx) {
    // Notificator::draw (45185..47404).
    Layout layout;
    const Effect effect = EffectOf(rules, card, ageMs);
    const double scale = effect.scale; // scale(elasticScale): units a sprite pixel
    layout.alpha = effect.alpha;
    layout.unitsPerFontPx = rules.unitsPerFontPx * scale; // #45-47
    const glm::dvec2 iconSize = rules.iconUnits * scale;  // #51-62
    layout.hasIcon = !card.icon.empty();
    const double textPitch = layout.hasIcon ? iconSize.x / 2.0 : 0.0; // #64-83
    // #91-137: ComputeTextBoxSize x the font's scale, widened by the pitch.
    const glm::dvec2 textBox = textFontPx * layout.unitsPerFontPx + glm::dvec2(textPitch, 0.0);
    const double width = textBox.x;
    const glm::dvec2 originPos = viewUnits * rules.normalizedPos; // #139-148
    // #150-192: the background cut columns x 1, a column's frame times the scale.
    const int columns = std::max(rules.backgroundColumns, 1);
    const glm::dvec2 frame(rules.backgroundUnits.x / columns, rules.backgroundUnits.y);
    const glm::dvec2 size = frame * scale;
    const glm::dvec2 pieceOrigin(0.0, rules.origin.y); // #194-206
    const glm::dvec2 piece0 = originPos - glm::dvec2(width / 2.0 + size.x, 0.0); // #208-224
    const glm::dvec2 piece1 = piece0 + glm::dvec2(size.x, 0.0);                  // #248-261
    const glm::dvec2 piece2 = piece1 + glm::dvec2(width, 0.0);                   // #313-324
    const glm::dvec2 sizes[3] = {size, glm::dvec2(width, size.y), size};
    const glm::dvec2 points[3] = {piece0, piece1, piece2};
    for (int c = 0; c < 3; ++c) {
        layout.pieces[c].size = sizes[c];
        layout.pieces[c].min = points[c] - pieceOrigin * sizes[c];
        const int column = std::min(c, columns - 1);
        layout.uvMin[c] = glm::dvec2(static_cast<double>(column) / columns, 0.0);
        layout.uvMax[c] = glm::dvec2(static_cast<double>(column + 1) / columns, 1.0);
    }
    // #359-380: the card's middle; #382-428 the text there, offset and pitched.
    const glm::dvec2 centre = originPos - glm::dvec2(0.0, rules.origin.y * size.y * 0.5);
    const glm::dvec2 offset = rules.textOffsetUnits * scale;
    layout.textCentre = centre + offset + glm::dvec2(textPitch, 0.0);
    // #430-506: the icon at V2_HALF on the text box's left edge.
    if (layout.hasIcon) {
        const glm::dvec2 at = centre - glm::dvec2(width / 2.0, 0.0) + offset;
        layout.icon.size = iconSize;
        layout.icon.min = at - iconSize * 0.5;
    }
    return layout;
}

Box LayOutBox(const glm::dvec2& tileUnits, int columns, int rows, double tileScale, double unitsPerFontPx,
              const glm::dvec2& at, const glm::dvec2& origin, const glm::dvec2& textFontPx) {
    Box box;
    // TextBoxFrame::draw #3-54: the box and its top-left.
    const glm::dvec2 size = textFontPx * unitsPerFontPx;
    const glm::dvec2 pos = at - size * origin;
    box.textTopLeft = pos;
    // ImageFrame::draw with V2_ZERO for its origin and borderOutside set: #26-75 the
    // tiles cut and a tile's size; #77-135 nothing moved.
    const int cols = std::max(columns, 1);
    const int rws = std::max(rows, 1);
    const glm::dvec2 tile = glm::dvec2(tileUnits.x / cols, tileUnits.y / rws) * tileScale;
    // Each piece: where, its origin, its size (#137-511), drawSprite placing it at
    // pos - origin x size (ImageFrame::drawSprite 35953..36120).
    struct Placed {
        glm::dvec2 at;
        glm::dvec2 origin;
        glm::dvec2 size;
    };
    const Placed placed[9] = {
        {pos, {1.0, 1.0}, tile},                                       // 0, #148-166
        {pos, {0.0, 1.0}, {size.x, tile.y}},                           // 1, #181-207
        {pos + glm::dvec2(size.x, 0.0), {0.0, 1.0}, tile},             // 2, #222-251
        {pos, {1.0, 0.0}, {tile.x, size.y}},                           // 3, #266-292
        {pos, {0.0, 0.0}, size},                                       // 4, #307-325
        {pos + glm::dvec2(size.x, 0.0), {0.0, 0.0}, {tile.x, size.y}}, // 5, #340-377
        {pos + glm::dvec2(0.0, size.y), {1.0, 0.0}, tile},             // 6, #392-421
        {pos + glm::dvec2(0.0, size.y), {0.0, 0.0}, {size.x, tile.y}}, // 7, #436-473
        {pos + size, {0.0, 0.0}, tile},                                // 8, #488-511
    };
    for (int i = 0; i < 9; ++i) {
        box.pieces[i].size = placed[i].size;
        box.pieces[i].min = placed[i].at - placed[i].origin * placed[i].size;
        const int column = (i % 3) % cols;
        const int row = (i / 3) % rws;
        box.uvMin[i] = glm::dvec2(static_cast<double>(column) / cols, static_cast<double>(row) / rws);
        box.uvMax[i] = glm::dvec2(static_cast<double>(column + 1) / cols, static_cast<double>(row + 1) / rws);
    }
    return box;
}

} // namespace MagicPortals::Notificator

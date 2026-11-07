#include "sim/Sky.hpp"

#include "sim/Roles.hpp"

#include "core/Json.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <utility>

namespace MagicPortals::Sky {

namespace {

namespace Json = Supersonic::Json;

bool PositionOf(const Tscn::Node& node, glm::dvec2& out) {
    const Tscn::Value* position = node.Find("position");
    if (position == nullptr || position->kind != Tscn::Value::Kind::Vector2) return false;
    out = glm::dvec2(position->numbers[0], position->numbers[1]);
    return true;
}

// A level entity's float as ETHEntity::GetFloat answers it: the number, or 0 for
// a name the entity does not carry.
double FloatOf(const Tscn::Node& node, const std::string& key) {
    double value = 0.0;
    const Tscn::Value* found = node.Meta(key);
    if (found == nullptr || !found->AsNumber(value)) return 0.0;
    return value;
}

} // namespace

bool LoadRules(const std::string& path, Rules& out, std::string& error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = path + ": cannot read";
        return false;
    }
    std::ostringstream text;
    text << file.rdbuf();

    // Named, not a temporary: the parser keeps a reference to what it reads.
    const std::string content = text.str();
    Json::Parser parser(content);
    Json::Value root;
    if (!parser.Parse(root)) {
        error = path + ": " + parser.Error();
        return false;
    }
    const Json::Value& block = root["static_sky"];
    const auto number = [&](const char* key, double& into) -> bool {
        if (!block.Has(key) || !block[key].IsNumber()) {
            error = path + ": static_sky." + key + " is missing or not a number";
            return false;
        }
        into = block[key].AsNumber();
        return true;
    };
    const auto string = [&](const char* key, std::string& into) -> bool {
        if (!block.Has(key) || !block[key].IsString() || block[key].AsString().empty()) {
            error = path + ": static_sky." + key + " is missing, not a string or empty";
            return false;
        }
        into = block[key].AsString();
        return true;
    };

    Rules read;
    if (!string("properties_entity", read.propertiesEntity) || !string("space_bg_key", read.spaceBgKey) ||
        !string("satellite_name", read.satelliteName) || !string("scroll_key", read.scrollKey) ||
        !number("screen_pitch", read.screenPitch) || !number("frame_cap_ms", read.frameCapMs)) {
        return false;
    }
    if (!block.Has("entity_names") || !block["entity_names"].IsArray()) {
        error = path + ": static_sky.entity_names is missing or not an array";
        return false;
    }
    for (const Json::Value& one : block["entity_names"].AsArray()) {
        if (!one.IsString() || one.AsString().empty()) {
            error = path + ": static_sky.entity_names holds a value that is not a name";
            return false;
        }
        read.skyNames.push_back(one.AsString());
    }
    // Each of these would draw a sky that looks placed and is not: no names finds
    // no sky, so every level keeps the position its editor left; a pitch of 0 pins
    // the sky's centre to the view's corner; no frame cap lets one long frame
    // throw a scrolling strip past its wrap.
    if (read.skyNames.empty()) {
        error = path + ": static_sky.entity_names names at least one entity";
        return false;
    }
    if (read.screenPitch <= 0.0 || read.frameCapMs <= 0.0) {
        error = path + ": static_sky.screen_pitch and static_sky.frame_cap_ms are above zero";
        return false;
    }
    if (!block.Has("pixel_cover") || !block["pixel_cover"].IsBool()) {
        error = path + ": static_sky.pixel_cover is missing or not true or false";
        return false;
    }
    read.pixelCover = block["pixel_cover"].AsBool();

    // Enhanced's own wide-window rule (wide_view), strictly: a file that does not say whether the
    // sky grows on a wide window is refused, as pixel_cover's is, and so is a number that would
    // grow it where the original's frame stands (a negative tolerance) or draw it short of the view.
    const Json::Value& wide = block["wide_view"];
    if (!wide.Has("enabled") || !wide["enabled"].IsBool()) {
        error = path + ": static_sky.wide_view.enabled is missing or not true or false";
        return false;
    }
    read.wide.enabled = wide["enabled"].AsBool();
    for (const char* key : {"tolerance_units", "overscan_units"}) {
        if (!wide.Has(key) || !wide[key].IsNumber() || wide[key].AsNumber() < 0.0) {
            error = path + ": static_sky.wide_view." + key + " is missing, not a number or below zero";
            return false;
        }
    }
    read.wide.toleranceUnits = wide["tolerance_units"].AsNumber();
    read.wide.overscanUnits = wide["overscan_units"].AsNumber();
    if (!wide.Has("pictures") || !wide["pictures"].IsArray()) {
        error = path + ": static_sky.wide_view.pictures is missing or not an array";
        return false;
    }
    for (const Json::Value& one : wide["pictures"].AsArray()) {
        const Json::Value& contentUnits = one["content_units"];
        if (!one["file"].IsString() || one["file"].AsString().empty() || !one["tier"].IsString() || !contentUnits.IsArray() ||
            contentUnits.AsArray().size() != 2 || !contentUnits.AsArray()[0].IsNumber() || !contentUnits.AsArray()[1].IsNumber()) {
            error = path + ": static_sky.wide_view.pictures holds an entry without a file, a tier and two content_units";
            return false;
        }
        WidePicture picture;
        picture.file = one["file"].AsString();
        picture.tier = one["tier"].AsString();
        picture.contentFromUnits = contentUnits.AsArray()[0].AsNumber();
        picture.contentToUnits = contentUnits.AsArray()[1].AsNumber();
        if (!(picture.contentFromUnits >= 0.0) || !(picture.contentToUnits > picture.contentFromUnits)) {
            error = path + ": static_sky.wide_view.pictures: " + picture.file + "'s content_units must run from 0 or more to above that";
            return false;
        }
        read.wide.pictures.push_back(std::move(picture));
    }
    out = std::move(read);
    return true;
}

bool Build(const Rules& rules, const Tscn::Scene& scene, const std::vector<Sprites::Sprite>& sprites,
           double viewHeightPx, Controller& out, std::string& error) {
    Controller built;

    // Game::preLoop's branch: a space level's sky is SpaceSky's, and StaticSky is
    // never made. readProperties asks only whether the first `properties` carries
    // space_bg at all (CheckCustomData != 0), not what it holds.
    for (const Tscn::Node& node : scene.nodes) {
        if (node.parent != "." || Roles::EntityName(node) != rules.propertiesEntity) continue;
        if (node.Meta(rules.spaceBgKey) != nullptr) {
            out = Controller{};
            return true;
        }
        break;
    }

    // GetEntityArray for each name in turn, each exact. The original walks its
    // buckets; the port walks the file. No level places two skies, so the two
    // orders never differ in play.
    for (const std::string& name : rules.skyNames) {
        for (const Tscn::Node& node : scene.nodes) {
            if (node.parent != "." || Roles::EntityName(node) != name) continue;
            const auto sprite = std::find_if(sprites.begin(), sprites.end(),
                                             [&node](const Sprites::Sprite& s) { return s.node == node.name; });
            if (sprite == sprites.end() || sprite->sizePx.y <= 0.0) {
                error = "the sky " + node.name + " has no picture to size";
                return false;
            }
            Layer sky;
            sky.node = node.name;
            sky.imagePx = sprite->sizePx;
            // Its content: the whole file, unless wide_view knows black columns in it (found by the
            // file's name and the tier folder it was drawn from; a sprite with neither is all content).
            sky.contentFromPx = 0.0;
            sky.contentToPx = sprite->sizePx.x;
            const std::string file = std::filesystem::path(sprite->texture).filename().string();
            for (const WidePicture& known : rules.wide.pictures) {
                if (known.file != file || known.tier != sprite->tier) continue;
                sky.contentFromPx = known.contentFromUnits;
                sky.contentToPx = known.contentToUnits;
                break;
            }
            sky.scrollValue = FloatOf(node, rules.scrollKey);
            built.skies.push_back(std::move(sky));
        }
    }

    // scaleSky, in its own order: the scale, then m_width and m_scroll from the
    // sky just scaled - one member each, so the last sky's stand - then the pivot
    // a scrolling strip's ends take.
    const std::size_t count = built.skies.size();
    for (std::size_t t = 0; t < count; ++t) {
        Layer& sky = built.skies[t];
        sky.scale = viewHeightPx / sky.imagePx.y;
        built.widthPx = sky.imagePx.x * sky.scale;
        built.scrollValue = sky.scrollValue;
        built.scroll = sky.scrollValue > 0.0;
        sky.scrollX = 0.0;
        if (!built.scroll) continue;
        // SetPivotAdjust(size * (+-1, 0)) with the size read BEFORE the scale. It
        // divides by the new scale and the draw multiplies by it again, so what
        // moves the picture is that unscaled width. The last call wins on a lone
        // sky, which is both the first and the last.
        if (t == 0) sky.pivotX = sky.imagePx.x;
        if (t + 1 == count) sky.pivotX = -sky.imagePx.x;
    }

    for (const Tscn::Node& node : scene.nodes) {
        if (node.parent != "." || Roles::EntityName(node) != rules.satelliteName) continue;
        glm::dvec2 atPx(0.0);
        if (!PositionOf(node, atPx)) continue;
        built.hasSatellite = true;
        built.satelliteNode = node.name;
        built.satelliteOriginalPx = atPx;
        break;
    }

    built.running = count > 0 || built.hasSatellite;
    out = std::move(built);
    return true;
}

glm::dvec2 PositionPx(const Rules& rules, const Controller& sky, std::size_t t, const glm::dvec2& viewCentrePx,
                      const glm::dvec2& viewPx) {
    const Layer& layer = sky.skies[t];
    const glm::dvec2 size = layer.imagePx * layer.scale;
    glm::dvec2 pitch = size * rules.screenPitch;
    if (!sky.scroll) {
        pitch = viewPx * rules.screenPitch;
    } else {
        if (t == 0) pitch.x += size.x;
        if (t + 1 == sky.skies.size()) pitch.x -= size.x;
    }
    const glm::dvec2 originalPos = pitch + glm::dvec2(sky.widthPx * static_cast<double>(t), 0.0);
    // GetCameraPos() is the corner; the port's camera keeps its centre.
    const glm::dvec2 cornerPx = viewCentrePx - viewPx * 0.5;
    return cornerPx + originalPos + glm::dvec2(layer.scrollX, 0.0);
}

glm::dvec2 DrawnCentrePx(const Rules& rules, const Controller& sky, std::size_t t, const glm::dvec2& viewCentrePx,
                         const glm::dvec2& viewPx) {
    return PositionPx(rules, sky, t, viewCentrePx, viewPx) - glm::dvec2(sky.skies[t].pivotX, 0.0);
}

glm::dvec2 DrawnSizePx(const Controller& sky, std::size_t t) {
    const Layer& layer = sky.skies[t];
    return layer.imagePx * layer.scale;
}

Fit WideFit(const Rules& rules, const Layer& layer, double viewWidthPx) {
    Fit fit;
    // The content across as drawn, and where the file's centre lies against the content's.
    const double contentPx = (layer.contentToPx - layer.contentFromPx) * layer.scale;
    if (!rules.wide.enabled || !(contentPx > 0.0) || !(viewWidthPx > contentPx + rules.wide.toleranceUnits)) return fit;
    fit.scale = (viewWidthPx + 2.0 * rules.wide.overscanUnits) / contentPx;
    // The quad grows about ITS OWN centre, which is the view's centre (a still sky is pinned there). Its content's centre lies `a`
    // from that (the content's middle less the file's) and the growth moves it to a * scale; for the content to cover the view it has
    // to be centred on the view, so the quad moves by -a * scale. a is 0 for the skies shipped, whose black columns are symmetric,
    // so only a picture with unequal margins shows the difference: a shift of a * (1 - scale) instead leaves the content a from the
    // view's centre and a bare strip of |a| less the overscan on one side.
    const double contentMiddlePx = (layer.contentFromPx + layer.contentToPx) * 0.5 * layer.scale;
    const double offsetPx = contentMiddlePx - layer.imagePx.x * layer.scale * 0.5;
    fit.shiftXPx = -offsetPx * fit.scale;
    return fit;
}

glm::dvec2 SatellitePx(const Controller& sky, const glm::dvec2& viewCentrePx, const glm::dvec2& viewPx) {
    return viewCentrePx - viewPx * 0.5 + sky.satelliteOriginalPx;
}

Span PixelCover(double lowPx, double highPx, double viewLowPx, double viewSpanPx, double framePx) {
    if (!(highPx > lowPx) || !(viewSpanPx > 0.0) || !(framePx > 0.0)) return Span{lowPx, highPx};
    const double perLevelPx = framePx / viewSpanPx;
    // Pixel i's centre is at i + 0.5. A low edge at e draws pixel i when
    // i + 0.5 >= e, so from ceil(e - 0.5); a high edge at e draws it when
    // i + 0.5 < e, so up to ceil(e - 0.5), exclusive.
    const double low = std::ceil((lowPx - viewLowPx) * perLevelPx - 0.5);
    const double high = std::ceil((highPx - viewLowPx) * perLevelPx - 0.5);
    return Span{viewLowPx + low / perLevelPx, viewLowPx + high / perLevelPx};
}

void Advance(const Rules& rules, Controller& sky, double dtS) {
    if (!sky.scroll) return;
    // scaledUnitsPerSecond(-m_scrollValue): the frame capped at 200 ms, in the
    // level's own units once g_scale is folded away.
    const double frameMs = std::min(rules.frameCapMs, dtS * 1000.0);
    const double move = -sky.scrollValue * frameMs / 1000.0;
    for (Layer& layer : sky.skies) {
        layer.scrollX += move;
        if (std::fabs(layer.scrollX) >= sky.widthPx) layer.scrollX = 0.0;
    }
}

// ---- SpaceSky ---------------------------------------------------------------

bool LoadSpaceRules(const std::string& path, SpaceRules& out, std::string& error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = path + ": cannot read";
        return false;
    }
    std::ostringstream text;
    text << file.rdbuf();
    const std::string content = text.str();
    Json::Parser parser(content);
    Json::Value root;
    if (!parser.Parse(root)) {
        error = path + ": " + parser.Error();
        return false;
    }
    const Json::Value& block = root["space_sky"];
    const auto number = [&](const char* key, double& into) -> bool {
        if (!block.Has(key) || !block[key].IsNumber()) {
            error = path + ": space_sky." + key + " is missing or not a number";
            return false;
        }
        into = block[key].AsNumber();
        return true;
    };
    const auto string = [&](const char* key, std::string& into) -> bool {
        if (!block.Has(key) || !block[key].IsString() || block[key].AsString().empty()) {
            error = path + ": space_sky." + key + " is missing, not a string or empty";
            return false;
        }
        into = block[key].AsString();
        return true;
    };
    const auto pair = [&](const char* key, glm::dvec2& into) -> bool {
        const Json::Value& value = block[key];
        if (!value.IsArray() || value.AsArray().size() != 2 || !value.AsArray()[0].IsNumber() ||
            !value.AsArray()[1].IsNumber()) {
            error = path + ": space_sky." + key + " is not two numbers";
            return false;
        }
        into = glm::dvec2(value.AsArray()[0].AsNumber(), value.AsArray()[1].AsNumber());
        return true;
    };

    SpaceRules read;
    double startAngle = 0.0;
    double updateStartAngle = 0.0;
    if (!string("planet_name", read.planetName) || !string("spiral_name", read.spiralName) ||
        !string("sky_name", read.skyName) || !string("eclipse_name", read.eclipseName) ||
        !number("spiral_diagonal_scale", read.spiralDiagonalScale) || !pair("spiral_pitch", read.spiralPitch) ||
        !number("spiral_turn_deg_per_s", read.spiralTurnDegPerS) ||
        !string("spiral_colour_key", read.spiralColourKey) ||
        !number("spiral_colour_default", read.spiralColourDefault) || !pair("sky_pitch", read.skyPitch) ||
        !pair("planet_pitch", read.planetPitch) || !number("planet_speed", read.planet.speed) ||
        !number("planet_stride", read.planet.stride) || !number("planet_axis_deg", read.planet.axisDeg) ||
        !number("planet_start_angle_rad", startAngle) ||
        !number("planet_update_start_angle_rad", updateStartAngle) ||
        !number("eclipse_pitch_x", read.eclipsePitchX)) {
        return false;
    }
    if (!block["planet_vertical"].IsBool()) {
        error = path + ": space_sky.planet_vertical is missing or not true or false";
        return false;
    }
    read.planet.vertical = block["planet_vertical"].AsBool();
    // The constructor's call is the planet's first, so its start angle is the one
    // that counts; update's is read by no call and is carried only as decoded.
    read.planet.startFrom = startAngle;
    read.planet.startTo = startAngle;
    read.planet.entities = {read.planetName};
    // A spiral of no size is not drawn, and a colour of 0 adds nothing: either
    // would draw a space level with its spiral gone and say nothing.
    if (!(read.spiralDiagonalScale > 0.0) || !(read.spiralColourDefault > 0.0)) {
        error = path + ": space_sky.spiral_diagonal_scale and space_sky.spiral_colour_default are above zero";
        return false;
    }
    out = std::move(read);
    return true;
}

bool BuildSpace(const SpaceRules& rules, const Rules& skyRules, const Motion::Rules& motion,
                const Tscn::Scene& scene, const glm::dvec2& viewPx, double constructorFrameMs, Space& out,
                std::string& error) {
    Space built;
    // Game::preLoop's branch, the other side of Build's: the first `properties`,
    // and space_bg's presence alone.
    for (const Tscn::Node& node : scene.nodes) {
        if (node.parent != "." || Roles::EntityName(node) != skyRules.propertiesEntity) continue;
        built.running = node.Meta(skyRules.spaceBgKey) != nullptr;
        break;
    }
    if (!built.running) {
        out = Space{};
        return true;
    }
    // SeekEntity(name) for each, an exact == on the entity's name. The original
    // walks its buckets and the port the file; no level places two of any.
    const auto seek = [&scene](const std::string& name) -> const Tscn::Node* {
        for (const Tscn::Node& node : scene.nodes) {
            if (node.parent == "." && Roles::EntityName(node) == name) return &node;
        }
        return nullptr;
    };
    if (const Tscn::Node* sky = seek(rules.skyName)) built.skyNode = sky->name;
    if (const Tscn::Node* spiral = seek(rules.spiralName)) {
        built.spiralNode = spiral->name;
        // scaleToSize(vector2(size, size)), size = |screen| x the factor: that side
        // whatever the picture's own size, because Scale multiplies what it has.
        built.spiralSidePx = glm::length(viewPx) * rules.spiralDiagonalScale;
        built.spiralColour = rules.spiralColourDefault;
        if (const Tscn::Value* datum = spiral->Meta(rules.spiralColourKey)) {
            double colour = 0.0;
            if (!datum->AsNumber(colour)) {
                error = "the spiral " + spiral->name + "'s " + rules.spiralColourKey + " is not a number";
                return false;
            }
            built.spiralColour = colour;
        }
        built.spiralTurn.degPerS = rules.spiralTurnDegPerS;
    }
    if (const Tscn::Node* planet = seek(rules.planetName)) {
        built.planetNode = planet->name;
        // SetFloat speed and stride, then linearMotion's first call: the start
        // angle, and one step of the frame the constructor runs on.
        built.planetBob = Motion::Start(rules.planet, rules.planet.startFrom);
        Motion::Advance(motion, built.planetBob, constructorFrameMs);
    }
    if (const Tscn::Node* eclipse = seek(rules.eclipseName)) {
        glm::dvec2 atPx(0.0);
        if (PositionOf(*eclipse, atPx)) {
            built.eclipseNode = eclipse->name;
            built.eclipseYPx = atPx.y;
        }
    }
    out = std::move(built);
    return true;
}

void AdvanceSpace(const Motion::Rules& motion, Space& space, const glm::dvec2& viewCentrePx, const glm::dvec2& viewPx,
                  double frameMs) {
    if (!space.running) return;
    // update's order: the planet, the sky, the spiral, the eclipse. Only the
    // planet's step, the spiral's turn and the eclipse's y carry anything over.
    if (!space.planetNode.empty()) Motion::Advance(motion, space.planetBob, frameMs);
    if (!space.spiralNode.empty()) Motion::Advance(motion, space.spiralTurn, frameMs);
    // #181-218: its own y plus the corner's y, every frame, so under a camera whose
    // corner is not at y 0 it walks off. 4-19, the one level with an eclipse, shows
    // its whole 256 u height, so there the corner's y is 0.
    if (!space.eclipseNode.empty()) space.eclipseYPx += (viewCentrePx - viewPx * 0.5).y;
}

glm::dvec2 SpaceSkyPx(const SpaceRules& rules, const glm::dvec2& viewCentrePx, const glm::dvec2& viewPx) {
    return viewCentrePx - viewPx * 0.5 + viewPx * rules.skyPitch;
}

glm::dvec2 SpiralPx(const SpaceRules& rules, const glm::dvec2& viewCentrePx, const glm::dvec2& viewPx) {
    return viewCentrePx - viewPx * 0.5 + viewPx * rules.spiralPitch;
}

glm::dvec2 PlanetPx(const SpaceRules& rules, const Space& space, const glm::dvec2& viewCentrePx,
                    const glm::dvec2& viewPx) {
    // originalPos rewritten to the pin, then linearMotion puts it there plus its bob.
    return viewCentrePx - viewPx * 0.5 + viewPx * rules.planetPitch + Motion::OffsetPx(space.planetBob);
}

glm::dvec2 EclipsePx(const SpaceRules& rules, const Space& space, const glm::dvec2& viewCentrePx,
                     const glm::dvec2& viewPx) {
    const glm::dvec2 corner = viewCentrePx - viewPx * 0.5;
    return glm::dvec2(corner.x + viewPx.x * rules.eclipsePitchX, space.eclipseYPx);
}

} // namespace MagicPortals::Sky

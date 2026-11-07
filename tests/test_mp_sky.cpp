// The sky the original pins to the camera: StaticSky, as sim/Sky reads it.
//
// What is pinned, and from where:
//
//   the rules    sky.json's names and numbers, each decoded (sky.json's _source):
//                `sky` and `sky.ent`, `satellite`, the `scroll` float, the 0.5
//                pitch, the 200 ms frame cap, and space_bg on `properties`.
//   the centre   a still sky's entity is placed at the camera's corner plus half
//                the view, whatever the level said - so it IS the view's centre,
//                wherever the camera is. The original's frames put it at (640, 360)
//                of 1280 x 720, to within 1 px, on 16 levels of 16 (sky.json's
//                static_sky._measured).
//   the size     view height / picture height, and m_width the last sky's width.
//   the strip    the t-indexed arithmetic of a scrolling strip, its end pivots, and
//                its scroll: moved after the frame's positions, capped at 200 ms a
//                frame, back to 0 once it has run a whole width.
//   space        a level carrying space_bg at all builds no StaticSky, and builds
//                SpaceSky instead (step 122): the space sky at the view's centre,
//                the spiral at its bottom-centre as wide as its diagonal, coloured
//                0.7 or its alpha and turned -1.7 degrees a second, the planet's
//                bob, and the eclipse's y adding the corner's; and the census of
//                the four over the 19 space levels.
//   the census   over the 128 converted levels: 95 one sky, 19 space, 13 neither,
//                one satellite alone; no sky scrolls; every sky's scale is 1.
//   the tier     a sky is drawn from the file the original draws (tiers.json,
//                step 66): the fullhd sky.png, icy_sky.png, red_sky.png and
//                sky_purple.png are 1024 x 512 over 455 x 256 1x files, so 28
//                skies grow to 512 u wide, and stay one screen tall and centred.
//
// The arithmetic is pure and runs anywhere. The census reads the converted levels
// and their art from outside this repository, and is skipped without them, never
// with 77.

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "sim/Motion.hpp"
#include "sim/Roles.hpp"
#include "sim/Sky.hpp"
#include "sim/Sprites.hpp"
#include "sim/Tiers.hpp"
#include "sim/Tscn.hpp"

#include <stb_image.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

using namespace MagicPortals;

namespace {

const std::string kLevels = MAGICPORTALS_LEVELS_DIR;
const std::string kPortData = MAGICPORTALS_PORT_DATA_DIR;

// The port's view at 1280 x 720: 256 px tall, and 256 * 16 / 9 wide.
constexpr double kViewHeight = 256.0;
const glm::dvec2 kView(256.0 * 1280.0 / 720.0, 256.0);

bool Near(const glm::dvec2& a, const glm::dvec2& b, double eps = 1e-9) {
    return std::fabs(a.x - b.x) <= eps && std::fabs(a.y - b.y) <= eps;
}

std::string Px(const glm::dvec2& p) {
    return "(" + std::to_string(p.x) + ", " + std::to_string(p.y) + ")";
}

Tiers::Rules TheTiers() {
    Tiers::Rules rules;
    std::string error;
    CHECK_MSG(Tiers::LoadRules(kPortData + "/tiers.json", rules, error), error);
    return rules;
}

Sky::Rules TheRules() {
    Sky::Rules rules;
    std::string error;
    CHECK_MSG(Sky::LoadRules(kPortData + "/sky.json", rules, error), error);
    return rules;
}

// A top-level entity node, as the converter writes one.
Tscn::Node Entity(const std::string& name, const std::string& entityName, const glm::dvec2& at) {
    Tscn::Node node;
    node.name = name;
    node.type = "Node2D";
    node.parent = ".";
    node.path = name;
    Tscn::Property position;
    position.key = "position";
    position.value.kind = Tscn::Value::Kind::Vector2;
    position.value.numbers = {at.x, at.y};
    node.properties.push_back(position);
    Tscn::Property entity;
    entity.key = "metadata/entity_name";
    entity.value.kind = Tscn::Value::Kind::String;
    entity.value.text = entityName;
    node.properties.push_back(entity);
    return node;
}

void Meta(Tscn::Node& node, const std::string& key, const std::string& text) {
    Tscn::Property property;
    property.key = "metadata/" + key;
    property.value.kind = Tscn::Value::Kind::String;
    property.value.text = text;
    node.properties.push_back(property);
}

Sprites::Sprite Picture(const Tscn::Node& node, const glm::dvec2& sizePx) {
    Sprites::Sprite sprite;
    sprite.node = node.name;
    sprite.sizePx = sizePx;
    sprite.atPx = glm::dvec2(node.Find("position")->numbers[0], node.Find("position")->numbers[1]);
    return sprite;
}

void TheRulesAreSkyJsons() {
    const Sky::Rules rules = TheRules();
    CHECK_EQ(rules.skyNames.size(), static_cast<std::size_t>(2));
    CHECK_MSG(rules.skyNames.size() == 2 && rules.skyNames[0] == "sky" && rules.skyNames[1] == "sky.ent",
              "GetEntityArray(name) and GetEntityArray(name + '.ent'), name 'sky'");
    CHECK(rules.satelliteName == "satellite");
    CHECK(rules.scrollKey == "scroll");
    CHECK(rules.propertiesEntity == "properties");
    CHECK(rules.spaceBgKey == "space_bg");
    CHECK(rules.screenPitch == 0.5);
    CHECK(rules.frameCapMs == 200.0);
    CHECK_MSG(rules.pixelCover, "pixel_cover: a sky short of the frame is drawn to its edge pixels, as the "
                                "original's one sample at each pixel's centre draws it (step 145)");

    // A file that would pin the sky's centre to the view's corner is refused.
    const std::filesystem::path broken = std::filesystem::temp_directory_path() / "supersonic-test-mp-sky.json";
    {
        std::ofstream out(broken, std::ios::binary);
        out << R"({"static_sky": {"properties_entity": "properties", "space_bg_key": "space_bg",
                  "entity_names": ["sky"], "satellite_name": "satellite", "scroll_key": "scroll",
                  "screen_pitch": 0.0, "frame_cap_ms": 200.0}})";
    }
    Sky::Rules refused;
    std::string error;
    CHECK_MSG(!Sky::LoadRules(broken.string(), refused, error) && error.find("screen_pitch") != std::string::npos,
              "a pitch of 0 is refused: " + error);
    // And one that does not say whether a sky is drawn to the frame's edge pixels.
    {
        std::ofstream out(broken, std::ios::binary | std::ios::trunc);
        out << R"({"static_sky": {"properties_entity": "properties", "space_bg_key": "space_bg",
                  "entity_names": ["sky"], "satellite_name": "satellite", "scroll_key": "scroll",
                  "screen_pitch": 0.5, "frame_cap_ms": 200.0}})";
    }
    error.clear();
    CHECK_MSG(!Sky::LoadRules(broken.string(), refused, error) && error.find("pixel_cover") != std::string::npos,
              "a file without pixel_cover is refused: " + error);
    std::error_code ec;
    std::filesystem::remove(broken, ec);
}

// THE PIXEL-CENTRE COVER (sky.json pixel_cover, planning step 145). The original's
// frame is one sample a pixel, at its centre; a sky's cover spans, edge to edge, the
// pixels whose centres the sky covers. dark_sky.png, 455 u at the centre of the
// 455.11 u view of a 1280 px frame, is 0.15625 px short of each side: its cover is
// the whole frame. Wherever the camera is: the view moves, and the frame with it.
void ThePixelCoverIsWhatThePixelCentresDraw() {
    const double perPx = 1280.0 / kView.x; // 2.8125 frame px a level px
    const auto frame = [perPx](double levelPx, double viewLow) { return (levelPx - viewLow) * perPx; };
    for (const double centre : {kView.x * 0.5, 284.4, 1000.0, -40.25}) {
        const double viewLow = centre - kView.x * 0.5;
        const Sky::Span dark = Sky::PixelCover(centre - 227.5, centre + 227.5, viewLow, kView.x, 1280.0);
        CHECK_MSG(std::fabs(frame(centre - 227.5, viewLow) - 0.15625) < 1e-9 &&
                      std::fabs(frame(centre + 227.5, viewLow) - 1279.84375) < 1e-9,
                  "dark_sky.png is 0.15625 px short of each side");
        CHECK_MSG(std::fabs(frame(dark.lowPx, viewLow)) < 1e-9 && std::fabs(frame(dark.highPx, viewLow) - 1280.0) < 1e-9,
                  "its cover is the frame, 0 to 1280 px: " + std::to_string(frame(dark.lowPx, viewLow)) + " to " +
                      std::to_string(frame(dark.highPx, viewLow)) + " with the view at " + std::to_string(centre));
        // A 512 u sky's edges are on boundaries, 80 px past the frame's: they stay.
        const Sky::Span wide = Sky::PixelCover(centre - 256.0, centre + 256.0, viewLow, kView.x, 1280.0);
        CHECK_MSG(std::fabs(frame(wide.lowPx, viewLow) + 80.0) < 1e-9 &&
                      std::fabs(frame(wide.highPx, viewLow) - 1360.0) < 1e-9,
                  "a 512 u sky's span is its own");
    }
    // Each edge by the rule, in frame pixels (a view of 1280 level px, one to one).
    const auto cover = [](double low, double high) { return Sky::PixelCover(low, high, 0.0, 1280.0, 1280.0); };
    const Sky::Span out = cover(0.4, 1279.6);
    CHECK_MSG(out.lowPx == 0.0 && out.highPx == 1280.0, "0.4 px short: out to the edge, whose centre it covers");
    const Sky::Span in = cover(0.6, 1279.4);
    CHECK_MSG(in.lowPx == 1.0 && in.highPx == 1279.0, "0.6 px short: in to the next boundary, the edge pixel's "
                                                      "centre uncovered and the original's pixel not the sky's");
    const Sky::Span on = cover(3.0, 1277.0);
    CHECK_MSG(on.lowPx == 3.0 && on.highPx == 1277.0, "an edge on a boundary stays");
    const Sky::Span half = cover(0.5, 1279.5);
    CHECK_MSG(half.lowPx == 0.0 && half.highPx == 1279.0,
              "a centre exactly on the low edge is drawn, on the high edge is not");
    // What cannot be a span or a view comes back as given.
    const Sky::Span none = Sky::PixelCover(10.0, 10.0, 0.0, 1280.0, 1280.0);
    const Sky::Span noView = Sky::PixelCover(0.4, 1279.6, 0.0, 0.0, 1280.0);
    const Sky::Span noFrame = Sky::PixelCover(0.4, 1279.6, 0.0, 1280.0, 0.0);
    CHECK(none.lowPx == 10.0 && none.highPx == 10.0);
    CHECK(noView.lowPx == 0.4 && noView.highPx == 1279.6);
    CHECK(noFrame.lowPx == 0.4 && noFrame.highPx == 1279.6);
}

void AStillSkyIsTheViewsCentreWhereverTheCameraIs() {
    const Sky::Rules rules = TheRules();
    // level0's sky: the level puts it 2 u right and 8 u up of the view's centre.
    Tscn::Scene scene;
    scene.nodes.push_back(Entity("sky_ent_171", "sky.ent", {229.5, 120.0}));
    const std::vector<Sprites::Sprite> sprites = {Picture(scene.nodes[0], {455.0, 256.0})};
    Sky::Controller sky;
    std::string error;
    CHECK_MSG(Sky::Build(rules, scene, sprites, kViewHeight, sky, error), error);
    CHECK(sky.running);
    CHECK_EQ(sky.skies.size(), static_cast<std::size_t>(1));
    if (sky.skies.size() != 1) return;
    CHECK(!sky.scroll);
    CHECK(sky.skies[0].scale == 1.0);
    CHECK(sky.widthPx == 455.0);
    CHECK(Sky::DrawnSizePx(sky, 0) == glm::dvec2(455.0, 256.0));
    CHECK(!sky.hasSatellite);

    // The view's centre at the level's corner, then scrolled right and down: the
    // sky's entity and its picture's centre are the view's centre every time.
    const glm::dvec2 cameras[] = {kView * 0.5, {540.5, 128.0}, {227.5 + 313.0, 128.0 + 126.0}};
    for (const glm::dvec2& camera : cameras) {
        const glm::dvec2 at = Sky::PositionPx(rules, sky, 0, camera, kView);
        CHECK_MSG(Near(at, camera), "camera " + Px(camera) + ": the sky at " + Px(at));
        CHECK_MSG(Near(Sky::DrawnCentrePx(rules, sky, 0, camera, kView), camera), "and its picture's centre too");
    }
    // At the level's corner that is 1.944 u left of and 8 u below where level0
    // puts it - which is where the original's frame of 1-01 has it.
    const glm::dvec2 moved = Sky::PositionPx(rules, sky, 0, kView * 0.5, kView) - glm::dvec2(229.5, 120.0);
    CHECK_MSG(Near(moved, {256.0 * 1280.0 / 720.0 * 0.5 - 229.5, 8.0}), "moved by " + Px(moved));
    // A still sky has nothing to advance.
    Sky::Advance(rules, sky, 1.0);
    CHECK(sky.skies[0].scrollX == 0.0);
    CHECK_MSG(Near(Sky::PositionPx(rules, sky, 0, kView * 0.5, kView), kView * 0.5), "and has not moved");
}

void TheSkyIsAsTallAsTheView() {
    const Sky::Rules rules = TheRules();
    Tscn::Scene scene;
    scene.nodes.push_back(Entity("sky_1", "sky", {256.0, 128.0}));
    // A 512 x 256 picture (satellite_sky.png) under a view twice the level's
    // height: scale 2, and m_width the scaled width.
    const std::vector<Sprites::Sprite> sprites = {Picture(scene.nodes[0], {512.0, 256.0})};
    Sky::Controller sky;
    std::string error;
    CHECK_MSG(Sky::Build(rules, scene, sprites, 512.0, sky, error), error);
    if (sky.skies.size() != 1) {
        CHECK(false);
        return;
    }
    CHECK(sky.skies[0].scale == 2.0);
    CHECK(sky.widthPx == 1024.0);
    CHECK(Sky::DrawnSizePx(sky, 0) == glm::dvec2(1024.0, 512.0));
}

void ASpaceLevelBuildsNoStaticSky() {
    const Sky::Rules rules = TheRules();
    // readProperties asks only whether space_bg is there: "0" counts as set.
    for (const char* value : {"1", "0"}) {
        Tscn::Scene scene;
        scene.nodes.push_back(Entity("properties_843", "properties", {31.0, 24.0}));
        Meta(scene.nodes[0], "space_bg", value);
        scene.nodes.push_back(Entity("sky_1", "sky", {227.5, 128.0}));
        const std::vector<Sprites::Sprite> sprites = {Picture(scene.nodes[1], {455.0, 256.0})};
        Sky::Controller sky;
        sky.running = true;
        std::string error;
        CHECK_MSG(Sky::Build(rules, scene, sprites, kViewHeight, sky, error), error);
        CHECK_MSG(!sky.running && sky.skies.empty(), std::string("space_bg = ") + value + ": SpaceSky's, not this");
    }
    // Without it, the same scene's sky is StaticSky's.
    Tscn::Scene scene;
    scene.nodes.push_back(Entity("properties_843", "properties", {31.0, 24.0}));
    Meta(scene.nodes[0], "scrolling_sky", "1");
    scene.nodes.push_back(Entity("sky_1", "sky", {227.5, 128.0}));
    const std::vector<Sprites::Sprite> sprites = {Picture(scene.nodes[1], {455.0, 256.0})};
    Sky::Controller sky;
    std::string error;
    CHECK_MSG(Sky::Build(rules, scene, sprites, kViewHeight, sky, error), error);
    CHECK_MSG(sky.running && sky.skies.size() == 1, "a level without space_bg builds StaticSky");
    // scrolling_sky is a different variable from a sky's own scroll, and nothing
    // in the original reads it.
    CHECK_MSG(!sky.scroll, "properties' scrolling_sky does not scroll the sky");
}

void OnlyTheExactNamesAreSkies() {
    const Sky::Rules rules = TheRules();
    Tscn::Scene scene;
    scene.nodes.push_back(Entity("space_sky_2002", "space_sky", {256.0, 128.0}));
    scene.nodes.push_back(Entity("sky_dark_1", "sky_dark.ent", {227.5, 128.0}));
    scene.nodes.push_back(Entity("wall_1", "wall04.ent", {218.0, 6.0}));
    std::vector<Sprites::Sprite> sprites;
    for (const Tscn::Node& node : scene.nodes) sprites.push_back(Picture(node, {455.0, 256.0}));
    Sky::Controller sky;
    std::string error;
    CHECK_MSG(Sky::Build(rules, scene, sprites, kViewHeight, sky, error), error);
    CHECK_MSG(!sky.running && sky.skies.empty(), "space_sky, sky_dark.ent and a wall are not StaticSky's");
}

void ASkyWithoutAPictureIsRefused() {
    const Sky::Rules rules = TheRules();
    Tscn::Scene scene;
    scene.nodes.push_back(Entity("sky_1", "sky", {227.5, 128.0}));
    Sky::Controller sky;
    std::string error;
    CHECK_MSG(!Sky::Build(rules, scene, {}, kViewHeight, sky, error) && error.find("sky_1") != std::string::npos,
              "a sky with no picture cannot be sized: " + error);
}

void TheSatelliteKeepsItsPlaceOnTheScreen() {
    const Sky::Rules rules = TheRules();
    // level14b's: a satellite and no sky.
    Tscn::Scene scene;
    scene.nodes.push_back(Entity("satellite_1166", "satellite", {140.0, 182.0}));
    const std::vector<Sprites::Sprite> sprites = {Picture(scene.nodes[0], {128.0, 128.0})};
    Sky::Controller sky;
    std::string error;
    CHECK_MSG(Sky::Build(rules, scene, sprites, kViewHeight, sky, error), error);
    CHECK(sky.running && sky.skies.empty());
    CHECK(sky.hasSatellite && sky.satelliteNode == "satellite_1166");
    CHECK(sky.satelliteOriginalPx == glm::dvec2(140.0, 182.0));
    // The camera's corner plus where the level put it.
    CHECK_MSG(Near(Sky::SatellitePx(sky, kView * 0.5, kView), {140.0, 182.0}), "at the level's corner, unmoved");
    const glm::dvec2 camera(800.0, 300.0);
    const glm::dvec2 at = Sky::SatellitePx(sky, camera, kView);
    CHECK_MSG(Near(at, camera - kView * 0.5 + glm::dvec2(140.0, 182.0)), "moved with the camera: " + Px(at));
    // SeekEntity: the first, and not `satellite.ent`.
    Tscn::Scene two;
    two.nodes.push_back(Entity("satellite_ent_1", "satellite.ent", {10.0, 10.0}));
    two.nodes.push_back(Entity("satellite_2", "satellite", {20.0, 20.0}));
    two.nodes.push_back(Entity("satellite_3", "satellite", {30.0, 30.0}));
    Sky::Controller other;
    CHECK_MSG(Sky::Build(rules, two, {}, kViewHeight, other, error), error);
    CHECK_MSG(other.hasSatellite && other.satelliteNode == "satellite_2", "the first entity named exactly satellite");
}

// A three-sky strip, which no level places, as StaticSky would lay it out.
Sky::Controller Strip(const Sky::Rules& rules, const char* scroll) {
    Tscn::Scene scene;
    std::vector<Sprites::Sprite> sprites;
    for (int i = 0; i < 3; ++i) {
        scene.nodes.push_back(Entity("sky_" + std::to_string(i), "sky", {100.0 * i, 0.0}));
        if (scroll != nullptr) Meta(scene.nodes.back(), "scroll", scroll);
    }
    for (const Tscn::Node& node : scene.nodes) sprites.push_back(Picture(node, {455.0, 256.0}));
    Sky::Controller sky;
    std::string error;
    CHECK_MSG(Sky::Build(rules, scene, sprites, kViewHeight, sky, error), error);
    return sky;
}

void AStillStripSitsSideBySide() {
    const Sky::Rules rules = TheRules();
    const Sky::Controller sky = Strip(rules, nullptr);
    CHECK_EQ(sky.skies.size(), static_cast<std::size_t>(3));
    if (sky.skies.size() != 3) return;
    const glm::dvec2 camera(600.0, 200.0);
    for (std::size_t t = 0; t < 3; ++t) {
        // originalPos = view * 0.5 + (m_width * t, 0), from the corner.
        const glm::dvec2 want = camera + glm::dvec2(455.0 * static_cast<double>(t), 0.0);
        CHECK_MSG(Near(Sky::PositionPx(rules, sky, t, camera, kView), want), "still sky " + std::to_string(t));
        CHECK(sky.skies[t].pivotX == 0.0);
    }
}

void AScrollingStripAndItsEnds() {
    const Sky::Rules rules = TheRules();
    Sky::Controller sky = Strip(rules, "60");
    CHECK(sky.scroll);
    CHECK(sky.scrollValue == 60.0);
    if (sky.skies.size() != 3) {
        CHECK(false);
        return;
    }
    // The pivots: + the width on the first, - on the last, none between.
    CHECK(sky.skies[0].pivotX == 455.0);
    CHECK(sky.skies[1].pivotX == 0.0);
    CHECK(sky.skies[2].pivotX == -455.0);
    const glm::dvec2 camera(600.0, 200.0);
    const glm::dvec2 corner = camera - kView * 0.5;
    // position(t): size * 0.5, + size.x on the first, - size.x on the last, + m_width * t.
    const double half = 227.5;
    const double positions[] = {half + 455.0, half + 455.0, half - 455.0 + 910.0};
    const double drawn[] = {half, half + 455.0, half + 910.0};
    for (std::size_t t = 0; t < 3; ++t) {
        const glm::dvec2 at = Sky::PositionPx(rules, sky, t, camera, kView);
        CHECK_MSG(Near(at, corner + glm::dvec2(positions[t], 128.0)), "scrolling sky " + std::to_string(t) + " at " + Px(at));
        // Drawn less its pivot: the three pictures edge to edge from the corner.
        const glm::dvec2 centre = Sky::DrawnCentrePx(rules, sky, t, camera, kView);
        CHECK_MSG(Near(centre, corner + glm::dvec2(drawn[t], 128.0)), "drawn at " + Px(centre));
    }

    // scaledUnitsPerSecond(-60) a tick, every sky alike.
    Sky::Advance(rules, sky, 1.0 / 60.0);
    for (const Sky::Layer& layer : sky.skies) CHECK(std::fabs(layer.scrollX + 1.0) < 1e-12);
    CHECK_MSG(Near(Sky::DrawnCentrePx(rules, sky, 0, camera, kView), corner + glm::dvec2(half - 1.0, 128.0)),
              "and the next frame draws it a unit left");
    // A long frame counts 200 ms, not a second.
    Sky::Advance(rules, sky, 1.0);
    CHECK(std::fabs(sky.skies[0].scrollX + 13.0) < 1e-9);
    // Back to 0 once it has run a whole width: 455 u at 60 u/s.
    Sky::Controller wrap = Strip(rules, "60");
    int ticks = 0;
    double least = 0.0;
    while (ticks < 1000) {
        Sky::Advance(rules, wrap, 1.0 / 60.0);
        ++ticks;
        least = std::min(least, wrap.skies[0].scrollX);
        if (wrap.skies[0].scrollX == 0.0) break;
    }
    CHECK_MSG(ticks == 455 || ticks == 456, "wrapped after " + std::to_string(ticks) + " ticks");
    CHECK_MSG(least > -455.0, "and never drew a whole width off: " + std::to_string(least));
}

void ALoneScrollingSkyIsBothEnds() {
    const Sky::Rules rules = TheRules();
    Tscn::Scene scene;
    scene.nodes.push_back(Entity("sky_1", "sky", {227.5, 128.0}));
    Meta(scene.nodes[0], "scroll", "30");
    const std::vector<Sprites::Sprite> sprites = {Picture(scene.nodes[0], {455.0, 256.0})};
    Sky::Controller sky;
    std::string error;
    CHECK_MSG(Sky::Build(rules, scene, sprites, kViewHeight, sky, error), error);
    if (sky.skies.size() != 1) {
        CHECK(false);
        return;
    }
    // The last SetPivotAdjust wins; the pitch takes both.
    CHECK(sky.skies[0].pivotX == -455.0);
    const glm::dvec2 corner(0.0);
    CHECK(Near(Sky::PositionPx(rules, sky, 0, kView * 0.5, kView), corner + glm::dvec2(227.5, 128.0)));
    CHECK(Near(Sky::DrawnCentrePx(rules, sky, 0, kView * 0.5, kView), corner + glm::dvec2(227.5 + 455.0, 128.0)));
    // m_scroll and m_width are one member each: a still sky after a scrolling one
    // leaves the strip still.
    Tscn::Scene mixed;
    mixed.nodes.push_back(Entity("sky_a", "sky", {0.0, 0.0}));
    Meta(mixed.nodes[0], "scroll", "30");
    mixed.nodes.push_back(Entity("sky_b", "sky.ent", {0.0, 0.0}));
    const std::vector<Sprites::Sprite> two = {Picture(mixed.nodes[0], {455.0, 256.0}),
                                              Picture(mixed.nodes[1], {512.0, 256.0})};
    Sky::Controller last;
    CHECK_MSG(Sky::Build(rules, mixed, two, kViewHeight, last, error), error);
    CHECK_MSG(!last.scroll && last.widthPx == 512.0, "the last sky's scroll and width stand");
    CHECK_MSG(last.skies.size() == 2 && last.skies[0].pivotX == 455.0,
              "though the first took its pivot while it was the one scaled");
}

// A FULLHD SKY (step 66): the level names sky.png, 455 x 256; the original draws
// fullhd/sky.png, 1024 x 512 at density 2, so 512 x 256 units. scaleSky's scale
// is still the view's height over the picture's, 1, and the sky is 512 u wide and
// drawn at the view's centre wherever the camera is: the wider picture changes
// its width and nothing about where it stands.
void AFullhdSkyIsWiderAndStillCentred() {
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "supersonic-test-mp-sky";
    std::error_code ec;
    std::filesystem::create_directories(dir / "assets" / "entities" / "fullhd", ec);
    const auto png = [](const std::filesystem::path& path, uint32_t width, uint32_t height) {
        std::vector<unsigned char> bytes = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A, 0, 0, 0, 13, 'I', 'H', 'D', 'R'};
        for (const uint32_t v : {width, height}) {
            for (int shift = 24; shift >= 0; shift -= 8) bytes.push_back(static_cast<unsigned char>(v >> shift));
        }
        bytes.push_back(8);
        bytes.push_back(6);
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    };
    png(dir / "assets" / "entities" / "sky.png", 455, 256);
    png(dir / "assets" / "entities" / "fullhd" / "sky.png", 1024, 512);

    Tscn::Scene scene;
    std::string error;
    CHECK_MSG(Tscn::Parse(R"([gd_scene format=3]

[ext_resource type="Texture2D" path="res://assets/entities/sky.png" id="tex_0"]

[node name="level" type="Node2D"]

[node name="sky_629" type="Node2D" parent="."]
position = Vector2(227.5, 128)
z_index = -100
metadata/entity_name = "sky"

[node name="Sprite" type="Sprite2D" parent="sky_629"]
texture = ExtResource("tex_0")
)",
                          scene, error),
              error);
    const Sky::Rules rules = TheRules();
    for (const bool withTiers : {true, false}) {
        std::vector<Sprites::Sprite> sprites;
        const bool found = Sprites::Find(scene, dir.string(), withTiers ? TheTiers() : Tiers::Rules{}, sprites, error);
        CHECK_MSG(found && sprites.size() == 1, error);
        if (!found || sprites.size() != 1) return;
        Sky::Controller sky;
        CHECK_MSG(Sky::Build(rules, scene, sprites, kViewHeight, sky, error), error);
        if (sky.skies.size() != 1) {
            CHECK(false);
            return;
        }
        const double width = withTiers ? 512.0 : 455.0;
        CHECK_MSG(sky.skies[0].imagePx == glm::dvec2(width, 256.0) && sky.skies[0].scale == 1.0 &&
                      sky.widthPx == width && Sky::DrawnSizePx(sky, 0) == glm::dvec2(width, 256.0),
                  std::string(withTiers ? "the fullhd sky" : "the 1x sky") + " is " + std::to_string(width) +
                      " x 256 u at scale 1, drawn " + Px(Sky::DrawnSizePx(sky, 0)));
        for (const glm::dvec2& camera : {kView * 0.5, glm::dvec2(1000.0, 300.0), glm::dvec2(-40.0, 612.5)}) {
            CHECK_MSG(Near(Sky::DrawnCentrePx(rules, sky, 0, camera, kView), camera),
                      "and its centre is the view's: " + Px(Sky::DrawnCentrePx(rules, sky, 0, camera, kView)) +
                          " for " + Px(camera));
        }
    }
}

// ---- SpaceSky (planning step 122) -------------------------------------------
//
// The 19 space levels' sky, sky.json space_sky: the space sky at the view's
// centre at its own size; the spiral at the bottom-centre, a square as wide as
// the view's diagonal, coloured 0.7 (or its `alpha`) and turned 1.7 degrees a
// second clockwise; the planet at the bottom-centre, bobbing 1 u from PI at 0.5
// radians a second; the eclipse at the view's centre across and its own y plus
// the corner's y down. The original's frames put the spiral's centre, side and
// turn, the sky's centre and the planet's centre where these say (step 122).

Motion::Rules TheMotions() {
    Motion::Rules rules;
    std::string error;
    CHECK_MSG(Motion::LoadRules(kPortData + "/motions.json", rules, error), error);
    return rules;
}

Sky::SpaceRules TheSpaceRules() {
    Sky::SpaceRules rules;
    std::string error;
    CHECK_MSG(Sky::LoadSpaceRules(kPortData + "/sky.json", rules, error), error);
    return rules;
}

// A space level's four, where 4-19 (level18c) puts them, under its properties.
Tscn::Scene SpaceScene(bool withAlpha) {
    Tscn::Scene scene;
    scene.nodes.push_back(Entity("properties_1", "properties", {31.0, 24.0}));
    Meta(scene.nodes[0], "space_bg", "1");
    scene.nodes.push_back(Entity("spiral_2062", "spiral", {256.0, 256.0}));
    if (withAlpha) Meta(scene.nodes[1], "alpha", "0.6");
    scene.nodes.push_back(Entity("space_sky_2061", "space_sky", {256.0, 128.0}));
    scene.nodes.push_back(Entity("planet_bg_ent_2074", "planet_bg.ent", {256.0, 256.0}));
    scene.nodes.push_back(Entity("eclipse_ent_2583", "eclipse.ent", {256.0, 320.0}));
    return scene;
}

void TheSpaceRulesAreSkyJsons() {
    const Sky::SpaceRules rules = TheSpaceRules();
    CHECK(rules.planetName == "planet_bg.ent");
    CHECK(rules.spiralName == "spiral");
    CHECK(rules.skyName == "space_sky");
    CHECK(rules.eclipseName == "eclipse.ent");
    CHECK(rules.spiralDiagonalScale == 1.0);
    CHECK(rules.spiralPitch == glm::dvec2(0.5, 1.0));
    CHECK(rules.spiralTurnDegPerS == -1.7);
    CHECK(rules.spiralColourKey == "alpha");
    CHECK(rules.spiralColourDefault == 0.7);
    CHECK_MSG(rules.skyPitch == glm::dvec2(0.5, 0.5), "V2_HALF");
    CHECK(rules.planetPitch == glm::dvec2(0.5, 1.0));
    CHECK(rules.planet.speed == 0.5 && rules.planet.stride == 1.0 && rules.planet.vertical &&
          rules.planet.axisDeg == 0.0);
    CHECK_MSG(rules.planet.startFrom == static_cast<double>(3.141592654f) &&
                  rules.planet.startTo == rules.planet.startFrom,
              "the constructor's start angle is the float PI");
    CHECK(rules.eclipsePitchX == 0.5);

    // A file with no space_sky block, and one whose spiral would have no size, are
    // refused - and StaticSky's own loader still reads a file that has none.
    const std::filesystem::path broken = std::filesystem::temp_directory_path() / "supersonic-test-mp-space.json";
    std::string error;
    Sky::SpaceRules refused;
    {
        std::ofstream out(broken, std::ios::binary);
        out << R"({"static_sky": {}})";
    }
    CHECK_MSG(!Sky::LoadSpaceRules(broken.string(), refused, error) && error.find("space_sky") != std::string::npos,
              "no space_sky block: " + error);
    {
        std::ifstream in(kPortData + "/sky.json", std::ios::binary);
        std::ostringstream text;
        text << in.rdbuf();
        std::string content = text.str();
        const std::string key = "\"spiral_diagonal_scale\": 1.0";
        const std::size_t at = content.find(key);
        CHECK(at != std::string::npos);
        if (at != std::string::npos) content.replace(at, key.size(), "\"spiral_diagonal_scale\": 0.0");
        std::ofstream out(broken, std::ios::binary | std::ios::trunc);
        out << content;
    }
    error.clear();
    CHECK_MSG(!Sky::LoadSpaceRules(broken.string(), refused, error) &&
                  error.find("spiral_diagonal_scale") != std::string::npos,
              "a spiral of no size is refused: " + error);
    std::error_code ec;
    std::filesystem::remove(broken, ec);
}

void ASpaceLevelBuildsSpaceSky() {
    const Sky::SpaceRules rules = TheSpaceRules();
    const Motion::Rules motion = TheMotions();
    const double tickMs = 1000.0 / 60.0;
    for (const bool withAlpha : {false, true}) {
        const Tscn::Scene scene = SpaceScene(withAlpha);
        Sky::Space space;
        std::string error;
        CHECK_MSG(Sky::BuildSpace(rules, TheRules(), motion, scene, kView, tickMs, space, error), error);
        CHECK(space.running);
        CHECK(space.skyNode == "space_sky_2061");
        CHECK(space.spiralNode == "spiral_2062");
        CHECK(space.planetNode == "planet_bg_ent_2074");
        CHECK(space.eclipseNode == "eclipse_ent_2583");
        // scaleToSize(|screen|): 522.2 u at 16:9, whatever the picture.
        CHECK_MSG(std::fabs(space.spiralSidePx - std::sqrt(kView.x * kView.x + kView.y * kView.y)) < 1e-9 &&
                      std::fabs(space.spiralSidePx - 522.2) < 0.05,
                  "the spiral is the view's diagonal: " + std::to_string(space.spiralSidePx));
        CHECK_MSG(space.spiralColour == (withAlpha ? 0.6 : 0.7),
                  "SetColor(c, c, c), c = its alpha or 0.7: " + std::to_string(space.spiralColour));
        CHECK(space.spiralTurn.degPerS == -1.7 && space.spiralTurn.turnedDeg == 0.0);
        // The constructor's linearMotion call: PI, then one step of its frame.
        CHECK_MSG(space.planetBob.started &&
                      std::fabs(space.planetBob.angle - (static_cast<double>(3.141592654f) + 0.5 * tickMs / 1000.0)) <
                          1e-12,
                  "the planet's angle after the constructor: " + std::to_string(space.planetBob.angle));
        CHECK(space.eclipseYPx == 320.0);
    }
    // The space levels' diagonal on another screen: 4:3, and a view twice as tall.
    {
        Sky::Space space;
        std::string error;
        CHECK(Sky::BuildSpace(rules, TheRules(), motion, SpaceScene(false), {341.0 + 1.0 / 3.0, 256.0}, tickMs,
                              space, error));
        CHECK_MSG(std::fabs(space.spiralSidePx - (341.0 + 1.0 / 3.0) * 1.25) < 1e-9,
                  "4:3: " + std::to_string(space.spiralSidePx));
    }
    // Without space_bg nothing is SpaceSky's; "0" counts as set, as readProperties asks.
    {
        Tscn::Scene scene = SpaceScene(false);
        scene.nodes[0] = Entity("properties_1", "properties", {31.0, 24.0});
        Sky::Space space;
        space.running = true;
        std::string error;
        CHECK(Sky::BuildSpace(rules, TheRules(), motion, scene, kView, tickMs, space, error));
        CHECK_MSG(!space.running && space.spiralNode.empty(), "no space_bg, no SpaceSky");
        Meta(scene.nodes[0], "space_bg", "0");
        CHECK(Sky::BuildSpace(rules, TheRules(), motion, scene, kView, tickMs, space, error));
        CHECK_MSG(space.running, "space_bg = 0 is still set");
    }
    // Each is sought by its exact name: a spiral.ent is not `spiral`, and a level
    // missing any of the four builds the rest.
    {
        Tscn::Scene scene;
        scene.nodes.push_back(Entity("properties_1", "properties", {31.0, 24.0}));
        Meta(scene.nodes[0], "space_bg", "1");
        scene.nodes.push_back(Entity("spiral_1", "spiral.ent", {256.0, 256.0}));
        scene.nodes.push_back(Entity("planet_1", "planet_bg", {256.0, 256.0}));
        scene.nodes.push_back(Entity("space_sky_1", "space_sky", {256.0, 128.0}));
        Sky::Space space;
        std::string error;
        CHECK(Sky::BuildSpace(rules, TheRules(), motion, scene, kView, tickMs, space, error));
        CHECK_MSG(space.running && space.skyNode == "space_sky_1" && space.spiralNode.empty() &&
                      space.planetNode.empty() && space.eclipseNode.empty(),
                  "only space_sky is found");
    }
    // A spiral whose alpha is no number is refused, naming it.
    {
        Tscn::Scene scene = SpaceScene(false);
        Meta(scene.nodes[1], "alpha", "dim");
        Sky::Space space;
        std::string error;
        CHECK_MSG(!Sky::BuildSpace(rules, TheRules(), motion, scene, kView, tickMs, space, error) &&
                      error.find("spiral_2062") != std::string::npos,
                  "an alpha that is not a number: " + error);
    }
}

void SpaceSkyIsPinnedToTheCamera() {
    const Sky::SpaceRules rules = TheSpaceRules();
    const Motion::Rules motion = TheMotions();
    Sky::Space space;
    std::string error;
    CHECK(Sky::BuildSpace(rules, TheRules(), motion, SpaceScene(false), kView, 1000.0 / 60.0, space, error));
    // The view's centre at the level's corner, then panned right: every point moves
    // with it, whatever the level said.
    for (const glm::dvec2& camera : {kView * 0.5, glm::dvec2(540.5, 128.0), glm::dvec2(700.0, 128.0)}) {
        const glm::dvec2 bottom = camera + glm::dvec2(0.0, kView.y * 0.5);
        CHECK_MSG(Near(Sky::SpaceSkyPx(rules, camera, kView), camera), "the space sky at the view's centre");
        CHECK_MSG(Near(Sky::SpiralPx(rules, camera, kView), bottom),
                  "the spiral at the bottom-centre: " + Px(Sky::SpiralPx(rules, camera, kView)));
        const glm::dvec2 planet = Sky::PlanetPx(rules, space, camera, kView);
        CHECK_MSG(Near(planet, bottom + glm::dvec2(0.0, std::cos(space.planetBob.angle))),
                  "the planet at the bottom-centre plus its bob: " + Px(planet));
        const glm::dvec2 eclipse = Sky::EclipsePx(rules, space, camera, kView);
        CHECK_MSG(Near(eclipse, {camera.x, 320.0}), "the eclipse across the centre, at its y: " + Px(eclipse));
    }
}

void TheSpiralTurnsAndThePlanetBobs() {
    const Sky::SpaceRules rules = TheSpaceRules();
    const Motion::Rules motion = TheMotions();
    const double tickMs = 1000.0 / 60.0;
    Sky::Space space;
    std::string error;
    CHECK(Sky::BuildSpace(rules, TheRules(), motion, SpaceScene(false), kView, tickMs, space, error));
    const double start = space.planetBob.angle;
    // Seven seconds of ticks: -11.9 degrees, and the planet 3.5 radians on.
    for (int tick = 0; tick < 420; ++tick) Sky::AdvanceSpace(motion, space, kView * 0.5, kView, tickMs);
    CHECK_MSG(std::fabs(space.spiralTurn.turnedDeg - (-1.7 * 7.0)) < 1e-9,
              "the spiral turned " + std::to_string(space.spiralTurn.turnedDeg) + " degrees");
    CHECK_MSG(std::fabs(space.planetBob.angle - (start + 0.5 * 7.0 - motion.wrapRad)) < 1e-9,
              "the planet's angle, a turn taken off: " + std::to_string(space.planetBob.angle));
    const double bob = Motion::OffsetPx(space.planetBob).y;
    CHECK_MSG(std::fabs(bob - std::cos(space.planetBob.angle)) < 1e-12 && std::fabs(bob) <= 1.0,
              "the bob is cos(angle) x 1 u: " + std::to_string(bob));
    // unitsPerSecond's cap: a frame of a second turns it as 200 ms does; a frame of
    // 0 - the original's m_factor 0 - turns nothing.
    const double turned = space.spiralTurn.turnedDeg;
    Sky::AdvanceSpace(motion, space, kView * 0.5, kView, 1000.0);
    CHECK_MSG(std::fabs(space.spiralTurn.turnedDeg - (turned - 1.7 * 0.2)) < 1e-9, "capped at 200 ms");
    const double held = space.spiralTurn.turnedDeg;
    const double heldAngle = space.planetBob.angle;
    Sky::AdvanceSpace(motion, space, kView * 0.5, kView, 0.0);
    CHECK(space.spiralTurn.turnedDeg == held && space.planetBob.angle == heldAngle);
}

void TheEclipseAddsTheCornersY() {
    const Sky::SpaceRules rules = TheSpaceRules();
    const Motion::Rules motion = TheMotions();
    Sky::Space space;
    std::string error;
    CHECK(Sky::BuildSpace(rules, TheRules(), motion, SpaceScene(false), kView, 1000.0 / 60.0, space, error));
    // 4-19's camera: the view is the level's height, so the corner's y is 0 and the
    // eclipse holds at 320 however long it runs.
    for (int tick = 0; tick < 600; ++tick) Sky::AdvanceSpace(motion, space, {600.0, 128.0}, kView, 1000.0 / 60.0);
    CHECK_MSG(space.eclipseYPx == 320.0, "a corner at y 0 leaves it: " + std::to_string(space.eclipseYPx));
    // A corner 5 u down adds 5 every frame: its own y plus the camera's, as
    // update #181-218 writes it.
    for (int tick = 0; tick < 3; ++tick) Sky::AdvanceSpace(motion, space, {600.0, 133.0}, kView, 1000.0 / 60.0);
    CHECK_MSG(space.eclipseYPx == 335.0, "three frames at a corner 5 down: " + std::to_string(space.eclipseYPx));
    // A level with no SpaceSky advances nothing.
    Sky::Space none;
    Sky::AdvanceSpace(motion, none, {600.0, 133.0}, kView, 1000.0 / 60.0);
    CHECK(none.eclipseYPx == 0.0 && none.spiralTurn.turnedDeg == 0.0);
}

// ---- the converted levels ---------------------------------------------------

void EveryLevelsSky() {
    const Sky::Rules rules = TheRules();
    const Tiers::Rules tiers = TheTiers();
    int one = 0, space = 0, neither = 0, satelliteAlone = 0, levels = 0, named = 0, namedEnt = 0;
    int wide = 0, narrow = 0, otherWidth = 0; // skies of 512 u (fullhd), 455 u (1x), and any other
    // Of the skies narrower than the view, those whose pixel cover is the 1280 px frame
    // edge to edge (sky.json pixel_cover, step 145), and those that are not.
    int coveredToTheFrame = 0, coveredOtherwise = 0;
    int scrolling = 0, scaledOtherThanOne = 0, notTheViewsCentre = 0, scrollingSkyKey = 0;
    std::string firstWrong;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(kLevels, ec)) {
        if (entry.path().extension() != ".tscn") continue;
        ++levels;
        const std::string level = entry.path().stem().string();
        Tscn::Scene scene;
        std::vector<Sprites::Sprite> sprites;
        std::string error;
        if (!Tscn::Load(entry.path().string(), scene, error) || !Sprites::Find(scene, kLevels + "/..", tiers, sprites, error)) {
            CHECK_MSG(false, level + ": " + error);
            continue;
        }
        Sky::Controller sky;
        CHECK_MSG(Sky::Build(rules, scene, sprites, kViewHeight, sky, error), level + ": " + error);
        for (const Tscn::Node& node : scene.nodes) {
            if (node.parent != ".") continue;
            if (Roles::EntityName(node) == "sky") ++named;
            if (Roles::EntityName(node) == "sky.ent") ++namedEnt;
            if (node.Meta("scrolling_sky") != nullptr) ++scrollingSkyKey;
        }
        bool spaceBg = false;
        for (const Tscn::Node& node : scene.nodes) {
            if (node.parent == "." && Roles::EntityName(node) == "properties" && node.Meta("space_bg") != nullptr) {
                spaceBg = true;
            }
        }
        if (spaceBg) {
            ++space;
            CHECK_MSG(!sky.running, level + ": a space level builds no StaticSky");
            continue;
        }
        if (sky.skies.size() == 1) ++one;
        if (sky.skies.empty() && sky.hasSatellite) ++satelliteAlone;
        if (!sky.running) ++neither;
        CHECK_MSG(sky.skies.size() <= 1, level + ": " + std::to_string(sky.skies.size()) + " skies");
        if (sky.scroll) ++scrolling;
        for (std::size_t t = 0; t < sky.skies.size(); ++t) {
            if (sky.skies[t].scale != 1.0) ++scaledOtherThanOne;
            if (sky.skies[t].imagePx == glm::dvec2(512.0, 256.0)) {
                ++wide;
            } else if (sky.skies[t].imagePx == glm::dvec2(455.0, 256.0)) {
                ++narrow;
            } else {
                ++otherWidth;
            }
            const glm::dvec2 camera(500.0, 300.0);
            if (!Near(Sky::DrawnCentrePx(rules, sky, t, camera, kView), camera)) {
                ++notTheViewsCentre;
                if (firstWrong.empty()) firstWrong = level;
            }
            const double width = Sky::DrawnSizePx(sky, t).x;
            if (width < kView.x) {
                const double centre = Sky::DrawnCentrePx(rules, sky, t, camera, kView).x;
                const double viewLow = camera.x - kView.x * 0.5;
                const Sky::Span cover =
                    Sky::PixelCover(centre - width * 0.5, centre + width * 0.5, viewLow, kView.x, 1280.0);
                const double perPx = 1280.0 / kView.x;
                if (std::fabs((cover.lowPx - viewLow) * perPx) < 1e-9 &&
                    std::fabs((cover.highPx - viewLow) * perPx - 1280.0) < 1e-9) {
                    ++coveredToTheFrame;
                } else {
                    ++coveredOtherwise;
                }
            }
        }
    }
    std::printf("  %d levels: %d one sky (%d 'sky', %d 'sky.ent'), %d space, %d neither, %d satellite alone; "
                "%d scrolling_sky keys, %d scrolling\n",
                levels, one, named, namedEnt, space, neither, satelliteAlone, scrollingSkyKey, scrolling);
    CHECK_EQ(levels, 128);
    CHECK_EQ(one, 95);
    CHECK_EQ(named, 93);
    CHECK_EQ(namedEnt, 2);
    CHECK_EQ(space, 19);
    CHECK_EQ(neither, 13);
    CHECK_EQ(satelliteAlone, 1);
    // The key 37 levels carry, and no sky that scrolls.
    CHECK_EQ(scrollingSkyKey, 37);
    CHECK_EQ(scrolling, 0);
    CHECK_EQ(scaledOtherThanOne, 0);
    // Drawn from its tier. 51 skies are 512 u wide: the 28 whose 455 x 256 file
    // has a 1024 x 512 fullhd copy (sky 3, red_sky 7, icy_sky 14, sky_purple 4),
    // wider than their 1x files, and the 23 satellite_sky.png, whose 512 x 256
    // file is already that wide and whose fullhd copy is exactly twice it. The
    // other 44 are dark_sky.png, with no tier file, 455 u.
    std::printf("  skies by drawn width: %d at 512 u, %d at 455 u, %d other\n", wide, narrow, otherWidth);
    CHECK_EQ(wide, 51);
    CHECK_EQ(narrow, 44);
    CHECK_EQ(otherWidth, 0);
    // The 44 are the skies narrower than the view, and each one's cover is the frame.
    std::printf("  skies narrower than the view: %d covered to the 1280 px frame's edges, %d otherwise\n",
                coveredToTheFrame, coveredOtherwise);
    CHECK_EQ(coveredToTheFrame, 44);
    CHECK_EQ(coveredOtherwise, 0);
    CHECK_MSG(notTheViewsCentre == 0, "every sky at the view's centre, first wrong " + firstWrong);
}

// SpaceSky over the 128 converted levels: the 19 that set space_bg (level0c to
// level18c, 4-01 to 4-19) build it, and nothing else does. Each of the 19 places
// one space sky and one spiral; 18 a planet (all but 4-01); one an eclipse (4-19);
// and one spiral carries `alpha`, 4-01's 0.6.
void EveryLevelsSpaceSky() {
    const Sky::SpaceRules rules = TheSpaceRules();
    const Sky::Rules skyRules = TheRules();
    const Motion::Rules motion = TheMotions();
    int levels = 0, running = 0, skies = 0, spirals = 0, planets = 0, eclipses = 0, tinted = 0;
    std::string spaceLevels, tintedAt, eclipseAt, noPlanet;
    std::error_code ec;
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(kLevels, ec)) {
        if (entry.path().extension() == ".tscn") files.push_back(entry.path());
    }
    std::sort(files.begin(), files.end());
    for (const std::filesystem::path& file : files) {
        ++levels;
        const std::string level = file.stem().string();
        Tscn::Scene scene;
        std::string error;
        if (!Tscn::Load(file.string(), scene, error)) {
            CHECK_MSG(false, level + ": " + error);
            continue;
        }
        Sky::Space space;
        CHECK_MSG(Sky::BuildSpace(rules, skyRules, motion, scene, kView, 1000.0 / 60.0, space, error),
                  level + ": " + error);
        if (!space.running) continue;
        ++running;
        spaceLevels += level + " ";
        if (!space.skyNode.empty()) ++skies;
        if (!space.spiralNode.empty()) ++spirals;
        if (!space.planetNode.empty()) {
            ++planets;
        } else {
            noPlanet += level;
        }
        if (!space.eclipseNode.empty()) {
            ++eclipses;
            eclipseAt += level;
        }
        if (space.spiralColour != rules.spiralColourDefault) {
            ++tinted;
            tintedAt += level + " " + std::to_string(space.spiralColour);
        }
    }
    std::printf("  %d levels: %d SpaceSky (%s); %d space skies, %d spirals, %d planets (none on %s), %d eclipse (%s); "
                "%d spiral tinted (%s)\n",
                levels, running, spaceLevels.c_str(), skies, spirals, planets, noPlanet.c_str(), eclipses,
                eclipseAt.c_str(), tinted, tintedAt.c_str());
    CHECK_EQ(levels, 128);
    CHECK_EQ(running, 19);
    for (int i = 0; i <= 18; ++i) {
        const std::string name = "level" + std::to_string(i) + "c ";
        CHECK_MSG(spaceLevels.find(name) != std::string::npos, name + "builds SpaceSky");
    }
    CHECK_EQ(skies, 19);
    CHECK_EQ(spirals, 19);
    CHECK_EQ(planets, 18);
    CHECK(noPlanet == "level0c");
    CHECK_EQ(eclipses, 1);
    CHECK(eclipseAt == "level18c");
    CHECK_EQ(tinted, 1);
    CHECK_MSG(tintedAt.rfind("level0c 0.6", 0) == 0, "4-01's spiral at 0.6: " + tintedAt);
}

// ---- THE SKY ON A WINDOW WIDER THAN ITS CONTENT (sky.json wide_view; ENHANCED'S OWN) -------------------------------
//
// scaleSky sizes a sky by the screen's height alone (listing 30116-30225, #35-39 and #41-50), so on a window wider than
// 16:9 the picture's CONTENT - 455 u in every sky there is, the fullhd ones padded to 512 u with black columns - ends
// short of the view's sides and the black clear shows there wherever a level's art has an opening. The original does the
// same on such a screen (its app.enml has no fixedWidth or fixedHeight outside ios, so the backbuffer is the screen); the
// port grows the sky. What is pinned: the content covers the view at every window shape, centred, with the overscan to
// spare, and 16:9 and every narrower window are the original's frame, scale 1 and shift 0, to the bit.

// What a view `viewW` wide sees of a picture, as the layer draws it: the content's edges from the view's centre
// (a still sky's quad is centred on it), and the quad's half height. The fit is the code's; the content is the caller's.
struct WideSeen {
    double scale = 1.0;
    double shiftX = 0.0;
    double contentLo = 0.0;
    double contentHi = 0.0;
    double halfHeight = 0.0;
};

WideSeen WideSeenBy(const Sky::Rules& rules, const Sky::Layer& layer, double fromUnits, double toUnits, double viewW) {
    const Sky::Fit fit = Sky::WideFit(rules, layer, viewW);
    const double quadW = layer.imagePx.x * layer.scale;
    WideSeen seen;
    seen.scale = fit.scale;
    seen.shiftX = fit.shiftXPx;
    seen.contentLo = fit.shiftXPx + (fromUnits * layer.scale - quadW * 0.5) * fit.scale;
    seen.contentHi = fit.shiftXPx + (toUnits * layer.scale - quadW * 0.5) * fit.scale;
    seen.halfHeight = layer.imagePx.y * layer.scale * 0.5 * fit.scale;
    return seen;
}

// Window shapes: 4:3, 16:10, 16:9, 456 u of view (the tolerance's edge for a 455 u content), 2:1, the tester's
// 2340 x 1080, 20:9, 21:9 and 32:9.
constexpr double kWideAspects[] = {4.0 / 3.0, 16.0 / 10.0, 16.0 / 9.0, 57.0 / 32.0, 2.0, 2340.0 / 1080.0, 20.0 / 9.0, 21.0 / 9.0, 32.0 / 9.0};

// Pure arithmetic over the shapes the pictures have, and over two they do not: with margins that are not equal, a shift
// that is right only for a centred content (a * (1 - scale) instead of -a * scale) passes the first three and fails the last two.
void AWideViewIsCoveredByTheSkysContent() {
    const Sky::Rules rules = TheRules();
    CHECK_MSG(rules.wide.enabled, "wide_view is on: a sky is grown on a window wider than its content");
    struct Shape {
        const char* what;
        double width, from, to;
    };
    const Shape shapes[] = {{"a fullhd sky, 455 u in 512", 512.0, 28.5, 483.5},
                            {"dark_sky.png", 455.0, 0.0, 455.0},
                            {"satellite_sky.png", 512.0, 0.0, 512.0},
                            {"content 10 to 483.5 of 512", 512.0, 10.0, 483.5},
                            {"content 40 to 500 of 512", 512.0, 40.0, 500.0}};
    for (const Shape& shape : shapes) {
        Sky::Layer layer;
        layer.imagePx = glm::dvec2(shape.width, 256.0);
        layer.scale = 1.0;
        layer.contentFromPx = shape.from;
        layer.contentToPx = shape.to;
        for (const double aspect : kWideAspects) {
            const double viewW = 256.0 * aspect;
            const WideSeen seen = WideSeenBy(rules, layer, shape.from, shape.to, viewW);
            const std::string where = std::string(shape.what) + " at " + std::to_string(aspect) + ":1 (" + std::to_string(viewW) + " u)";
            if (viewW > shape.to - shape.from + rules.wide.toleranceUnits) {
                CHECK_MSG(seen.scale > 1.0, where + ": grown");
                CHECK_MSG(seen.contentLo <= -viewW * 0.5 - rules.wide.overscanUnits + 1e-9 &&
                              seen.contentHi >= viewW * 0.5 + rules.wide.overscanUnits - 1e-9,
                          where + ": the content covers the view with the overscan to spare: " + std::to_string(seen.contentLo) + " to " +
                              std::to_string(seen.contentHi));
                CHECK_MSG(std::fabs(seen.contentLo + seen.contentHi) < 1e-9, where + ": and is centred on it");
                CHECK_MSG(seen.halfHeight >= 128.0, where + ": the quad is at least one view tall");
            } else {
                CHECK_MSG(seen.scale == 1.0 && seen.shiftX == 0.0, where + ": the original's frame, scale 1 and no shift");
            }
        }
    }
    // The tolerance's own edge: 456 u of view over 455 of content is not grown, a hair more is.
    Sky::Layer edge;
    edge.imagePx = glm::dvec2(512.0, 256.0);
    edge.contentFromPx = 28.5;
    edge.contentToPx = 483.5;
    CHECK_MSG(Sky::WideFit(rules, edge, 455.0 + rules.wide.toleranceUnits).scale == 1.0, "at the tolerance: the original's frame");
    CHECK_MSG(Sky::WideFit(rules, edge, 455.0 + rules.wide.toleranceUnits + 1e-6).scale > 1.0, "a hair past it: grown");
    // 16:9 itself (455.11 u over 455) stays the original's.
    CHECK_MSG(Sky::WideFit(rules, edge, 256.0 * 16.0 / 9.0).scale == 1.0, "16:9 is the original's frame");
    // The numbers themselves, not only that the growth covers (every check above is a lower bound, and a growth 1.5 times too large passes them):
    // the rule's own two values, and the tester's phone: 2340 x 1080 is 554.667 u across, so the content (455 u) is grown to 554.667 + 2 x 0.5.
    CHECK_MSG(rules.wide.toleranceUnits == 1.0 && rules.wide.overscanUnits == 0.5, "sky.json's wide_view: tolerance 1 u, overscan 0.5 u");
    const double phoneView = 256.0 * 2340.0 / 1080.0;
    const Sky::Fit phone = Sky::WideFit(rules, edge, phoneView);
    CHECK_MSG(std::fabs(phone.scale - (phoneView + 1.0) / 455.0) < 1e-9 && std::fabs(phone.scale - 1.221246) < 1e-6,
              "the tester's 2340 x 1080 grows the sky 1.2212 times: " + std::to_string(phone.scale));
    CHECK_MSG(std::fabs(1.0 / phone.scale - 0.8188) < 1e-3, "which shows 81.9 percent of its height");
    CHECK_MSG(std::fabs((phoneView - 455.0) * 0.5 - 49.83) < 1e-2 && std::fabs((phoneView - 455.0) * 0.5 / phoneView - 0.0898) < 1e-4,
              "of which the original's frame left 49.83 u, 8.98 percent of the width, bare at each side");
    // Off is the original's frame at every shape.
    Sky::Rules off = rules;
    off.wide.enabled = false;
    for (const double aspect : kWideAspects) {
        const Sky::Fit fit = Sky::WideFit(off, edge, 256.0 * aspect);
        CHECK_MSG(fit.scale == 1.0 && fit.shiftXPx == 0.0, "enabled false draws the original's frame");
    }
    // A picture with no content (a layer Build did not give one) is left alone.
    Sky::Layer none;
    none.imagePx = glm::dvec2(512.0, 256.0);
    CHECK_MSG(Sky::WideFit(rules, none, 600.0).scale == 1.0, "no content, no growth");
}

// The file is strict, as pixel_cover's is.
void AWideViewFileThatDoesNotSayIsRefused() {
    const std::filesystem::path broken = std::filesystem::temp_directory_path() / "supersonic-test-mp-sky-wide.json";
    const auto refused = [&broken](const std::string& wide, const std::string& key) {
        {
            std::ofstream out(broken, std::ios::binary | std::ios::trunc);
            out << R"({"static_sky": {"properties_entity": "properties", "space_bg_key": "space_bg",
                  "entity_names": ["sky"], "satellite_name": "satellite", "scroll_key": "scroll",
                  "screen_pitch": 0.5, "frame_cap_ms": 200.0, "pixel_cover": true)"
                << wide << "}}";
        }
        Sky::Rules read;
        std::string error;
        const bool loaded = Sky::LoadRules(broken.string(), read, error);
        CHECK_MSG(!loaded && error.find(key) != std::string::npos, key + " is refused: " + error);
    };
    refused("", "wide_view");
    refused(R"(, "wide_view": {"enabled": true, "tolerance_units": -1.0, "overscan_units": 0.5, "pictures": []})", "tolerance_units");
    refused(R"(, "wide_view": {"enabled": true, "tolerance_units": 1.0, "overscan_units": 0.5,
             "pictures": [{"file": "a.png", "tier": "fullhd", "content_units": [30.0, 20.0]}]})", "content_units");
    std::error_code ec;
    std::filesystem::remove(broken, ec);
}

// ---- THE PICTURES' OWN PIXELS (the suite test_mp_sky_levels: it reads the converted levels and their art, and exits 77 without them) ----

struct Columns {
    int w = 0, h = 0, blackLeft = 0, blackRight = 0;
    bool read = false;
};

// The columns at each side of a picture whose every pixel is (0, 0, 0) or has alpha 0: the padding the artist left,
// found from the file the renderer reads, never from sky.json.
Columns MeasureColumns(const std::string& path) {
    Columns found;
    int channels = 0;
    stbi_uc* pixels = stbi_load(path.c_str(), &found.w, &found.h, &channels, 4);
    if (pixels == nullptr) return found;
    const auto black = [&](int x) {
        for (int y = 0; y < found.h; ++y) {
            const stbi_uc* p = pixels + (static_cast<std::size_t>(y) * static_cast<std::size_t>(found.w) + static_cast<std::size_t>(x)) * 4;
            if (p[3] != 0 && (p[0] != 0 || p[1] != 0 || p[2] != 0)) return false;
        }
        return true;
    };
    while (found.blackLeft < found.w && black(found.blackLeft)) ++found.blackLeft;
    if (found.blackLeft < found.w) {
        while (black(found.w - 1 - found.blackRight)) ++found.blackRight;
    }
    found.read = true;
    stbi_image_free(pixels);
    return found;
}

void EverySkyCoversEveryWindowFromItsOwnPixels() {
    const Sky::Rules rules = TheRules();
    const Tiers::Rules tiers = TheTiers();
    Sky::Rules off = rules;
    off.wide.enabled = false;
    std::map<std::string, Columns> measured;  // by the file the level draws
    int skies = 0, bareWithoutTheFix = 0, growing = 0;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(kLevels, ec)) {
        if (entry.path().extension() != ".tscn") continue;
        const std::string level = entry.path().stem().string();
        Tscn::Scene scene;
        std::vector<Sprites::Sprite> sprites;
        std::string error;
        if (!Tscn::Load(entry.path().string(), scene, error) || !Sprites::Find(scene, kLevels + "/..", tiers, sprites, error)) {
            CHECK_MSG(false, level + ": " + error);
            continue;
        }
        Sky::Controller sky;
        CHECK_MSG(Sky::Build(rules, scene, sprites, kViewHeight, sky, error), level + ": " + error);
        for (const Sky::Layer& layer : sky.skies) {
            const auto sprite = std::find_if(sprites.begin(), sprites.end(), [&layer](const Sprites::Sprite& s) { return s.node == layer.node; });
            CHECK_MSG(sprite != sprites.end(), level + ": the sky's picture");
            if (sprite == sprites.end()) continue;
            ++skies;
            auto at = measured.find(sprite->texture);
            if (at == measured.end()) at = measured.emplace(sprite->texture, MeasureColumns(sprite->texture)).first;
            const Columns& columns = at->second;
            CHECK_MSG(columns.read, level + ": " + sprite->texture + " is read");
            if (!columns.read) continue;
            const double fromUnits = static_cast<double>(columns.blackLeft) / static_cast<double>(sprite->density);
            const double toUnits = static_cast<double>(columns.w - columns.blackRight) / static_cast<double>(sprite->density);
            // The table against the picture: what Build took for the content is what the pixels say.
            CHECK_MSG(std::fabs(layer.contentFromPx - fromUnits) < 1e-9 && std::fabs(layer.contentToPx - toUnits) < 1e-9,
                      level + ": " + sprite->texture + "'s content is " + std::to_string(fromUnits) + " to " + std::to_string(toUnits) +
                          " in its pixels, and sky.json has " + std::to_string(layer.contentFromPx) + " to " + std::to_string(layer.contentToPx));
            for (const double aspect : kWideAspects) {
                const double viewW = 256.0 * aspect;
                const WideSeen seen = WideSeenBy(rules, layer, fromUnits, toUnits, viewW);
                if (viewW > toUnits - fromUnits + rules.wide.toleranceUnits) {
                    CHECK_MSG(seen.contentLo <= -viewW * 0.5 + 1e-9 && seen.contentHi >= viewW * 0.5 - 1e-9 && seen.halfHeight >= 128.0 - 1e-9,
                              level + " at " + std::to_string(aspect) + ":1: the sky's content covers the view (" + std::to_string(seen.contentLo) +
                                  " to " + std::to_string(seen.contentHi) + " of +-" + std::to_string(viewW * 0.5) + ")");
                    ++growing;
                } else {
                    CHECK_MSG(seen.scale == 1.0 && seen.shiftX == 0.0, level + " at " + std::to_string(aspect) + ":1: the original's frame");
                }
                if (aspect == 2340.0 / 1080.0) {
                    // The same measurement without the fix finds the strip, on every sky: the check bites.
                    const WideSeen bare = WideSeenBy(off, layer, fromUnits, toUnits, viewW);
                    if (bare.contentLo > -viewW * 0.5 + 1e-9 || bare.contentHi < viewW * 0.5 - 1e-9) ++bareWithoutTheFix;
                }
            }
        }
    }
    CHECK_MSG(skies > 0, "the levels place skies");
    CHECK_MSG(bareWithoutTheFix == skies, "without wide_view every sky leaves a bare strip at 2340 x 1080: " + std::to_string(bareWithoutTheFix) +
                                              " of " + std::to_string(skies));
    CHECK_MSG(growing > 0, "and some are grown");
    // The pictures themselves: the four fullhd skies hold 57 black texels each side (columns 0 to 56 and 967 to 1023), nothing else holds any.
    for (const auto& [file, columns] : measured) {
        const std::string name = std::filesystem::path(file).filename().string();
        const bool padded = file.find("/fullhd/") != std::string::npos &&
                            (name == "sky.png" || name == "icy_sky.png" || name == "red_sky.png" || name == "sky_purple.png");
        CHECK_MSG(columns.blackLeft == (padded ? 57 : 0) && columns.blackRight == (padded ? 57 : 0),
                  file + " has " + std::to_string(columns.blackLeft) + " and " + std::to_string(columns.blackRight) + " black columns");
    }
}

constexpr int kSkippedExit = 77;  // SKIPS_WITH_77, ctest's SKIP_RETURN_CODE (tests/test_mp.cpp)

int skyLevelsEntry(int, char**) {
    std::error_code ec;
    if (!std::filesystem::is_directory(kLevels, ec) || !std::filesystem::is_regular_file(kLevels + "/../assets/entities/dark_sky.png", ec)) {
        std::printf("test_mp_sky_levels: SKIPPED - needs the converted levels and their art at %s (no check was run)\n"
                    "  They live outside this repository; configure with -DSUPERSONIC_MAGICPORTALS_LEVELS=...\n",
                    kLevels.c_str());
        return kSkippedExit;
    }
    EverySkyCoversEveryWindowFromItsOwnPixels();
    return ::test::summary("test_mp_sky_levels", 500);
}

} // namespace

MP_SUITE_MAIN("test_mp_sky") {
    TheRulesAreSkyJsons();
    AStillSkyIsTheViewsCentreWhereverTheCameraIs();
    TheSkyIsAsTallAsTheView();
    ASpaceLevelBuildsNoStaticSky();
    OnlyTheExactNamesAreSkies();
    ASkyWithoutAPictureIsRefused();
    TheSatelliteKeepsItsPlaceOnTheScreen();
    AStillStripSitsSideBySide();
    AScrollingStripAndItsEnds();
    ALoneScrollingSkyIsBothEnds();
    AFullhdSkyIsWiderAndStillCentred();
    ThePixelCoverIsWhatThePixelCentresDraw();
    AWideViewIsCoveredByTheSkysContent();
    AWideViewFileThatDoesNotSayIsRefused();
    TheSpaceRulesAreSkyJsons();
    ASpaceLevelBuildsSpaceSky();
    SpaceSkyIsPinnedToTheCamera();
    TheSpiralTurnsAndThePlanetBobs();
    TheEclipseAddsTheCornersY();

    std::error_code ec;
    if (!std::filesystem::is_directory(kLevels, ec)) {
        std::printf("test_mp_sky: the levels' census SKIPPED - needs the converted levels at %s.\n"
                    "  They live outside this repository; configure with -DSUPERSONIC_MAGICPORTALS_LEVELS=...\n",
                    kLevels.c_str());
        return ::test::summary("test_mp_sky", 60);
    }
    EveryLevelsSky();
    EveryLevelsSpaceSky();
    return ::test::summary("test_mp_sky", 70);
}

// The part that reads the converted levels and their art is a suite of its own that exits 77 without them (planning step 209),
// so that a run without the data reports it SKIPPED and not passed: tests/CMakeLists.txt, test_mp_sky_levels.
static const ::MpSuite::Registration skyLevelsRegistration("test_mp_sky_levels", &skyLevelsEntry);

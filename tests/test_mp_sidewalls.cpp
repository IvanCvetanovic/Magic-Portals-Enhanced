// The stone wall beside a level narrower than the window: side_walls.json and sim/SideWalls (planning step 284).
//
// What is pinned, and from where:
//
//   the rules    side_walls.json's keys, strictly, and the mock's numbers the owner approved (2026-11-28): three columns 31 apart, pieces every 125 with
//                every second column from 0 and the others from 62, a quarter turn, the fade from 76 to 94 units. A missing key, a wrong kind, a variant
//                that is not one and a backing short of the fade are refused by name.
//   closed       a side is closed only by the named invisible walls, static, upright, unbreakable and unmoving, covering the line closed_reach_units
//                outside the edge over the whole height. A rigid, a trigger, a polygon, a turned, a breakable, a moving or an unnamed piece does not count,
//                nor does a wall with a gap or one that stands too far out.
//   the stone    the variant most of the level's own blocks carry, the world's default among equals and where it places none.
//   the layout   backings, stones and fades back to front, no two at one depth, all behind the black bars (z 1) and in front of the markers' boxes (0.5);
//                every column covers the level's whole height; the black reaches past a 48:9 window; the fade's slices meet, none over another, their
//                alpha rising from nothing to whole; nothing of the wall inside the level; the switch off lays nothing.
//   the census   (test_mp_sidewalls_levels, which needs the owner's converted levels and the original) 104 of the 128 converted levels are closed on both
//                sides, the other 24 by name, and the three variants of the stone read as .ent files with the facts the wall is drawn by.
//
// test_mp_sidewalls is pure arithmetic and files the suite writes: it runs anywhere and never skips.

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "sim/Art.hpp"
#include "sim/Chapters.hpp"
#include "sim/SideWalls.hpp"
#include "sim/Tscn.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace MagicPortals;

namespace {

const std::string kPortData = MAGICPORTALS_PORT_DATA_DIR;
const std::string kLevels = MAGICPORTALS_LEVELS_DIR;
const std::string kChapters = MAGICPORTALS_CHAPTERS_FILE;
const std::string kOriginal = MAGICPORTALS_ORIGINAL_DIR;

constexpr double kViewHeight = 256.0;
constexpr double kWidestShape = 48.0 / 9.0;

bool Near(double a, double b, double eps = 1e-9) {
    return std::fabs(a - b) <= eps;
}

SideWalls::Rules TheRules() {
    SideWalls::Rules rules;
    std::string error;
    CHECK_MSG(SideWalls::LoadRules(kPortData + "/side_walls.json", rules, error), error);
    return rules;
}

std::string ReadText(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

// ---- the rules ----------------------------------------------------------------

void TheRulesAreTheMocks() {
    const SideWalls::Rules rules = TheRules();
    CHECK(rules.enabled);
    CHECK_EQ(rules.columns, 3);
    CHECK_EQ(rules.columnStepUnits, 31.0);
    CHECK_EQ(rules.rowStepUnits, 125.0);
    CHECK_MSG(rules.rowStartUnits == std::vector<double>({62.0, 0.0}), "the columns' first rows are the mock's");
    CHECK_MSG(Near(rules.turnRad, std::acos(-1.0) / 2.0), "a quarter turn");
    CHECK_EQ(rules.fadeFromUnits, 76.0);
    CHECK_EQ(rules.fadeToUnits, 94.0);
    CHECK(rules.fadeSteps >= 48);
    CHECK_EQ(rules.closedReachUnits, 4.0);
    CHECK_MSG(rules.stoneEntity == "double_block_plat" && rules.blackEntity == "black_bar.ent", "the game's own entities");
    CHECK_MSG(rules.variants == std::vector<std::string>({"", "_low_emissive", "_no_emissive"}), "the three variants");
    CHECK_EQ(rules.worldVariants.size(), static_cast<std::size_t>(4));
    CHECK_MSG(SideWalls::StoneFile(rules, "_low_emissive") == "double_block_plat_low_emissive.ent", "the stone's file");
}

void AFileThatDoesNotSayIsRefused() {
    const std::string shipped = ReadText(kPortData + "/side_walls.json");
    CHECK(!shipped.empty());
    const std::string path = (std::filesystem::temp_directory_path() / "test_mp_sidewalls.json").string();
    const auto refused = [&](const std::string& from, const std::string& to, const std::string& said) {
        std::string text = shipped;
        const std::size_t at = text.find(from);
        CHECK_MSG(at != std::string::npos, "the shipped file says " + from);
        if (at == std::string::npos) return;
        text.replace(at, from.size(), to);
        std::ofstream(path, std::ios::binary) << text;
        SideWalls::Rules rules;
        rules.columns = -7;
        std::string error;
        CHECK_MSG(!SideWalls::LoadRules(path, rules, error), "refused: " + to);
        CHECK_MSG(error.find(said) != std::string::npos, "the refusal names it: " + error);
        CHECK_MSG(rules.columns == -7, "and writes nothing");
    };
    refused("\"side_walls\": {", "\"side_walls_gone\": {", "side_walls is missing");
    refused("\"enabled\": true", "\"enabled\": 1", "enabled");
    refused("\"closed_reach_units\": 4.0", "\"closed_reach_units\": 0.0", "closed_reach_units");
    refused("\"solid_entities\": [\"", "\"solid_entities\": [], \"x\": [\"", "solid_entities");
    refused("\"stone_entity\": \"double_block_plat\"", "\"stone_entity\": \"\"", "stone_entity");
    refused("\"black_entity\":", "\"black_entity_gone\":", "black_entity");
    refused("\"block_families\":", "\"block_families_gone\":", "block_families");
    refused("\"world_default_variants\": [\"\",", "\"world_default_variants\": [\"_bright\",", "_bright");
    refused("\"turn_deg\": 90.0", "\"turn_deg\": \"90\"", "turn_deg");
    refused("\"columns\": 3", "\"columns\": 2.5", "columns");
    refused("\"column_step_units\": 31.0", "\"column_step_units\": 0", "column_step_units");
    refused("\"row_step_units\": 125.0", "\"row_step_units\": -125.0", "row_step_units");
    refused("\"row_start_units\": [62.0, 0.0]", "\"row_start_units\": []", "row_start_units");
    refused("\"backing_reach_units\": 2048.0", "\"backing_reach_units\": 95.0", "backing_reach_units");
    refused("\"margin_units\": 128.0", "\"margin_units\": -1.0", "margin_units");
    refused("\"fade_to_units\": 94.0", "\"fade_to_units\": 76.0", "fade_to_units");
    refused("\"fade_steps\": 64", "\"fade_steps\": 0", "fade_steps");
    refused("\"fade_cap_units\": 2.0", "\"fade_cap_units\": 0.0", "fade_cap_units");
    refused("\"z_first\": 0.8", "\"z_first\": 0.0", "z_first");
    refused("\"z_step\": 0.0005", "\"z_step\": 0.0", "z_step");
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    SideWalls::Rules rules;
    std::string error;
    CHECK_MSG(!SideWalls::LoadRules(path, rules, error) && error.find("cannot read") != std::string::npos, error);
}

// ---- which sides are closed -----------------------------------------------------

// A scene written as the converter writes one: walls by their place and size.
struct SceneText {
    std::string shapes;
    std::string nodes;
    int made = 0;

    SceneText& Add(const std::string& entity, double x, double y, double w, double h, const std::string& body = "StaticBody2D",
                   const std::string& extra = "") {
        const std::string n = std::to_string(++made);
        shapes += "[sub_resource type=\"RectangleShape2D\" id=\"R" + n + "\"]\nsize = Vector2(" + std::to_string(w) + ", " +
                  std::to_string(h) + ")\n\n";
        nodes += "[node name=\"n" + n + "\" type=\"Node2D\" parent=\".\"]\nposition = Vector2(" + std::to_string(x) + ", " +
                 std::to_string(y) + ")\n" + extra + "metadata/entity_name = \"" + entity + "\"\n\n[node name=\"Body\" type=\"" + body +
                 "\" parent=\"n" + n + "\"]\n\n[node name=\"Shape\" type=\"CollisionShape2D\" parent=\"n" + n +
                 "/Body\"]\nshape = SubResource(\"R" + n + "\")\n\n";
        return *this;
    }
    // A 128 x 768 invisible wall with its inner face on x = `face`, `side` -1 left of it and +1 right.
    SceneText& Wall(double face, int side) { return Add("collision_128x768.ent", face + side * 64.0, 384.0, 128.0, 768.0); }
    // An entity node with no body: a placement the variant counts.
    SceneText& Place(const std::string& entity) {
        const std::string n = std::to_string(++made);
        nodes += "[node name=\"n" + n + "\" type=\"Node2D\" parent=\".\"]\nmetadata/entity_name = \"" + entity + "\"\n\n";
        return *this;
    }
    Tscn::Scene Parsed() const {
        const std::string text = "[gd_scene load_steps=1 format=3]\n\n" + shapes + "[node name=\"level\" type=\"Node2D\"]\n\n" + nodes;
        Tscn::Scene scene;
        std::string error;
        CHECK_MSG(Tscn::Parse(text, scene, error), error);
        return scene;
    }
};

const glm::dvec2 kBounds(512.0, 256.0);

void TwoWallsCloseALevel() {
    const SideWalls::Rules rules = TheRules();
    SceneText text;
    text.Wall(0.0, -1).Wall(512.0, 1);
    const SideWalls::Closed closed = SideWalls::ClosedSides(rules, text.Parsed(), kBounds);
    CHECK(closed.left && closed.right && closed.Both());
    SceneText left;
    left.Wall(0.0, -1);
    const SideWalls::Closed one = SideWalls::ClosedSides(rules, left.Parsed(), kBounds);
    CHECK_MSG(one.left && !one.right && !one.Both(), "one wall closes one side");
    SceneText right;
    right.Wall(512.0, 1);
    const SideWalls::Closed other = SideWalls::ClosedSides(rules, right.Parsed(), kBounds);
    CHECK_MSG(!other.left && other.right && !other.Both(), "and the other");
    CHECK(!SideWalls::ClosedSides(rules, SceneText{}.Parsed(), kBounds).Both());
    CHECK(!SideWalls::ClosedSides(rules, text.Parsed(), glm::dvec2(0.0)).Both());
}

void AWallCoversTheWholeHeightWithinReach() {
    const SideWalls::Rules rules = TheRules();
    const auto rightIs = [&](const SceneText& with) { return SideWalls::ClosedSides(rules, with.Parsed(), kBounds).right; };
    // 2-23 and 2-24: the inner face 2 units out of the edge.
    CHECK_MSG(rightIs(SceneText{}.Wall(514.0, 1)), "a wall 2 units out closes the side");
    CHECK_MSG(rightIs(SceneText{}.Wall(516.0, 1)), "and one at the reach");
    CHECK_MSG(!rightIs(SceneText{}.Wall(516.5, 1)), "one past the reach does not");
    // 1-08 and 1-27: 64 units out.
    CHECK(!rightIs(SceneText{}.Wall(576.0, 1)));
    // Two that meet cover the height; a gap between them, a start below the top or an end short of the bottom does not.
    const auto stack = [](double firstBottom, double secondTop, double secondBottom, double firstTop) {
        SceneText text;
        text.Add("collision_128_sqr.ent", 576.0, (firstTop + firstBottom) * 0.5, 128.0, firstBottom - firstTop);
        text.Add("collision_128x768.ent", 576.0, (secondTop + secondBottom) * 0.5, 128.0, secondBottom - secondTop);
        return text;
    };
    CHECK_MSG(rightIs(stack(128.0, 128.0, 256.0, 0.0)), "two walls that meet");
    CHECK_MSG(rightIs(stack(200.0, 128.0, 300.0, -40.0)), "two that overlap");
    CHECK_MSG(!rightIs(stack(128.0, 129.0, 256.0, 0.0)), "a unit's gap");
    CHECK_MSG(!rightIs(stack(128.0, 128.0, 255.0, 0.0)), "short of the level's height");
    CHECK_MSG(!rightIs(stack(128.0, 128.0, 256.0, 1.0)), "short of the top");
    // 1-25's left wall ends above the level's height.
    CHECK(!SideWalls::ClosedSides(rules, SceneText{}.Add("collision_128x768.ent", -64.0, -128.0, 128.0, 768.0).Parsed(),
                                  glm::dvec2(512.0, 378.0))
               .left);
    // The node's own `scale` datum scales its rectangle.
    CHECK_MSG(rightIs(SceneText{}.Add("collision_128x768.ent", 576.0, 192.0, 64.0, 384.0, "StaticBody2D", "metadata/scale = \"2\"\n")),
              "a scaled wall");
    CHECK_MSG(!rightIs(SceneText{}.Add("collision_128x768.ent", 576.0, 192.0, 64.0, 384.0)), "which unscaled stands too far out");
}

void NothingThatMovesOrBreaksClosesASide() {
    const SideWalls::Rules rules = TheRules();
    // The same rectangle, which closes the right side when it is a wall, made everything a wall is not.
    const auto closes = [&](const std::string& entity, const std::string& body, const std::string& extra) {
        SceneText text;
        text.Add(entity, 576.0, 128.0, 128.0, 768.0, body, extra);
        return SideWalls::ClosedSides(rules, text.Parsed(), kBounds).right;
    };
    CHECK_MSG(closes("collision_128x768.ent", "StaticBody2D", ""), "the wall itself");
    CHECK_MSG(closes("collision_768x128.ent", "StaticBody2D", ""), "its long sibling");
    CHECK_MSG(!closes("collision_128x768.ent", "RigidBody2D", ""), "a rigid body moves");
    CHECK_MSG(!closes("collision_128x768.ent", "Area2D", ""), "a trigger stops nothing");
    CHECK_MSG(!closes("collision_128x768.ent", "StaticBody2D", "metadata/breakable = \"1\"\n"), "a breakable piece");
    CHECK_MSG(!closes("collision_128x768.ent", "StaticBody2D", "metadata/stride = \"100\"\n"), "a piece with a stride moves");
    CHECK_MSG(!closes("collision_128x768.ent", "StaticBody2D", "metadata/speed = \"1\"\n"), "and one with a speed");
    CHECK_MSG(!closes("collision_128x768.ent", "StaticBody2D", "rotation = 0.5\n"), "a turned rectangle");
    for (const char* entity : {"breakable_wall", "breakable_wall.ent", "light_wall.ent", "door_lift", "door_locked.ent", "lift.ent",
                               "moving_platform", "bounce", "enemy_killer", "platform.ent"}) {
        CHECK_MSG(!closes(entity, "StaticBody2D", ""), std::string(entity) + " is not one of the walls");
    }
    // A polygon, whatever it is called.
    const std::string polygon =
        "[gd_scene load_steps=1 format=3]\n\n[node name=\"level\" type=\"Node2D\"]\n\n"
        "[node name=\"w\" type=\"Node2D\" parent=\".\"]\nposition = Vector2(576, 128)\nmetadata/entity_name = \"collision_128x768.ent\"\n\n"
        "[node name=\"Body\" type=\"StaticBody2D\" parent=\"w\"]\n\n"
        "[node name=\"Shape\" type=\"CollisionPolygon2D\" parent=\"w/Body\"]\n"
        "polygon = PackedVector2Array(-64, -384, 64, -384, 64, 384, -64, 384)\n";
    Tscn::Scene scene;
    std::string error;
    CHECK_MSG(Tscn::Parse(polygon, scene, error), error);
    CHECK_MSG(!SideWalls::ClosedSides(rules, scene, kBounds).right, "a polygon");
}

// ---- the stone ------------------------------------------------------------------

void TheStoneTakesTheLevelsOwnVariant() {
    const SideWalls::Rules rules = TheRules();
    const auto of = [&](const std::vector<std::string>& placed, int world) {
        SceneText text;
        for (const std::string& entity : placed) text.Place(entity);
        return SideWalls::Variant(rules, text.Parsed(), world);
    };
    CHECK_MSG(of({"platform.ent", "block00.ent", "double_block_plat_no_emissive.ent"}, 1) == "", "the most common wins");
    CHECK_MSG(of({"platform_low_emissive.ent", "single_block_plat_low_emissive.ent", "block00.ent"}, 0) == "_low_emissive", "low");
    CHECK_MSG(of({"block00_no_emissive.ent", "platform_no_emissive.ent", "platform.ent"}, 0) == "_no_emissive", "none");
    // Names with and without the extension are the same block.
    CHECK_MSG(of({"platform_no_emissive", "double_block_plat_no_emissive"}, 0) == "_no_emissive", "bare names");
    // A tie: the world's own among the tied, else the first of the variants.
    CHECK_MSG(of({"platform.ent", "platform_no_emissive.ent"}, 1) == "_no_emissive", "a tie takes chapter 2's");
    CHECK_MSG(of({"platform.ent", "platform_no_emissive.ent"}, 0) == "", "and chapter 1's");
    CHECK_MSG(of({"platform_low_emissive.ent", "platform_no_emissive.ent"}, 0) == "_low_emissive", "neither: the first listed");
    // None of the family: the world's default.
    const std::vector<std::string> others = {"wall04_low_emissive.ent", "crate_no_emissive", "collision_128x768.ent",
                                             "bridge01_no_emissive.ent"};
    CHECK_MSG(of(others, 0) == "", "chapter 1's");
    CHECK_MSG(of(others, 1) == "_no_emissive", "chapter 2's");
    CHECK_MSG(of(others, 2) == "_low_emissive", "chapter 3's");
    CHECK_MSG(of(others, 3) == "", "chapter 4's");
    CHECK_MSG(of({}, -1) == "" && of({}, 9) == "", "a world the file does not list takes the first variant");
}

// ---- the layout -------------------------------------------------------------------

const glm::dvec2 kStone(128.0, 32.0);

// A piece's footprint on the level: a quarter-turned stone lies down the screen.
struct Box {
    glm::dvec2 lo{0.0};
    glm::dvec2 hi{0.0};
};
Box Footprint(const SideWalls::Piece& piece) {
    glm::dvec2 half = piece.sizePx * 0.5;
    const double c = std::fabs(std::cos(piece.turnRad));
    const double s = std::fabs(std::sin(piece.turnRad));
    const glm::dvec2 turned(half.x * c + half.y * s, half.x * s + half.y * c);
    return Box{piece.centrePx - turned, piece.centrePx + turned};
}

void TheWallIsLaidAsTheMock() {
    const SideWalls::Rules rules = TheRules();
    for (const glm::dvec2& bounds : {glm::dvec2(512.0, 256.0), glm::dvec2(456.0, 256.0), glm::dvec2(484.0, 376.0),
                                     glm::dvec2(768.0, 512.0), glm::dvec2(512.0, 301.0)}) {
        const std::string level = std::to_string(static_cast<int>(bounds.x)) + "x" + std::to_string(static_cast<int>(bounds.y));
        const std::vector<SideWalls::Piece> pieces = SideWalls::Layout(rules, bounds, kStone);
        CHECK_MSG(!pieces.empty(), level);
        if (pieces.empty()) continue;
        // Depths: one each, rising in the list's order, between the boxes and the black bars.
        bool rising = true;
        for (std::size_t i = 1; i < pieces.size(); ++i) rising = rising && pieces[i].z > pieces[i - 1].z;
        CHECK_MSG(rising, level + ": no two pieces at one depth");
        CHECK_MSG(pieces.front().z > 0.75 && pieces.back().z < 0.99, level + ": between the boxes and the bars");
        // Back to front by kind.
        bool ordered = true;
        for (std::size_t i = 1; i < pieces.size(); ++i) {
            ordered = ordered && static_cast<int>(pieces[i].kind) >= static_cast<int>(pieces[i - 1].kind);
        }
        CHECK_MSG(ordered, level + ": backings, stones, fades");
        for (const int side : {-1, 1}) {
            const double edge = side < 0 ? 0.0 : bounds.x;
            std::map<double, std::vector<Box>> columns; // by the distance of the column's centre from the edge
            std::vector<SideWalls::Piece> fades;
            int backings = 0;
            for (const SideWalls::Piece& piece : pieces) {
                if (piece.side != side) continue;
                const Box box = Footprint(piece);
                const double inner = side < 0 ? box.hi.x : box.lo.x;
                CHECK_MSG(side * (inner - edge) >= -1e-9, level + ": nothing of the wall inside the level");
                if (piece.kind == SideWalls::Kind::Stone) {
                    CHECK_MSG(piece.sizePx == kStone && Near(piece.turnRad, rules.turnRad) && piece.alpha == 1.0, level + ": a stone");
                    columns[std::round((side * (piece.centrePx.x - edge)) * 1000.0) / 1000.0].push_back(box);
                } else if (piece.kind == SideWalls::Kind::Fade) {
                    fades.push_back(piece);
                } else {
                    ++backings;
                    const double reach = side < 0 ? edge - box.lo.x : box.hi.x - edge;
                    // A 48:9 window shows this much beyond the side.
                    CHECK_MSG(reach >= (kViewHeight * kWidestShape - bounds.x) * 0.5, level + ": the black reaches past 48:9");
                    CHECK_MSG(box.lo.y <= -64.0 && box.hi.y >= bounds.y + 64.0, level + ": and above and below the level");
                    CHECK(piece.alpha == 1.0 && piece.turnRad == 0.0);
                }
            }
            CHECK_EQ(backings, 1);
            CHECK_EQ(columns.size(), static_cast<std::size_t>(3));
            std::vector<double> centres;
            for (auto& [centre, boxes] : columns) {
                centres.push_back(centre);
                std::sort(boxes.begin(), boxes.end(), [](const Box& a, const Box& b) { return a.lo.y < b.lo.y; });
                double reached = 0.0;
                bool whole = true;
                for (const Box& box : boxes) {
                    whole = whole && box.lo.y <= reached + 1e-9;
                    reached = std::max(reached, box.hi.y);
                    CHECK_MSG(Near(box.hi.x - box.lo.x, 32.0, 1e-9) && Near(box.hi.y - box.lo.y, 128.0, 1e-9),
                              level + ": 32 wide, 128 tall");
                    CHECK_MSG(box.hi.y > 0.0 && box.lo.y < bounds.y, level + ": no piece wholly off the level's height");
                }
                CHECK_MSG(whole && reached >= bounds.y, level + ": a column covers the whole height");
                for (std::size_t i = 1; i < boxes.size(); ++i) CHECK_MSG(Near(boxes[i].lo.y - boxes[i - 1].lo.y, 125.0), level);
            }
            CHECK_MSG(centres == std::vector<double>({16.0, 47.0, 78.0}), level + ": columns every 31 from the edge");
            // The mock's rows: every second column half a piece higher.
            const double first = std::fmod(columns[16.0].front().lo.y + 64.0 + 125.0, 125.0);
            const double second = std::fmod(columns[47.0].front().lo.y + 64.0 + 125.0, 125.0);
            CHECK_MSG(Near(first, 62.0) && Near(second, 0.0), level + ": the rows start at 62 and at 0");
            // The fade: slices from 76 to 94 that meet, rising in alpha on the smoothstep, then whole black.
            CHECK_EQ(fades.size(), static_cast<std::size_t>(rules.fadeSteps) + 1);
            double at = rules.fadeFromUnits;
            double alpha = 0.0;
            double largest = 0.0;
            for (const SideWalls::Piece& fade : fades) {
                const Box box = Footprint(fade);
                const double from = side < 0 ? edge - box.hi.x : box.lo.x - edge;
                const double to = side < 0 ? edge - box.lo.x : box.hi.x - edge;
                CHECK_MSG(Near(from, at, 1e-9), level + ": the slices meet, none over another");
                at = to;
                CHECK_MSG(fade.alpha >= alpha && fade.alpha <= 1.0, level + ": the fade rises");
                largest = std::max(largest, fade.alpha - alpha);
                alpha = fade.alpha;
            }
            CHECK_MSG(Near(at, rules.fadeToUnits + rules.fadeCapUnits, 1e-9) && alpha == 1.0, level + ": and ends in whole black");
            CHECK_MSG(fades.front().alpha < 0.001 && largest < 0.025, level + ": no step to see: " + std::to_string(largest));
            const double middle = fades[static_cast<std::size_t>(rules.fadeSteps / 2)].alpha;
            CHECK_MSG(std::fabs(middle - 0.5) < 0.02, level + ": half at the fade's middle");
        }
    }
}

void TheSwitchAndTheViews() {
    SideWalls::Rules rules = TheRules();
    CHECK(!SideWalls::Layout(rules, kBounds, kStone).empty());
    CHECK(SideWalls::Layout(rules, kBounds, glm::dvec2(0.0)).empty());
    CHECK(SideWalls::Layout(rules, glm::dvec2(0.0), kStone).empty());
    rules.enabled = false;
    CHECK(SideWalls::Layout(rules, kBounds, kStone).empty());
    // At 16:9 and narrower no level is narrower than the view (the narrowest is 456).
    CHECK(!SideWalls::Shown(kViewHeight * 16.0 / 9.0, 456.0));
    CHECK(!SideWalls::Shown(kViewHeight * 4.0 / 3.0, 456.0));
    CHECK(!SideWalls::Shown(512.0, 512.0));
    CHECK(SideWalls::Shown(kViewHeight * 20.0 / 9.0, 512.0));
    CHECK(SideWalls::Shown(kViewHeight * 16.0 / 9.0 + 1.0, 456.0));
}

// ---- the owner's levels -------------------------------------------------------------

void TheLevelsThatAreClosed() {
    const SideWalls::Rules rules = TheRules();
    Chapters::Table table;
    std::string error;
    CHECK_MSG(Chapters::Load(kChapters, table, error), error);
    CHECK_EQ(table.levels.size(), static_cast<std::size_t>(128));
    // The open ones, by the converted file's name: 1-08, 1-25, 1-27, 2-32 and chapter 4's first twenty.
    std::set<std::string> expected = {"level7", "level24", "level26", "level31a"};
    expected.insert("level0c");
    for (int i = 1; i < 20; ++i) expected.insert("level" + std::to_string(i) + "c");
    std::set<std::string> open;
    std::map<std::string, int> variants;
    int closed = 0;
    int within2 = 0;
    for (const Chapters::Level& level : table.levels) {
        Tscn::Scene scene;
        CHECK_MSG(Tscn::Load(kLevels + "/" + level.name + ".tscn", scene, error), error);
        glm::dvec2 bounds(0.0);
        for (const Tscn::Node& node : scene.nodes) {
            const Tscn::Value* name = node.Meta("entity_name");
            const Tscn::Value* at = node.Find("position");
            if (name != nullptr && name->text == "max" && at != nullptr && at->numbers.size() >= 2) {
                bounds = glm::dvec2(at->numbers[0], at->numbers[1]);
            }
        }
        CHECK_MSG(bounds.x >= 456.0 && bounds.y >= 256.0, level.name + " has its max marker");
        const bool both = SideWalls::ClosedSides(rules, scene, bounds).Both();
        if (both) {
            ++closed;
            ++variants[SideWalls::Variant(rules, scene, level.world)];
        } else {
            open.insert(level.name);
        }
        SideWalls::Rules near = rules;
        near.closedReachUnits = 1.0;
        within2 += SideWalls::ClosedSides(near, scene, bounds).Both() ? 1 : 0;
    }
    CHECK_EQ(closed, 104);
    // 2-23 and 2-24 stand their right wall 2 units outside the level: closed by the reach, open under it.
    CHECK_EQ(within2, 102);
    CHECK_EQ(open.size(), static_cast<std::size_t>(24));
    std::string names;
    for (const std::string& name : open) names += " " + name;
    CHECK_MSG(open == expected, "the open levels:" + names);
    std::printf("  side walls: %d levels closed (%d plain, %d low emissive, %d no emissive), %zu open:%s\n", closed, variants[""],
                variants["_low_emissive"], variants["_no_emissive"], open.size(), names.c_str());
    CHECK_EQ(variants[""] + variants["_low_emissive"] + variants["_no_emissive"], 104);
}

void TheStonesReadFromTheOriginal() {
    const SideWalls::Rules rules = TheRules();
    const double emissive[] = {1.0, 0.45, 0.0};
    for (std::size_t i = 0; i < rules.variants.size() && i < 3; ++i) {
        const std::string file = SideWalls::StoneFile(rules, rules.variants[i]);
        Art::Entity stone;
        std::string error;
        CHECK_MSG(Art::LoadEnt(kOriginal + "/entities/" + file, stone, error), error);
        CHECK_MSG(stone.sprite == "STONE03A4x10_half_contrast.png" && stone.normal == "wall__height4x2_half_nm.png",
                  file + " draws " + stone.sprite + " with " + stone.normal);
        CHECK_MSG(stone.isStatic && stone.applyLight && !stone.additive && stone.columns == 1 && stone.rows == 1 &&
                      stone.scale == glm::dvec2(1.0),
                  file + ": static, lit, mixed, one frame, at scale 1");
        CHECK_MSG(Near(stone.emissive.x, emissive[i], 1e-6) && stone.emissive.x == stone.emissive.y &&
                      stone.emissive.y == stone.emissive.z,
                  file + " has emissive " + std::to_string(stone.emissive.x));
    }
}

constexpr int kSkippedExit = 77; // SKIPS_WITH_77, ctest's SKIP_RETURN_CODE (tests/test_mp.cpp)

int sideWallsLevelsEntry(int, char**) {
    std::error_code ec;
    if (!std::filesystem::is_directory(kLevels, ec) || !std::filesystem::is_regular_file(kChapters, ec) ||
        !std::filesystem::is_regular_file(kOriginal + "/entities/double_block_plat.ent", ec)) {
        std::printf("test_mp_sidewalls_levels: SKIPPED - needs the converted levels at %s and the original's assets at %s (no check was run)\n"
                    "  They live outside this repository; configure with -DSUPERSONIC_MAGICPORTALS_LEVELS=... and _ORIGINAL=...\n",
                    kLevels.c_str(), kOriginal.c_str());
        return kSkippedExit;
    }
    TheLevelsThatAreClosed();
    TheStonesReadFromTheOriginal();
    return ::test::summary("test_mp_sidewalls_levels", 270);
}

} // namespace

MP_SUITE_MAIN("test_mp_sidewalls") {
    TheRulesAreTheMocks();
    AFileThatDoesNotSayIsRefused();
    TwoWallsCloseALevel();
    AWallCoversTheWholeHeightWithinReach();
    NothingThatMovesOrBreaksClosesASide();
    TheStoneTakesTheLevelsOwnVariant();
    TheWallIsLaidAsTheMock();
    TheSwitchAndTheViews();
    return ::test::summary("test_mp_sidewalls", 500);
}

// The part that reads the converted levels and the original is a suite of its own that exits 77 without them (planning step
// 209), so that a run without the data reports it SKIPPED and not passed: tests/CMakeLists.txt, test_mp_sidewalls_levels.
static const ::MpSuite::Registration sideWallsLevelsRegistration("test_mp_sidewalls_levels", &sideWallsLevelsEntry);

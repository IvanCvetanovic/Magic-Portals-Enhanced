// A level's own art: the sprites the remake's converter wrote, as sim/Sprites
// reads them.
//
// What the converter decides is pinned exactly: which texture each entity node
// shows, its offset, its z_index, its blend, and the order Godot draws a canvas
// in - by z_index, then as the file lists them. The sizes are the images' own,
// read from the files the converter copied, so they are pinned too.
//
// The reader is first tested on a scene and images this suite writes, which
// runs anywhere. The rest reads the converted levels from outside this
// repository, and is skipped, saying where it looked, when they are absent.

#include "TestHarness.hpp"

#include "core/AudioClip.hpp"

#include "sim/Art.hpp"
#include "sim/GravityWell.hpp"
#include "sim/Lighting.hpp"
#include "sim/Loading.hpp"
#include "sim/Particles.hpp"
#include "sim/Sprites.hpp"
#include "sim/Tiers.hpp"
#include "sim/Tscn.hpp"

#include <stb_image.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

using namespace MagicPortals;

namespace {

const std::string kLevels = MAGICPORTALS_LEVELS_DIR;
const std::string kOriginal = MAGICPORTALS_ORIGINAL_DIR;
const std::string kPortData = MAGICPORTALS_PORT_DATA_DIR;

// tiers.json's search, which the layer draws a level's images through.
Tiers::Rules TheTiers() {
    Tiers::Rules rules;
    std::string error;
    CHECK_MSG(Tiers::LoadRules(kPortData + "/tiers.json", rules, error), error);
    return rules;
}

std::filesystem::path Scratch() {
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "supersonic-test-mp-sprites";
    std::error_code ec;
    std::filesystem::create_directories(dir / "assets", ec);
    return dir;
}

void Write(const std::filesystem::path& path, const std::vector<unsigned char>& bytes) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

// A PNG's first 26 bytes: the signature, then the IHDR chunk's length and tag,
// the width and height, the bit depth and the colour type.
std::vector<unsigned char> PngHeader(uint32_t width, uint32_t height) {
    std::vector<unsigned char> bytes = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A, 0, 0, 0, 13, 'I', 'H', 'D', 'R'};
    for (const uint32_t v : {width, height}) {
        for (int shift = 24; shift >= 0; shift -= 8) bytes.push_back(static_cast<unsigned char>(v >> shift));
    }
    bytes.push_back(8);
    bytes.push_back(6);
    return bytes;
}

// A BMP's file header and the start of a 40-byte info header.
std::vector<unsigned char> BmpHeader(int32_t width, int32_t height) {
    std::vector<unsigned char> bytes = {'B', 'M', 0, 0, 0, 0, 0, 0, 0, 0, 54, 0, 0, 0, 40, 0, 0, 0};
    for (const int32_t v : {width, height}) {
        const uint32_t u = static_cast<uint32_t>(v);
        for (int shift = 0; shift < 32; shift += 8) bytes.push_back(static_cast<unsigned char>(u >> shift));
    }
    return bytes;
}

// A JPEG's start as a camera writes one: the start marker, a JFIF segment and a
// quantisation table, then a frame header of `marker` (0xC0 baseline) at 8 bits
// and three components - height BEFORE width, the reverse of a PNG. The scan
// that would follow is left off: the reader stops at the frame header.
std::vector<unsigned char> JpegHeader(uint16_t width, uint16_t height, unsigned char marker = 0xC0) {
    std::vector<unsigned char> bytes = {0xFF, 0xD8, 0xFF, 0xE0, 0, 16, 'J', 'F', 'I', 'F', 0, 1, 1, 0, 0, 1, 0, 1, 0, 0};
    bytes.insert(bytes.end(), {0xFF, 0xDB, 0, 67, 0});
    bytes.insert(bytes.end(), 64, static_cast<unsigned char>(1));
    bytes.insert(bytes.end(), {0xFF, marker, 0, 17, 8});
    for (const uint16_t v : {height, width}) {
        bytes.push_back(static_cast<unsigned char>(v >> 8));
        bytes.push_back(static_cast<unsigned char>(v));
    }
    bytes.insert(bytes.end(), {3, 1, 0x22, 0, 2, 0x11, 1, 3, 0x11, 1});
    return bytes;
}

// Where JpegHeader's frame header starts: after the JFIF segment and the table.
constexpr std::size_t kJpegFrameAt = 20 + 69;

bool Near(double a, double b) { return std::fabs(a - b) < 1e-6; }

// ---- the reader, on files this suite writes -------------------------------------

void AnImageSaysItsSizeInItsHeader() {
    const std::filesystem::path dir = Scratch();
    int w = 0;
    int h = 0;
    std::string error;

    Write(dir / "sky.png", PngHeader(455, 256));
    CHECK_MSG(Sprites::ImageSize((dir / "sky.png").string(), w, h, error), error);
    CHECK_EQ(w, 455);
    CHECK_EQ(h, 256);

    // Stored top down, which a BMP says with a negative height.
    Write(dir / "black.bmp", BmpHeader(40, -100));
    CHECK_MSG(Sprites::ImageSize((dir / "black.bmp").string(), w, h, error), error);
    CHECK_EQ(w, 40);
    CHECK_EQ(h, 100);

    // The old OS/2 header keeps its sizes in 16 bits.
    Write(dir / "old.bmp", {'B', 'M', 0, 0, 0, 0, 0, 0, 0, 0, 26, 0, 0, 0, 12, 0, 0, 0, 16, 0, 8, 0, 1, 0, 24, 0});
    CHECK_MSG(Sprites::ImageSize((dir / "old.bmp").string(), w, h, error), error);
    CHECK_EQ(w, 16);
    CHECK_EQ(h, 8);
}

void AnythingElseIsRefusedByName() {
    const std::filesystem::path dir = Scratch();
    int w = 0;
    int h = 0;
    std::string error;

    Write(dir / "fire.gif", {'G', 'I', 'F', '8', '9', 'a', 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
    const bool gif = Sprites::ImageSize((dir / "fire.gif").string(), w, h, error);
    CHECK(!gif);
    CHECK_MSG(error.find("fire.gif") != std::string::npos, error);

    std::vector<unsigned char> cut = PngHeader(64, 64);
    cut.resize(12);
    Write(dir / "cut.png", cut);
    const bool truncated = Sprites::ImageSize((dir / "cut.png").string(), w, h, error);
    CHECK_MSG(!truncated, "a header cut short says no size");

    Write(dir / "empty.png", PngHeader(0, 64));
    const bool empty = Sprites::ImageSize((dir / "empty.png").string(), w, h, error);
    CHECK_MSG(!empty, "nor does one that says zero");

    error.clear();
    const bool missing = Sprites::ImageSize((dir / "nothing.png").string(), w, h, error);
    CHECK(!missing);
    CHECK_MSG(error.find("nothing.png") != std::string::npos, error);
}

// A JPEG's size is in its frame header, behind its other segments (K16: the
// particle systems of fireball, burn_projectile, dragon, dark_dragon and
// ghost_utility_spawn draw particles/explosion.JPG).
void AJpegSaysItsSizeInItsFrameHeader() {
    const std::filesystem::path dir = Scratch();
    int w = 0;
    int h = 0;
    std::string error;

    // Not square, so a width and a height read the wrong way round would show.
    Write(dir / "wide.jpg", JpegHeader(455, 256));
    CHECK_MSG(Sprites::ImageSize((dir / "wide.jpg").string(), w, h, error), error);
    CHECK_EQ(w, 455);
    CHECK_EQ(h, 256);

    // Progressive, with fill bytes before its frame header's marker and a byte of
    // padding after the table, both of which the renderer's decoder passes over.
    std::vector<unsigned char> padded = JpegHeader(32, 100, 0xC2);
    padded.insert(padded.begin() + static_cast<std::ptrdiff_t>(kJpegFrameAt), {0x00, 0xFF, 0xFF});
    Write(dir / "tall.jpg", padded);
    CHECK_MSG(Sprites::ImageSize((dir / "tall.jpg").string(), w, h, error), error);
    CHECK_EQ(w, 32);
    CHECK_EQ(h, 100);

    // Extended sequential, one component: a frame header of 11 bytes.
    std::vector<unsigned char> grey = JpegHeader(7, 9, 0xC1);
    grey.resize(kJpegFrameAt + 9);
    grey[kJpegFrameAt + 3] = 11;
    grey.insert(grey.end(), {1, 1, 0x11, 0}); // one component: its id, sampling and table
    Write(dir / "grey.jpg", grey);
    CHECK_MSG(Sprites::ImageSize((dir / "grey.jpg").string(), w, h, error), error);
    CHECK_EQ(w, 7);
    CHECK_EQ(h, 9);
}

// A JPEG cut short, or one the renderer's decoder (stb_image) would not read, has
// no size: a size for it would put a particle system on a texture that draws as
// the fallback.
void AJpegTheRendererCannotReadIsRefused() {
    const std::filesystem::path dir = Scratch();
    const auto refused = [&dir](const char* name, const std::vector<unsigned char>& bytes, const char* says) {
        int w = 0;
        int h = 0;
        std::string error;
        Write(dir / name, bytes);
        const bool sized = Sprites::ImageSize((dir / name).string(), w, h, error);
        CHECK_MSG(!sized, std::string(name) + " was sized " + std::to_string(w) + " x " + std::to_string(h));
        CHECK_MSG(error.find(name) != std::string::npos && error.find(says) != std::string::npos,
                  std::string(name) + ": " + error);
    };
    const std::vector<unsigned char> whole = JpegHeader(64, 32);

    std::vector<unsigned char> cut = whole;
    cut.resize(kJpegFrameAt);
    refused("cut_before_frame.jpg", cut, "cut short before its frame header");
    cut = whole;
    cut.resize(kJpegFrameAt + 6);
    refused("cut_in_frame.jpg", cut, "cut short in its frame header");
    cut = whole;
    cut.resize(40); // inside the table, whose length says 67
    refused("cut_in_table.jpg", cut, "cut short before its frame header");
    refused("start_only.jpg", {0xFF, 0xD8}, "cut short before its frame header");

    refused("lossless.jpg", JpegHeader(64, 32, 0xC3), "marker 0xC3");
    refused("arithmetic.jpg", JpegHeader(64, 32, 0xC9), "marker 0xC9");
    std::vector<unsigned char> scanFirst = whole;
    scanFirst[kJpegFrameAt + 1] = 0xDA;
    refused("scan_first.jpg", scanFirst, "marker 0xDA");

    std::vector<unsigned char> twelve = whole;
    twelve[kJpegFrameAt + 4] = 12;
    refused("twelve_bit.jpg", twelve, "12 bits a sample");
    std::vector<unsigned char> components = whole;
    components[kJpegFrameAt + 9] = 2;
    refused("two_components.jpg", components, "frame header the renderer does not decode");
    std::vector<unsigned char> delayed = whole; // the height left to a later DNL segment
    delayed[kJpegFrameAt + 5] = 0;
    delayed[kJpegFrameAt + 6] = 0;
    refused("delayed_height.jpg", delayed, "says it has no size");

    std::vector<unsigned char> shortSegment = whole;
    shortSegment[5] = 1;
    refused("short_segment.jpg", shortSegment, "shorter than its own length");
    refused("no_marker.jpg", {0xFF, 0xD8, 0x00, 0xFF, 0xC0}, "no marker after its start");
}

// Three entities, written out of drawing order, with every property the reader
// takes between them.
const char* const kScene = R"([gd_scene load_steps=4 format=3]

[ext_resource type="Texture2D" path="res://assets/a.png" id="tex_0"]
[ext_resource type="Texture2D" path="res://assets/b.png" id="tex_1"]

[sub_resource type="CanvasItemMaterial" id="CanvasItemMaterial_blend1"]
blend_mode = 1

[node name="level" type="Node2D"]

[node name="front_1" type="Node2D" parent="."]
position = Vector2(10, 20)
z_index = 5

[node name="Sprite" type="Sprite2D" parent="front_1"]
texture = ExtResource("tex_0")

[node name="back_2" type="Node2D" parent="."]
position = Vector2(30, 40)
rotation = 1.5708
z_index = -3

[node name="Sprite" type="Sprite2D" parent="back_2"]
texture = ExtResource("tex_1")
offset = Vector2(4, 0)
material = SubResource("CanvasItemMaterial_blend1")

[node name="middle_3" type="Node2D" parent="."]

[node name="Sprite" type="Sprite2D" parent="middle_3"]
texture = ExtResource("tex_0")
)";

bool ParseScene(const char* text, Tscn::Scene& scene) {
    std::string error;
    const bool ok = Tscn::Parse(text, scene, error);
    CHECK_MSG(ok, error);
    return ok;
}

void TheCanvasOrderIsZThenTheFile() {
    const std::filesystem::path dir = Scratch();
    Write(dir / "assets" / "a.png", PngHeader(4, 2));
    Write(dir / "assets" / "b.png", PngHeader(8, 8));
    Tscn::Scene scene;
    if (!ParseScene(kScene, scene)) return;

    std::vector<Sprites::Sprite> sprites;
    std::string error;
    const bool found = Sprites::Find(scene, dir.string(), Tiers::Rules{}, sprites, error);
    CHECK_MSG(found, error);
    CHECK_EQ(sprites.size(), std::size_t{3});
    if (sprites.size() != 3) return;

    CHECK(sprites[0].node == "back_2");
    CHECK(sprites[1].node == "middle_3");
    CHECK_MSG(sprites[2].node == "front_1", "the highest z_index is drawn last, in front");
    for (int i = 0; i < 3; ++i) CHECK_EQ(sprites[static_cast<std::size_t>(i)].order, i);

    const Sprites::Sprite& back = sprites[0];
    CHECK_MSG(back.texture == dir.string() + "/assets/b.png", back.texture);
    CHECK(back.sizePx == glm::dvec2(8.0, 8.0));
    CHECK_MSG(back.tier.empty() && back.density == 1.0f && back.texels == glm::ivec2(8, 8),
              "with no tier searched, the file named at density 1");
    CHECK_EQ(back.zIndex, -3);
    CHECK(Near(back.rotation, 1.5708));
    CHECK(back.offsetPx == glm::dvec2(4.0, 0.0));
    CHECK_MSG(back.additive, "blend_mode 1 is added");
    // A quarter turn clockwise on the screen takes the offset from +x to +y,
    // which in the level's pixels is down.
    const glm::dvec2 centre = Sprites::CentrePx(back);
    CHECK_MSG(std::fabs(centre.x - 30.0) < 1e-3 && std::fabs(centre.y - 44.0) < 1e-3,
              "turned with its node: " + std::to_string(centre.x) + ", " + std::to_string(centre.y));

    const Sprites::Sprite& front = sprites[2];
    CHECK(front.sizePx == glm::dvec2(4.0, 2.0));
    CHECK(front.atPx == glm::dvec2(10.0, 20.0));
    CHECK_MSG(!front.additive && !sprites[1].additive, "and a sprite with no material mixes");
    CHECK_EQ(sprites[1].zIndex, 0);
}

// One entity node and its sprite, with whatever goes between.
std::string OneSprite(const std::string& resources, const std::string& between, const std::string& sprite) {
    return "[gd_scene format=3]\n\n" + resources + "\n[node name=\"level\" type=\"Node2D\"]\n\n"
           "[node name=\"wall_1\" type=\"Node2D\" parent=\".\"]\n\n" + between + sprite;
}

void WhatTheReaderDoesNotDrawIsNamed() {
    const std::filesystem::path dir = Scratch();
    Write(dir / "assets" / "a.png", PngHeader(4, 2));
    const std::string texture = "[ext_resource type=\"Texture2D\" path=\"res://assets/a.png\" id=\"tex_0\"]\n";
    const std::string sprite = "[node name=\"Sprite\" type=\"Sprite2D\" parent=\"wall_1\"]\ntexture = ExtResource(\"tex_0\")\n";

    const auto refused = [&dir](const std::string& text, const char* expect, const char* what) {
        Tscn::Scene scene;
        if (!ParseScene(text.c_str(), scene)) return;
        std::vector<Sprites::Sprite> sprites;
        std::string error;
        const bool found = Sprites::Find(scene, dir.string(), Tiers::Rules{}, sprites, error);
        CHECK_MSG(!found, what);
        CHECK_MSG(error.find(expect) != std::string::npos, std::string(what) + ": " + error);
    };

    // A texture the converter named and did not copy.
    refused(OneSprite("[ext_resource type=\"Texture2D\" path=\"res://assets/gone.png\" id=\"tex_0\"]\n", "", sprite),
            "gone.png", "a missing image is refused, by its file");

    // A blend the port does not draw: Godot's 2 subtracts.
    refused(OneSprite(texture + "\n[sub_resource type=\"CanvasItemMaterial\" id=\"m\"]\nblend_mode = 2\n", "",
                      sprite + "material = SubResource(\"m\")\n"),
            "blend_mode", "a subtracting sprite is refused");

    // A sprite that is not an entity's own.
    refused(OneSprite(texture, "[node name=\"Body\" type=\"StaticBody2D\" parent=\"wall_1\"]\n\n",
                      "[node name=\"Sprite\" type=\"Sprite2D\" parent=\"wall_1/Body\"]\ntexture = ExtResource(\"tex_0\")\n"),
            "wall_1/Body", "a sprite under a body is refused");
}

// THE TIER A SPRITE IS DRAWN FROM (tiers.json; planning doc steps 61 and 66).
// The level names the 1x file; the original's loader draws <dir>/fullhd/<file>,
// else <dir>/hd/<file>, else the file, and sizes it int(texels / density) units.
// The offset is the level's and no density touches it.
void ASpriteIsDrawnFromItsTier() {
    const std::filesystem::path dir = Scratch() / "tiers";
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    for (const char* folder : {"assets", "assets/hd", "assets/fullhd"}) {
        std::filesystem::create_directories(dir / folder, ec);
    }
    Write(dir / "assets" / "crystal.png", PngHeader(32, 32));
    Write(dir / "assets" / "hd" / "crystal.png", PngHeader(64, 64));
    // A fullhd file wider than twice its 1x file: the original's four skies.
    Write(dir / "assets" / "sky.png", PngHeader(455, 256));
    Write(dir / "assets" / "fullhd" / "sky.png", PngHeader(1024, 512));
    Write(dir / "assets" / "hd" / "sky.png", PngHeader(910, 512)); // fullhd is tried first
    // An odd tier file: 21 texels at density 2 is 10 units, not 10.5.
    Write(dir / "assets" / "odd.png", PngHeader(10, 10));
    Write(dir / "assets" / "hd" / "odd.png", PngHeader(21, 21));
    // A tier file with no 1x file beside it is still drawn: only the tier names are tried.
    Write(dir / "assets" / "hd" / "only.png", PngHeader(64, 128));
    Write(dir / "assets" / "plain.png", PngHeader(40, 100));
    // A tier file smaller than one unit.
    Write(dir / "assets" / "hd" / "speck.png", PngHeader(1, 4));

    // One entity with its sprite, naming `texture`, with whatever `offset` line is given.
    const auto scene = [](const std::string& texture, const std::string& offset) {
        return OneSprite("[ext_resource type=\"Texture2D\" path=\"res://assets/" + texture + "\" id=\"tex_0\"]\n", "",
                         "[node name=\"Sprite\" type=\"Sprite2D\" parent=\"wall_1\"]\ntexture = ExtResource(\"tex_0\")\n" +
                             offset);
    };
    const Tiers::Rules tiers = TheTiers();
    const auto drawn = [&dir, &scene](const Tiers::Rules& rules, const std::string& texture, Sprites::Sprite& out,
                                      std::string& error, const std::string& offset = std::string()) {
        Tscn::Scene parsed;
        if (!ParseScene(scene(texture, offset).c_str(), parsed)) return false;
        std::vector<Sprites::Sprite> sprites;
        const bool found = Sprites::Find(parsed, dir.string(), rules, sprites, error);
        if (found && sprites.size() == 1) out = sprites[0];
        return found && sprites.size() == 1;
    };

    Sprites::Sprite sprite;
    std::string error;
    CHECK_MSG(drawn(tiers, "crystal.png", sprite, error, "offset = Vector2(0, 16)\n"), error);
    CHECK_MSG(sprite.texture == dir.string() + "/assets/hd/crystal.png" && sprite.tier == "hd" && sprite.density == 2.0f,
              "an hd file is drawn in place of the 1x file: " + sprite.texture);
    CHECK_MSG(sprite.texels == glm::ivec2(64, 64) && sprite.sizePx == glm::dvec2(32.0, 32.0),
              "at its texels over its density, which is the 1x file's size");
    CHECK_MSG(sprite.offsetPx == glm::dvec2(0.0, 16.0), "and the level's offset, untouched by the density");
    CHECK_MSG(Sprites::CentrePx(sprite) == glm::dvec2(0.0, 16.0), "so it stands where the 1x file stood");

    CHECK_MSG(drawn(tiers, "sky.png", sprite, error), error);
    CHECK_MSG(sprite.tier == "fullhd" && sprite.texture == dir.string() + "/assets/fullhd/sky.png",
              "fullhd is tried before hd: " + sprite.texture);
    CHECK_MSG(sprite.texels == glm::ivec2(1024, 512) && sprite.sizePx == glm::dvec2(512.0, 256.0),
              "and a tier file that is not twice its 1x file is drawn at its own units, not the 1x size");

    CHECK_MSG(drawn(tiers, "odd.png", sprite, error), error);
    CHECK_MSG(sprite.texels == glm::ivec2(21, 21) && sprite.sizePx == glm::dvec2(10.0, 10.0),
              "whole units, truncated after the divide");

    CHECK_MSG(drawn(tiers, "only.png", sprite, error), error);
    CHECK_MSG(sprite.tier == "hd" && sprite.sizePx == glm::dvec2(32.0, 64.0), "a tier file with no 1x file is drawn");

    CHECK_MSG(drawn(tiers, "plain.png", sprite, error), error);
    CHECK_MSG(sprite.tier.empty() && sprite.density == 1.0f && sprite.texture == dir.string() + "/assets/plain.png" &&
                  sprite.sizePx == glm::dvec2(40.0, 100.0),
              "a file with no tier file is drawn as named, at density 1");

    CHECK_MSG(drawn(Tiers::Rules{}, "crystal.png", sprite, error), error);
    CHECK_MSG(sprite.tier.empty() && sprite.sizePx == glm::dvec2(32.0, 32.0) &&
                  sprite.texture == dir.string() + "/assets/crystal.png",
              "and with no search, every file is drawn as named");

    error.clear();
    CHECK_MSG(!drawn(tiers, "speck.png", sprite, error) && error.find("less than one unit") != std::string::npos,
              "a tier file under one unit is refused, by name: " + error);
    error.clear();
    CHECK_MSG(!drawn(tiers, "gone.png", sprite, error) && error.find("assets/gone.png") != std::string::npos,
              "a name with neither a tier file nor its own file is refused, by the name: " + error);

    // The same rule by name, with no scene: what the layer draws the port's own
    // pictures and every particle bitmap through (00_order 2.3).
    const std::string assets = dir.string() + "/assets/";
    Sprites::Image image;
    error.clear();
    CHECK_MSG(Sprites::Drawn(tiers, assets + "crystal.png", image, error), error);
    CHECK_MSG(image.path == assets + "hd/crystal.png" && image.tier == "hd" && image.density == 2.0f &&
                  image.texels == glm::ivec2(64, 64) && image.units == glm::ivec2(32, 32),
              "by name: the hd file, 64 texels, 32 units: " + image.path);
    CHECK_MSG(Sprites::Drawn(tiers, assets + "sky.png", image, error) && image.tier == "fullhd" &&
                  image.units == glm::ivec2(512, 256),
              "fullhd before hd, at its own units");
    CHECK_MSG(Sprites::Drawn(tiers, assets + "odd.png", image, error) && image.units == glm::ivec2(10, 10),
              "whole units by name too");
    CHECK_MSG(Sprites::Drawn(tiers, assets + "plain.png", image, error) && image.tier.empty() &&
                  image.density == 1.0f && image.path == assets + "plain.png" && image.units == glm::ivec2(40, 100),
              "no tier file: the name itself at density 1");
    error.clear();
    CHECK_MSG(!Sprites::Drawn(tiers, assets + "speck.png", image, error) &&
                  error == assets + "hd/speck.png is less than one unit at density " + std::to_string(2.0f) &&
                  image.path == assets + "hd/speck.png" && image.units == glm::ivec2(0),
              "under one unit refused, saying which file it tried: " + error);
    error.clear();
    CHECK_MSG(!Sprites::Drawn(tiers, assets + "gone.png", image, error) &&
                  error.find("assets/gone.png") != std::string::npos && image.path == assets + "gone.png" &&
                  image.texels == glm::ivec2(0) && image.units == glm::ivec2(0),
              "unreadable refused by the file tried, with no size: " + error);
}

// THE SCALE A SPRITE IS DRAWN AT (00_order 9.1, K9; planning step 93). The
// entity's <Scale>, on its Sprite2D: Ethanon draws the frame times the scale
// (ETHSpriteEntity.cpp:682) about the pivot times the scale (ETHEntity.cpp:296).
// It multiplies the whole units - truncated first, from the tier file's texels
// over its density - and the offset as the level wrote it, and touches neither
// the texels nor the density.
void ASpriteIsDrawnAtItsScale() {
    const std::filesystem::path dir = Scratch() / "scale";
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    for (const char* folder : {"assets", "assets/hd"}) {
        std::filesystem::create_directories(dir / folder, ec);
    }
    Write(dir / "assets" / "crystal.png", PngHeader(32, 32));
    Write(dir / "assets" / "hd" / "crystal.png", PngHeader(64, 64));
    Write(dir / "assets" / "odd.png", PngHeader(10, 10));
    Write(dir / "assets" / "hd" / "odd.png", PngHeader(21, 21)); // 10 units, not 10.5
    const Tiers::Rules tiers = TheTiers();
    const auto drawn = [&dir, &tiers](const std::string& texture, const std::string& lines, Sprites::Sprite& out,
                                      std::string& error) {
        Tscn::Scene parsed;
        const std::string text =
            OneSprite("[ext_resource type=\"Texture2D\" path=\"res://assets/" + texture + "\" id=\"tex_0\"]\n", "",
                      "[node name=\"Sprite\" type=\"Sprite2D\" parent=\"wall_1\"]\ntexture = ExtResource(\"tex_0\")\n" +
                          lines);
        if (!ParseScene(text.c_str(), parsed)) return false;
        std::vector<Sprites::Sprite> sprites;
        const bool found = Sprites::Find(parsed, dir.string(), tiers, sprites, error);
        if (found && sprites.size() == 1) out = sprites[0];
        return found && sprites.size() == 1;
    };

    Sprites::Sprite sprite;
    std::string error;
    CHECK_MSG(drawn("crystal.png", "offset = Vector2(4, 16)\n", sprite, error), error);
    CHECK_MSG(sprite.scale == glm::dvec2(1.0) && sprite.sizePx == glm::dvec2(32.0, 32.0) &&
                  sprite.offsetPx == glm::dvec2(4.0, 16.0),
              "a sprite with no scale is drawn at 1");

    CHECK_MSG(drawn("crystal.png", "offset = Vector2(4, 16)\nscale = Vector2(1.3, 0.8)\n", sprite, error), error);
    CHECK_MSG(sprite.scale == glm::dvec2(1.3, 0.8), "its scale, as the file writes it");
    CHECK_MSG(sprite.tier == "hd" && sprite.density == 2.0f && sprite.texels == glm::ivec2(64, 64),
              "from the same tier file, at the same density: the scale is not a tier");
    CHECK_MSG(Near(sprite.sizePx.x, 32.0 * 1.3) && Near(sprite.sizePx.y, 32.0 * 0.8),
              "at its units times its scale, each axis its own: " + std::to_string(sprite.sizePx.x) + " x " +
                  std::to_string(sprite.sizePx.y));
    CHECK_MSG(Near(sprite.offsetPx.x, 4.0 * 1.3) && Near(sprite.offsetPx.y, 16.0 * 0.8),
              "and its offset times its scale, so its pivot stays on its node");
    const glm::dvec2 centre = Sprites::CentrePx(sprite);
    CHECK_MSG(Near(centre.x, 5.2) && Near(centre.y, 12.8), "which is where its centre stands");

    // The truncation comes first: 21 hd texels are 10 units, and 10 x 1.3 is 13,
    // where 10.5 x 1.3 would be 13.65.
    CHECK_MSG(drawn("odd.png", "scale = Vector2(1.3, 1.3)\n", sprite, error), error);
    CHECK_MSG(sprite.texels == glm::ivec2(21, 21) && Near(sprite.sizePx.x, 13.0) && Near(sprite.sizePx.y, 13.0),
              "whole units, truncated, then scaled: " + std::to_string(sprite.sizePx.x));

    // The three placements' sizes 9.3 pins on the converted levels, here on files
    // of their units: a barrel's 32 at 1.3, the eclipse's 256 x 32 at 2 with its
    // pivot (0, -15), the stone's 64 at 0.8.
    Write(dir / "assets" / "barrel.png", PngHeader(32, 32));
    Write(dir / "assets" / "eclipse.png", PngHeader(256, 32));
    Write(dir / "assets" / "stone.png", PngHeader(64, 64));
    CHECK_MSG(drawn("barrel.png", "scale = Vector2(1.3, 1.3)\n", sprite, error) && Near(sprite.sizePx.x, 41.6) &&
                  Near(sprite.sizePx.y, 41.6),
              "a barrel at 1.3: 41.6 units " + error);
    CHECK_MSG(drawn("eclipse.png", "offset = Vector2(0, -15)\nscale = Vector2(2, 2)\n", sprite, error) &&
                  sprite.sizePx == glm::dvec2(512.0, 64.0) && sprite.offsetPx == glm::dvec2(0.0, -30.0),
              "the eclipse at 2: 512 x 64, its offset (0, -30) " + error);
    CHECK_MSG(drawn("stone.png", "scale = Vector2(0.8, 0.8)\n", sprite, error) && Near(sprite.sizePx.x, 51.2) &&
                  Near(sprite.sizePx.y, 51.2),
              "the stone at 0.8: 51.2 units " + error);
}

// ---- art.json: what no level pictures ------------------------------------------

void ThePortalAndTheShotAreTheirEnts() {
    // The .ent files' facts, pinned, and what the shot's script does to its
    // picture (planning step 81): Scale(0.8), a 30 ms cell, turned to its flight.
    Art::Rules rules;
    std::string error;
    const bool ok = Art::LoadRules(std::string(MAGICPORTALS_PORT_DATA_DIR) + "/art.json", rules, error);
    CHECK_MSG(ok, error);
    CHECK_MSG(rules.portal.sprite == "portal_halo.png" && rules.portal.additive && rules.portal.Frames() == 1,
              "portal.ent: its halo, added");
    CHECK_MSG(rules.shot.sprite == "projectile.png" && rules.shot.additive, "projectile.ent: its sheet, added");
    CHECK_MSG(rules.shot.columns == 6 && rules.shot.rows == 1, "cut 6 x 1, as its SpriteCut says");
    CHECK_MSG(rules.shot.strideMs == 30.0 && std::fabs(rules.shot.framesPerSecond - 1000.0 / 30.0) < 1e-9,
              "_projectileFrameTimer.set(0, 5, 30, true): a cell every 30 ms (ETHCallback_projectile #89-100)");
    CHECK_MSG(rules.shot.scale == 0.8, "Scale(0.8) once it is added (addProjectile #158-165)");
    CHECK_MSG(rules.shot.pivotXPx == 4.0 && rules.shot.pivotYPx == 0.0, "its PivotAdjust (4, 0), on the bolt's head");
    CHECK_MSG(rules.shot.turnsToFlight, "turned to its flight every frame (ETHCallback_projectile #102-111)");
    CHECK_MSG(64.0 * rules.shot.scale == 51.2, "a 64-unit cell drawn at 51.2");

    const Art::Character& mage = rules.character;
    CHECK_MSG(mage.sprite == "magic_portals_hd.png" && !mage.additive && mage.columns == 4 && mage.rows == 4,
              "dark_mage.ent: its sheet, cut 4 x 4, mixed");
    CHECK_MSG(mage.pivotXPx == 0.0 && mage.pivotYPx == 2.0, "its pivot 2 px below the middle");
    CHECK_MSG(mage.leftRow == 1 && mage.rightRow == 2, "the rows the decoded DIRECTION enum gives left and right");
    // GameCharacter's constructor sets direction 2, and every update with no walk
    // stands it on SetFrame(idleColumn, direction): frame 8, facing right. The
    // .ent's startFrame 4 is set once and replaced, so the port does not carry it.
    CHECK_MSG(mage.initialDirection == 2 && mage.initialDirection == mage.rightRow,
              "made facing right: GameCharacter's direction 2 is the right row");
    CHECK_MSG(mage.idleColumn == 0 && mage.initialDirection * mage.columns + mage.idleColumn == 8,
              "standing on idleColumn 0, so a level starts on frame 8");
    CHECK_MSG(mage.strideMs == 90.0 && std::fabs(mage.framesPerSecond - 1000.0 / 90.0) < 1e-9,
              "a column every 90 ms, SideScrollerCharacter's frameStride");
    // Its arm out (SideScrollerCharacter::detectPushing and findFinalDirection): a
    // ray 0.6 of the frame's width along the velocity applyForces sets, 6 units
    // higher at its far end, the body's own x velocity times 0.86 off the ground at
    // 60 Hz; row 0 pushing left, row 3 any other way - the sheet's two arm-out rows.
    const Art::Push& push = mage.push;
    CHECK_MSG(push.reachFrameWidthShare == 0.6 && push.reachFrameWidthShare * 40.0 == 24.0,
              "push: 0.6 of getSize().x, 24 units of a 40-unit frame");
    CHECK_MSG(push.offsetPx == glm::dvec2(0.0, -6.0), "push: scale(-6) on y, 6 units above the far end");
    CHECK_MSG(push.airVelocityShare == 0.86, "push: min(1, GetFPSRate() / 60 x 0.86) at 60 Hz");
    CHECK_MSG(push.leftRow == 0 && push.otherRow == 3 && push.leftRow != mage.leftRow &&
                  push.otherRow != mage.rightRow,
              "push: rows 0 and 3, neither a walking row");
    // On a no_gravity level: MainCharacter's constructor swaps the sheet and
    // nothing else (the normal map stays normalmap_77.png), and linearMotion turns
    // the standing column every 280 ms and hovers the pivot 1.2 screen pixels on a
    // cosine at 3 rad/s from PI + PIb, wrapped at 2 PI.
    CHECK_MSG(mage.noGravitySprite == "dark_mage_in_space.png" && mage.normal == "normalmap_77.png",
              "no_gravity: dark_mage_in_space.png, through the same normal map");
    const Art::NoGravityMotion& weightless = mage.noGravity;
    CHECK_MSG(weightless.columnStrideMs == 280.0, "no_gravity: set(0, 3, 280, true)");
    CHECK_MSG(weightless.hoverScreenPx == 1.2 && weightless.hoverAtScreenPx == 720.0 &&
                  weightless.hoverRadiansPerSecond == 3.0,
              "no_gravity: cos(angle) * 1.2 screen px, taken at 720, turning 3 rad a second");
    CHECK_MSG(std::fabs(weightless.hoverStartRadians - (3.1415927 + 1.5707963)) < 1e-6 &&
                  std::fabs(weightless.hoverWrapRadians - 2.0 * 3.1415927) < 1e-6,
              "no_gravity: from PI + PIb, wrapped at PI * 2");
    CHECK_MSG(std::fabs(weightless.HoverUnits(0.0f, 256.0) - 1.2 * 256.0 / 720.0) < 1e-9 &&
                  std::fabs(weightless.HoverUnits(3.1415927f, 256.0) + 1.2 * 256.0 / 720.0) < 1e-6,
              "no_gravity: 0.4267 units either way on a 256-unit view: " +
                  std::to_string(weightless.HoverUnits(0.0f, 256.0)));
    CHECK_MSG(Art::PlayerSheet(mage, true) == mage.noGravitySprite && Art::PlayerSheet(mage, false) == mage.sprite,
              "the sheet worn: the suit only where no_gravity is set");

    // Chapter 1's boss and its spikes (step 11b): the .ent files' facts, and the
    // pulse bounce() gives it in each thing it does, decoded from its script.
    const Art::Beholder& beholder = rules.beholder;
    CHECK_MSG(beholder.sprite == "beholder.png" && !beholder.additive && beholder.columns == 2 && beholder.rows == 1,
              "beholder.ent: its sheet, cut 2 x 1, mixed");
    const auto pulse = [](const Art::Pulse& p, glm::dvec2 from, glm::dvec2 to, double strideMs) {
        return p.fromScale == from && p.toScale == to && p.strideMs == strideMs;
    };
    CHECK_MSG(pulse(beholder.seeking, {1.0, 1.02}, {1.02, 1.0}, 400.0), "seeking, it breathes across");
    CHECK_MSG(pulse(beholder.hurt, {1.0, 1.0}, {1.25, 1.25}, 600.0), "hurt, it swells");
    CHECK_MSG(pulse(beholder.dead, {1.1, 1.1}, {1.25, 1.25}, 400.0), "dead, it throbs");
    CHECK_MSG(rules.spike.sprite == "beholder_spike.png" && !rules.spike.additive && rules.spike.Frames() == 1 &&
                  rules.spike.pivotXPx == 0.0 && rules.spike.pivotYPx == 10.0,
              "beholder_spike.ent: one frame, its pivot 10 px below the middle");

    // Each .ent's <EmissiveColor>, which the layer draws it with (step 45). Only
    // the shot's is below one, so only the shot is ever dimmed by a level's ambient.
    CHECK_MSG(rules.portal.emissive == glm::dvec3(1.0), "portal.ent: emissive 1");
    CHECK_MSG(rules.shot.emissive == glm::dvec3(0.6, 0.6, 1.0), "projectile.ent: emissive (0.6, 0.6, 1)");
    CHECK_MSG(mage.emissive == glm::dvec3(1.0), "dark_mage.ent: emissive 1");
    CHECK_MSG(beholder.emissive == glm::dvec3(1.0), "beholder.ent: emissive 1");
    CHECK_MSG(rules.spike.emissive == glm::dvec3(1.0), "beholder_spike.ent: emissive 1");

    // And their lighting (step 49). Only the player applies light: not static,
    // through normalmap_77.png. Only the shot has a light: projectile.ent's, live,
    // range 70, (0.6, 0.6, 1) 12 below it, with halo.bmp at 50 and 0.65.
    CHECK_MSG(!mage.isStatic && mage.applyLight && mage.normal == "normalmap_77.png" && !mage.light,
              "dark_mage.ent: not static, applies light through normalmap_77.png, no light of its own");
    CHECK_MSG(!rules.shot.isStatic && !rules.shot.applyLight && rules.shot.normal.empty(),
              "projectile.ent: not static, applies no light");
    CHECK_MSG(rules.shot.light.has_value(), "projectile.ent has a light");
    if (rules.shot.light) {
        const Lighting::Light& light = *rules.shot.light;
        CHECK_MSG(light.range == 70.0 && light.offset == glm::dvec3(0.0, 0.0, -12.0) &&
                      light.colour == glm::dvec3(0.6, 0.6, 1.0),
                  "range 70, offset (0, 0, -12), colour (0.6, 0.6, 1)");
        CHECK_MSG(light.halo == "halo.bmp" && light.haloOffset == glm::dvec2(0.0) &&
                      light.haloSize == glm::dvec2(50.0) && light.haloBrightness == 0.65,
                  "halo.bmp, centred, 50 units, brightness 0.65");
        CHECK_MSG(rules.shot.z == 0.0, "measured from the shot's depth, 0");
    }
    for (const Art::Picture* picture : {static_cast<const Art::Picture*>(&rules.portal),
                                        static_cast<const Art::Picture*>(&beholder),
                                        static_cast<const Art::Picture*>(&rules.spike)}) {
        CHECK_MSG(!picture->isStatic && !picture->applyLight && picture->normal.empty() && !picture->light,
                  picture->sprite + ": not static, applies no light, no normal map, no light");
    }

    // A static portal as ETHCallback_portal_static redraws it (bytes 360571..361404):
    // Scale(0.8f) once (ops 14-16), then SetColor by its `color` (ops 31-79), the
    // vector3's last argument pushed first - so 'red' is (1, 0.3, 0.3) and anything
    // else, an absent colour included, (0.3, 0.3, 1).
    const Art::StaticPortal& statics = rules.staticPortal;
    CHECK_MSG(statics.entity == "portal_static" && statics.scale == 0.8 && statics.red == "red",
              "portal_static, scaled by 0.8, red by the colour 'red'");
    CHECK_MSG(statics.tintRed == glm::dvec3(1.0, 0.3, 0.3) && statics.tintOtherwise == glm::dvec3(0.3, 0.3, 1.0),
              "tinted (1, 0.3, 0.3) when red, (0.3, 0.3, 1) otherwise");
    CHECK_MSG(statics.TintFor("red") == statics.tintRed && statics.TintFor("blue") == statics.tintOtherwise &&
                  statics.TintFor("") == statics.tintOtherwise,
              "GetString('color') == 'red': a blue portal and one with no colour both take the other tint");
}

// The static portal's script is not optional: without it the port draws the white,
// unscaled halo the footage refuses (step 59's ring-excess row).
void AStaticPortalWithoutItsScriptIsRefused() {
    std::ifstream file(std::string(MAGICPORTALS_PORT_DATA_DIR) + "/art.json", std::ios::binary);
    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string real = buffer.str();
    CHECK(real.find("\"static_portal\"") != std::string::npos);
    const auto refused = [&real](const std::string& from, const std::string& to, const std::string& name) {
        std::string text = real;
        const std::size_t at = text.find(from);
        CHECK_MSG(at != std::string::npos, from);
        if (at == std::string::npos) return;
        text.replace(at, from.size(), to);
        const std::filesystem::path path = Scratch() / ("art-static-portal-" + name + ".json");
        Write(path, std::vector<unsigned char>(text.begin(), text.end()));
        Art::Rules rules;
        std::string error;
        const bool ok = Art::LoadRules(path.string(), rules, error);
        CHECK_MSG(!ok, "refused: " + name);
        CHECK_MSG(error.find("static_portal needs") != std::string::npos, name + ": " + error);
    };
    refused("\"static_portal\"", "\"static_portal_gone\"", "absent");
    refused("\"scale\": 0.8", "\"scale\": 0", "scale 0");
    refused("\"tint_red\": [1.0, 0.3, 0.3]", "\"tint_red\": [1.0, 0.3]", "two numbers");
    refused("\"tint_otherwise\": [0.3, 0.3, 1.0]", "\"tint_otherwise\": [0.3, -0.3, 1.0]", "below zero");
    refused("\"red\": \"red\"", "\"red\": \"\"", "no red");
}

// The ring round a no-portal field as ETHCallback_antiportal redraws it (art.json
// antiportal): the two colours, the leg and the turn, and the triangle blinkColor
// makes of them. The layer's use of it is test_mp_layer's.
void TheAntiportalBlinksRedAndTurns() {
    Art::Rules rules;
    std::string error;
    const bool ok = Art::LoadRules(std::string(MAGICPORTALS_PORT_DATA_DIR) + "/art.json", rules, error);
    CHECK_MSG(ok, error);
    const Art::Antiportal& anti = rules.antiportal;
    CHECK_MSG(anti.entity == "antiportal", "the callback is named for the entity: " + anti.entity);
    CHECK_MSG(anti.from == glm::dvec3(0.2, 0.0, 0.0) && anti.to == glm::dvec3(0.4, 0.0, 0.0),
              "_portalColorA (0.2, 0, 0) and _portalColorB (0.4, 0, 0): red, and red alone");
    CHECK_MSG(anti.strideMs == 300.0 && anti.spinDegPerS == 10.0, "blinkColor(.., 300) and 10 degrees a second");
    // AntiportalColour: 0.2 at 0, 0.4 at a stride, 0.2 at two - a triangle whose
    // period is 600 ms, with green and blue 0 the whole way.
    const auto red = [&anti](double ms) { return anti.ColourAt(ms); };
    const auto near = [](const glm::dvec3& got, double r) {
        return std::fabs(got.x - r) < 1e-9 && got.y == 0.0 && got.z == 0.0;
    };
    CHECK_MSG(near(red(0.0), 0.2), "0 ms: 0.2");
    CHECK_MSG(near(red(150.0), 0.3), "150 ms: half way up");
    CHECK_MSG(near(red(300.0), 0.4), "300 ms: the top");
    CHECK_MSG(near(red(450.0), 0.3), "450 ms: half way down");
    CHECK_MSG(near(red(600.0), 0.2), "600 ms: back, one period on");
    CHECK_MSG(near(red(900.0), 0.4) && near(red(1200.0), 0.2), "and on: 900 the top, 1200 the bottom");
    CHECK_MSG(near(red(75.0), 0.25) && near(red(525.0), 0.25), "a quarter and three quarters of the way round");
    // Under a pause the port hands it the same millisecond twice, which must draw
    // the same colour, and a negative one never happens but must not read past 0.
    CHECK_MSG(red(123.25) == red(123.25) && near(red(-5.0), 0.2), "steady, and nothing before the level starts");
    // The whole period, against the decode read straight: bias = (ms % 300) / 300,
    // inverted on an odd leg.
    for (int ms = 0; ms <= 1200; ++ms) {
        const int leg = ms / 300;
        double bias = static_cast<double>(ms % 300) / 300.0;
        if (leg % 2 == 1) bias = 1.0 - bias;
        const glm::dvec3 got = red(static_cast<double>(ms));
        if (std::fabs(got.x - (0.2 + 0.2 * bias)) > 1e-9 || got.y != 0.0 || got.z != 0.0) {
            CHECK_MSG(false, "ms " + std::to_string(ms) + ": " + std::to_string(got.x));
            break;
        }
    }
    CHECK(true);
}

// Without the blink the port draws the white ring the footage refuses, and without
// the turn a ring its script turns would stand still.
void AnAntiportalWithoutItsBlinkIsRefused() {
    std::ifstream file(std::string(MAGICPORTALS_PORT_DATA_DIR) + "/art.json", std::ios::binary);
    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string real = buffer.str();
    CHECK(real.find("\"antiportal\"") != std::string::npos);
    const auto refused = [&real](const std::string& from, const std::string& to, const std::string& name) {
        std::string text = real;
        const std::size_t at = text.find(from);
        CHECK_MSG(at != std::string::npos, from);
        if (at == std::string::npos) return;
        text.replace(at, from.size(), to);
        const std::filesystem::path path = Scratch() / ("art-antiportal-" + name + ".json");
        Write(path, std::vector<unsigned char>(text.begin(), text.end()));
        Art::Rules rules;
        std::string error;
        const bool ok = Art::LoadRules(path.string(), rules, error);
        CHECK_MSG(!ok, "refused: " + name);
        CHECK_MSG(error.find("antiportal needs") != std::string::npos, name + ": " + error);
    };
    refused("\"antiportal\": {", "\"antiportal_gone\": {", "absent");
    refused("\"entity\": \"antiportal\",", "\"entity\": \"\",", "no entity");
    refused("\"entity\": \"antiportal\",", "\"entity\": \"antiportal.ent\",", "a name with its extension");
    refused("\"from\": [0.2, 0.0, 0.0]", "\"from\": [0.2, 0.0]", "two numbers");
    refused("\"to\": [0.4, 0.0, 0.0]", "\"to\": [0.4, -0.1, 0.0]", "below zero");
    refused("\"stride_ms\": 300.0", "\"stride_ms\": 0", "a leg of 0");
    refused("\"blink\": {", "\"blink_gone\": {", "no blink");
    refused("\"spin_deg_per_s\": 10.0", "\"spin_deg_per_s\": \"10\"", "a turn that is not a number");
}

// The dial behind a timed crystal (art.json timer): timer.ent's facts, pinned. The
// clock it runs by is test_mp_timed's.
void TheTimerIsTimerEnt() {
    Art::Rules rules;
    std::string error;
    const bool ok = Art::LoadRules(std::string(MAGICPORTALS_PORT_DATA_DIR) + "/art.json", rules, error);
    CHECK_MSG(ok, error);
    const Art::Timer& timer = rules.timer;
    CHECK_MSG(timer.sprite == "timer.png" && !timer.additive && timer.columns == 4 && timer.rows == 2,
              "timer.ent: timer.png, cut 4 x 2, mixed");
    CHECK_MSG(timer.frames == 8 && timer.frames == timer.Frames(), "the time in eight cells, every cell of the cut");
    CHECK_MSG(timer.emissive == glm::dvec3(1.0) && !timer.isStatic && !timer.applyLight && timer.normal.empty() &&
                  !timer.light,
              "emissive 1, not static, applies no light, no normal map, no light");
    CHECK_MSG(timer.framesPerSecond == 0.0, "its cell is chosen by the time, never played");
    CHECK_MSG(timer.alpha == 0.5 && timer.zOffset == -2, "SetAlpha(0.5f), 2 behind its crystal");
}

// Without the timer the port would draw no dial - or the remake's fade, which the
// original never had.
void ATimerWithoutItsClockIsRefused() {
    std::ifstream file(std::string(MAGICPORTALS_PORT_DATA_DIR) + "/art.json", std::ios::binary);
    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string real = buffer.str();
    const auto refused = [&real](const std::string& from, const std::string& to, const std::string& name,
                                 const std::string& says) {
        std::string text = real;
        const std::size_t at = text.find(from);
        CHECK_MSG(at != std::string::npos, from);
        if (at == std::string::npos) return;
        text.replace(at, from.size(), to);
        const std::filesystem::path path = Scratch() / ("art-timer-" + name + ".json");
        Write(path, std::vector<unsigned char>(text.begin(), text.end()));
        Art::Rules rules;
        std::string error;
        const bool ok = Art::LoadRules(path.string(), rules, error);
        CHECK_MSG(!ok, "refused: " + name);
        CHECK_MSG(error.find(says) != std::string::npos, name + ": " + error);
    };
    refused("\"timer\": {", "\"timer_gone\": {", "absent", "timer is an object");
    refused("\"sprite\": \"timer.png\",", "", "no sprite", "timer needs sprite and additive");
    refused("\"frames\": 8,", "\"frames\": 9,", "more frames than cells", "timer needs frames");
    refused("\"alpha\": 0.5,", "\"alpha\": 0,", "alpha 0", "timer needs frames");
    refused("\"z_offset\": -2,", "\"z_offset\": -2.5,", "a z_offset between depths", "timer needs frames");
    refused("\"pulse_to\": 1.15,", "", "no pulse_to", "timer needs frames");
    refused("\"pulse_min_leg_ms\": 400.0,", "\"pulse_min_leg_ms\": 0,", "a leg of 0", "timer needs frames");
    refused("\"shrink_per_frame\": 0.9,", "\"shrink_per_frame\": 1.0,", "no shrink", "timer needs frames");
    refused("\"gone_below_scale\": 0.1", "\"gone_below_scale\": 0", "never gone", "timer needs frames");
    refused("\"screen_px_per_unit\": 2.8125,", "\"screen_px_per_unit\": 0,", "no scale factor", "timer needs frames");
}

// The ghost of a taken light wall (art.json light_wall_ghost, step 80):
// dashed_door.ent's facts and the two numbers ETHCallback_light_wall_ghost runs it
// by, pinned. Where it stands and how long is the layer's and sim/Torch's.
void TheLightWallGhostIsDashedDoorEnt() {
    Art::Rules rules;
    std::string error;
    const bool ok = Art::LoadRules(std::string(MAGICPORTALS_PORT_DATA_DIR) + "/art.json", rules, error);
    CHECK_MSG(ok, error);
    const Art::LightWallGhost& ghost = rules.lightWallGhost;
    CHECK_MSG(ghost.sprite == "dashed_door.png" && !ghost.additive && ghost.Frames() == 1,
              "dashed_door.ent: dashed_door.png, one frame, alpha-blended (blendMode 0)");
    CHECK_MSG(ghost.emissive == glm::dvec3(1.0) && ghost.isStatic && !ghost.applyLight && ghost.normal.empty() &&
                  !ghost.light,
              "emissive 1, static, applies no light, no normal map, no light of its own");
    CHECK_MSG(ghost.framesPerSecond == 0.0, "nothing plays: its callback writes an alpha and a scale");
    // The -4 is added to an already scaled position, so it is raw, and 4 / 2.8125
    // of the level's units is how far behind its wall it is drawn.
    CHECK_MSG(ghost.zOffset == -4.0 && std::fabs(rules.LevelDepth(ghost.zOffset) + 1.4222222222) < 1e-9,
              "seekLightWallToDestroy's raw -4: 1.42 of the level's units behind the wall");
    CHECK_MSG(ghost.alphaBase == 0.2 && ghost.alphaAmplitude == 0.1 && ghost.spinDegPerSec == 300.0,
              "SetAlpha(cos(angle) x 0.1 + 0.2), the angle 300 degrees a second");
    CHECK_MSG(std::fabs(ghost.AlphaAt(0.0) - 0.3) < 1e-6 && std::fabs(ghost.AlphaAt(180.0) - 0.1) < 1e-6 &&
                  std::fabs(ghost.AlphaAt(360.0) - 0.3) < 1e-6,
              "0.3 at 0, 0.1 half a turn on, 0.3 again after 1.2 s");
    CHECK_MSG(ghost.pulse.fromScale == glm::dvec2(1.006) && ghost.pulse.toScale == glm::dvec2(0.994) &&
                  ghost.pulse.strideMs == 160.0,
              "bounceLightEntity(0.15): 1 +- 0.15 x 0.04, 160 ms a leg");
    CHECK_MSG(std::fabs(ghost.pulse.ScaleAt(0.0).x - 1.006) < 1e-9 &&
                  std::fabs(ghost.pulse.ScaleAt(160.0).x - 0.994) < 1e-9 &&
                  std::fabs(ghost.pulse.ScaleAt(320.0).x - 1.006) < 1e-9,
              "there and back again, 320 ms a period");
}

// Without it the port would draw no ghost, or one breathing a rate nobody read.
void AGhostWithoutItsCallbackIsRefused() {
    std::ifstream file(std::string(MAGICPORTALS_PORT_DATA_DIR) + "/art.json", std::ios::binary);
    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string real = buffer.str();
    const auto refused = [&real](const std::string& from, const std::string& to, const std::string& name,
                                 const std::string& says) {
        std::string text = real;
        const std::size_t at = text.find(from);
        CHECK_MSG(at != std::string::npos && text.find(from, at + 1) == std::string::npos, "once: " + from);
        if (at == std::string::npos) return;
        text.replace(at, from.size(), to);
        const std::filesystem::path path = Scratch() / ("art-ghost-" + name + ".json");
        Write(path, std::vector<unsigned char>(text.begin(), text.end()));
        Art::Rules rules;
        std::string error;
        const bool ok = Art::LoadRules(path.string(), rules, error);
        CHECK_MSG(!ok, "refused: " + name);
        CHECK_MSG(error.find(says) != std::string::npos, name + ": " + error);
    };
    refused("\"light_wall_ghost\": {", "\"light_wall_ghost_gone\": {", "absent", "light_wall_ghost is an object");
    refused("\"sprite\": \"dashed_door.png\",", "", "no sprite", "light_wall_ghost needs sprite and additive");
    refused("\"z_offset\": -4.0,", "", "no depth", "light_wall_ghost needs a finite z_offset");
    refused("\"alpha_base\": 0.2,", "\"alpha_base\": 0,", "alpha 0", "light_wall_ghost needs a finite z_offset");
    refused("\"alpha_amplitude\": 0.1,", "\"alpha_amplitude\": 0.3,", "a breath below 0",
            "light_wall_ghost needs a finite z_offset");
    refused("\"spin_deg_per_s\": 300.0,", "\"spin_deg_per_s\": 0,", "a still angle",
            "light_wall_ghost needs a finite z_offset");
    refused("\"stride_ms\": 160.0", "\"stride_ms\": 0", "a leg of 0", "light_wall_ghost needs a finite z_offset");
}

// The minions and knights (art.json minion, step 76): minion.ent's facts and the
// numbers ETHCallback_minion frames it by, pinned; and the depth a script's raw z is
// among the level's art (art.json depth), with the player's decoded 4 (R10).
void TheMinionIsMinionEnt() {
    Art::Rules rules;
    std::string error;
    const bool ok = Art::LoadRules(kPortData + "/art.json", rules, error);
    CHECK_MSG(ok, error);
    const Art::Minion& minion = rules.minion;
    CHECK_MSG(minion.sprite == "minion.png" && !minion.additive && minion.columns == 4 && minion.rows == 4,
              "minion.ent: minion.png, cut 4 x 4, mixed");
    CHECK_MSG(minion.emissive == glm::dvec3(1.0) && !minion.isStatic && !minion.applyLight && minion.normal.empty() &&
                  !minion.light,
              "emissive 1, not static, applies no light, no normal map, no light");
    CHECK_MSG(minion.framesPerSecond == 0.0, "its frame is chosen by its script, never played");
    CHECK_MSG(minion.pivotXPx == 0.0 && minion.pivotYPx == 4.0, "PivotAdjust (0, 4)");
    CHECK_MSG(std::fabs(minion.feetBelowEntityPx - 18.1) < 1e-12, "its feet 9 + 9.1 below the entity");
    CHECK_MSG(minion.firstColumn == 0 && minion.lastColumn == 3 && minion.strideMs == 120.0,
              "the walk: set(0, 3, 120, true)");
    CHECK_MSG(minion.holdColumn == 0 && minion.startRow == 0, "the hold on column 0, and row 0 before any walk");
    CHECK_MSG(minion.leftRow == 1 && minion.rightRow == 2, "the decoded rows: 1 walking left, 2 right (R3)");
    CHECK_MSG(minion.searchPeriodMs == 4000.0 && minion.searchRightAfterMs == 2000.0,
              "a blocked minion looks right while elapsedTime % 4000 > 2000");
    CHECK_MSG(minion.darkestZeroesEmissive, "blind, and unlit, where the ambient is DARKEST_AMBIENT_LIGHT");
    CHECK_MSG(minion.blackEntity == "ghost_minion.ent" && minion.blackColour == glm::dvec3(0.0) &&
                  minion.blackEmissive == glm::dvec3(0.0),
              "the black knight: ghost_minion.ent, SetColor(0), emissive 0");
    // The depths: a raw 2 and a raw 4 in the world's units, over the factor the
    // level's own nodes were multiplied by - the timer's m_scaleFactor, 720 / 256.
    CHECK_MSG(rules.scaleFactor == 2.8125 && rules.scaleFactor == rules.timer.screenPxPerUnit,
              "the scale factor is m_scaleFactor at 720 px, the timer's");
    CHECK_MSG(minion.z == 2.0 && rules.character.z == 4.0, "minion_spawn's 2 and GameCharacter's 4");
    CHECK_MSG(std::fabs(rules.LevelDepth(rules.character.z) - 1.4222222222) < 1e-9 &&
                  std::fabs(rules.LevelDepth(minion.z) - 0.7111111111) < 1e-9,
              "in the level's units: the player at 1.42, between eth_z 1 and 2; a minion at 0.71, behind it");
}

// Without its framing the port would draw a minion on rows, columns or feet nobody
// read; without the depth factor, the player in front of art the original draws over it.
void AMinionWithoutItsFramingIsRefused() {
    std::ifstream file(kPortData + "/art.json", std::ios::binary);
    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string real = buffer.str();
    const auto refused = [&real](const std::string& from, const std::string& to, const std::string& name,
                                 const std::string& says) {
        std::string text = real;
        const std::size_t at = text.find(from);
        CHECK_MSG(at != std::string::npos && text.find(from, at + 1) == std::string::npos, "once: " + from);
        if (at == std::string::npos) return;
        text.replace(at, from.size(), to);
        const std::filesystem::path path = Scratch() / ("art-minion-" + name + ".json");
        Write(path, std::vector<unsigned char>(text.begin(), text.end()));
        Art::Rules rules;
        std::string error;
        const bool ok = Art::LoadRules(path.string(), rules, error);
        CHECK_MSG(!ok, "refused: " + name);
        CHECK_MSG(error.find(says) != std::string::npos, name + ": " + error);
    };
    refused("\"minion\": {", "\"minion_gone\": {", "absent", "minion is an object");
    refused("\"sprite\": \"minion.png\",", "", "no sprite", "minion needs sprite and additive");
    refused("\"feet_below_entity_px\": 18.1,", "", "no feet", "minion needs");
    refused("\"last_column\": 3,", "\"last_column\": 4,", "a walk past the sheet", "minion needs");
    refused("\"stride_ms\": 120.0", "\"stride_ms\": 0", "a walk of no stride", "minion needs");
    refused("\"hold_column\": 0,", "\"hold_column\": 0.5,", "a hold between columns", "minion needs");
    refused("\"start_row\": 0,", "", "no start row", "minion needs");
    refused("\"right_after_ms\": 2000.0", "\"right_after_ms\": 4000.0", "a search that never looks right",
            "minion needs");
    refused("\"darkest_zeroes_emissive\": true,", "\"darkest_zeroes_emissive\": 1,", "a flag not a bool",
            "minion needs");
    refused("\"entity\": \"ghost_minion.ent\",", "\"entity\": \"\",", "no black knight", "minion needs");
    refused("\"scale_factor\": 2.8125", "\"scale_factor\": 0", "no depth factor", "depth needs a scale_factor");
    refused("\"value\": 4.0", "\"value\": \"4\"", "a player depth not a number", "character needs z.value");
    refused("\"value\": 2.0", "\"value\": \"2\"", "a minion depth not a number", "minion needs");
}

// ETHCallback_minion's frame writes, one update at a time (Art::UpdateMinion): the
// walk's row by its way and its column by the FrameTimer, whose column and time
// carry across a hold and a turn; the hold on column 0 of the row last walked, row
// 0 before any; the blocked arm looking right then left; and nothing written where
// the script writes nothing.
void AMinionIsFramedAsItsScriptFramesIt() {
    Art::Rules rules;
    std::string error;
    CHECK_MSG(Art::LoadRules(kPortData + "/art.json", rules, error), error);
    const Art::Minion& art = rules.minion;
    const double tick = 1000.0 / 60.0;

    Art::MinionPose pose;
    pose.row = art.startRow;
    Art::UpdateMinion(art, pose, Art::MinionArm::Hold, false, tick, 0.0);
    CHECK_MSG(pose.column == 0 && pose.row == 0 && pose.dirRow == -1, "held before any walk: SetFrame(0, 0)");

    // Walking right: the first set(0, 3) resets the constructor's (0, 0), then one
    // column each time 120 ms have gathered.
    Art::UpdateMinion(art, pose, Art::MinionArm::Walk, true, tick, 0.0);
    CHECK_MSG(pose.column == 0 && pose.row == 2 && pose.dirRow == 2 && pose.timer.timeMs == 0.0,
              "the first walk: row 2, column 0, no time");
    std::vector<int> steps;
    for (int i = 2; i <= 40; ++i) {
        const int before = pose.column;
        Art::UpdateMinion(art, pose, Art::MinionArm::Walk, true, tick, 0.0);
        if (pose.column != before) steps.push_back(i);
    }
    // Ticks 9, 16, 23, 30, 37: the time the first update reset, then 8 ticks
    // (133 ms) to the first step and 7 or 8 to each after.
    CHECK_MSG(steps.size() == 5 && steps[0] == 9 && steps[1] == 16 && steps[2] == 23 && steps[3] == 30,
              "a column every 120 ms of walking");
    const int walkedTo = pose.column;
    const double timeLeft = pose.timer.timeMs;

    // A hold: column 0 of row 2, and the timer untouched.
    for (int i = 0; i < 30; ++i) Art::UpdateMinion(art, pose, Art::MinionArm::Hold, false, tick, 0.0);
    CHECK_MSG(pose.column == 0 && pose.row == 2 && pose.timer.frame == walkedTo && pose.timer.timeMs == timeLeft,
              "held: column 0 of the row it walked, the walk's column and time kept");

    // Turning left: row 1, the column resuming where the timer stood.
    Art::UpdateMinion(art, pose, Art::MinionArm::Walk, false, tick, 0.0);
    CHECK_MSG(pose.row == 1 && pose.dirRow == 1 && (pose.column == walkedTo || pose.column == (walkedTo + 1) % 4),
              "walking left: row 1, on from the column it had");

    // Settled, or not updated: nothing written.
    const Art::MinionPose kept = pose;
    Art::UpdateMinion(art, pose, Art::MinionArm::Settled, true, tick, 0.0);
    Art::UpdateMinion(art, pose, Art::MinionArm::None, true, tick, 0.0);
    CHECK_MSG(pose.column == kept.column && pose.row == kept.row && pose.timer.timeMs == kept.timer.timeMs,
              "shouldntMove writes no frame");

    // The blocked arm: right while elapsedTime % 4000 > 2000, else left, and stored.
    Art::UpdateMinion(art, pose, Art::MinionArm::Search, false, tick, 1000.0);
    CHECK_MSG(pose.column == 0 && pose.row == 1 && pose.dirRow == 1, "searching at 1000 ms: looking left");
    Art::UpdateMinion(art, pose, Art::MinionArm::Search, false, tick, 2000.0);
    CHECK_MSG(pose.row == 1, "at 2000 ms: still left (not above 2000)");
    Art::UpdateMinion(art, pose, Art::MinionArm::Search, false, tick, 2500.0);
    CHECK_MSG(pose.row == 2 && pose.dirRow == 2, "at 2500 ms: looking right");
    Art::UpdateMinion(art, pose, Art::MinionArm::Search, false, tick, 6500.0);
    CHECK_MSG(pose.row == 2, "at 6500 ms: right again");
    Art::UpdateMinion(art, pose, Art::MinionArm::Hold, false, tick, 0.0);
    CHECK_MSG(pose.row == 2 && pose.column == 0, "and a hold after keeps the row the search stored");
}

void APulseGoesThereAndBack() {
    // bounce(): from one scale to the other in a stride, eased by smoothEnd, and
    // back in the next.
    Art::Pulse p;
    p.fromScale = glm::dvec2(1.0, 1.0);
    p.toScale = glm::dvec2(1.25, 1.5);
    p.strideMs = 600.0;
    const auto near = [](const glm::dvec2& got, double x, double y) {
        return std::fabs(got.x - x) < 1e-5 && std::fabs(got.y - y) < 1e-5;
    };
    const double half = std::sin(3.14159265358979 / 4.0);
    CHECK(near(p.ScaleAt(0.0), 1.0, 1.0));
    CHECK(near(p.ScaleAt(300.0), 1.0 + 0.25 * half, 1.0 + 0.5 * half));
    CHECK(near(p.ScaleAt(600.0), 1.25, 1.5));
    CHECK(near(p.ScaleAt(900.0), 1.25 - 0.25 * (1.0 - half), 1.5 - 0.5 * (1.0 - half)));
    CHECK(near(p.ScaleAt(1200.0), 1.0, 1.0));
}

// ETHCallback_projectile's SetAngle(radianToDegree(getAngle(dir)) - 90), getAngle
// being atan2(x, y) wrapped into [0, 2 PI) (GameMath.h:711-715), in level units
// with y down; and ComputeInScreenSpriteCenter's pivot, x the scale and turned by
// -angle in those axes (ETHSpriteEntity.cpp:699-709). projectile.png's head is its
// +x, so the angle turns the head onto the flight and the centre falls 3.2 units
// behind the entity's point.
void TheShotTurnsItsHeadOntoItsFlight() {
    Art::Rules rules;
    std::string error;
    CHECK_MSG(Art::LoadRules(std::string(MAGICPORTALS_PORT_DATA_DIR) + "/art.json", rules, error), error);
    const Art::Shot& shot = rules.shot;
    const auto nearly = [](double got, double want) { return std::fabs(got - want) < 1e-4; };
    const auto nearV = [](const glm::dvec2& got, double x, double y) {
        return std::fabs(got.x - x) < 1e-5 && std::fabs(got.y - y) < 1e-5;
    };
    const glm::dvec2 at(100.0, 50.0);
    struct Way {
        glm::dvec2 direction;
        double angle;
        glm::dvec2 centre;
        const char* says;
    };
    const Way ways[] = {
        {{1.0, 0.0}, 0.0, {96.8, 50.0}, "right: getAngle 90, less 90 - unturned, the centre 3.2 behind"},
        {{0.0, -1.0}, 90.0, {100.0, 53.2}, "up (y down): getAngle 180, 90 counter-clockwise"},
        {{-1.0, 0.0}, 180.0, {103.2, 50.0}, "left: getAngle 270, turned half round"},
        {{0.0, 1.0}, -90.0, {100.0, 46.8}, "down: getAngle 0, -90"},
        {{-3.0, -3.0}, 135.0, {100.0 + 3.2 * 0.70710678, 50.0 + 3.2 * 0.70710678}, "up-left: getAngle 225"},
        {{250.0, -120.0}, std::atan2(120.0, 250.0) * 180.0 / 3.14159265358979, {0.0, 0.0}, "any length"},
    };
    for (const Way& way : ways) {
        const float angle = shot.AngleDegrees(way.direction);
        CHECK_MSG(nearly(angle, way.angle), std::string(way.says) + ": " + std::to_string(angle));
        if (way.centre != glm::dvec2(0.0)) {
            const glm::dvec2 centre = shot.CentrePx(at, angle);
            CHECK_MSG(nearV(centre, way.centre.x, way.centre.y),
                      std::string(way.says) + ", centre (" + std::to_string(centre.x) + ", " +
                          std::to_string(centre.y) + ")");
        }
        // The head - the pivot's point of the turned picture - is on the entity,
        // and the centre is 3.2 units from it: 4 x 0.8.
        const glm::dvec2 centre = shot.CentrePx(at, angle);
        CHECK_MSG(nearly(glm::length(centre - at), 3.2), std::string(way.says) + ": 3.2 from the centre");
        const glm::dvec2 unit = glm::normalize(way.direction);
        CHECK_MSG(nearV(at - centre, 3.2 * unit.x, 3.2 * unit.y), std::string(way.says) + ": ahead along the flight");
    }
    Art::Shot upright = shot;
    upright.turnsToFlight = false;
    CHECK_MSG(upright.AngleDegrees(glm::dvec2(0.0, -1.0)) == 0.0f && nearV(upright.CentrePx(at, 0.0f), 96.8, 50.0),
              "turn none: upright, the pivot straight across");
}

void ASheetThatDoesNotSayHowFastIsRefused() {
    // Everything else in order, so the missing rate is what is refused: a picture
    // that plays says frames_per_second, and the shot, whose rate is decoded, its
    // stride (planning step 81).
    const auto refused = [](const std::string& portal, const std::string& shot, const std::string& says) {
        const std::filesystem::path path = Scratch() / "art.json";
        const std::string text = R"({"portal": {"sprite": "a.png", "additive": true, "emissive": [1, 1, 1],
                                                "static": false, "apply_light": false)" + portal + R"(},
                                     "shot": {"sprite": "b.png", "additive": true, "columns": 6, "emissive": [1, 1, 1],
                                              "static": false, "apply_light": false)" + shot + R"(},
                                     "torch_light": {"sprite": "d.png", "additive": false, "emissive": [0, 0, 0],
                                                     "static": true, "apply_light": true},
                                     "character": {"sprite": "c.png", "additive": false, "columns": 4, "rows": 4,
                                                   "emissive": [1, 1, 1], "static": false, "apply_light": true,
                                                   "initial_direction": 2, "pivot_px": [0, 2],
                                                   "rows_by_direction": {"left": 1, "right": 2},
                                                   "animation": {"stride_ms": 90, "idle_column": 0}}})";
        Write(path, std::vector<unsigned char>(text.begin(), text.end()));
        Art::Rules rules;
        std::string error;
        const bool ok = Art::LoadRules(path.string(), rules, error);
        CHECK_MSG(!ok, "refused: portal" + portal + " shot" + shot);
        CHECK_MSG(error.find(says) != std::string::npos, error);
    };
    refused(R"(, "columns": 6)", "", "portal has 6 frames and no animation.frames_per_second");
    const std::string drawn = R"(, "scale": {"value": 0.8}, "pivot_px": [4, 0], "turn": "toward_flight_minus_90")";
    refused("", drawn, "shot needs scale.value above 0, pivot_px, turn and animation.stride_ms above 0");
    refused("", drawn + R"(, "animation": {"frames_per_second": 15})", "animation.stride_ms above 0");
    refused("", drawn + R"(, "animation": {"stride_ms": 0})", "animation.stride_ms above 0");
    const std::string timed = R"(, "animation": {"stride_ms": 30})";
    refused("", R"(, "scale": {"value": 0}, "pivot_px": [4, 0], "turn": "none")" + timed, "shot needs scale.value");
    refused("", R"(, "scale": 0.8, "pivot_px": [4, 0], "turn": "none")" + timed, "shot needs scale.value");
    refused("", R"(, "scale": {"value": 0.8}, "pivot_px": [4], "turn": "none")" + timed, "pivot_px");
    refused("", R"(, "scale": {"value": 0.8}, "pivot_px": [4, 0])" + timed, "turn");
    refused("", R"(, "scale": {"value": 0.8}, "pivot_px": [4, 0], "turn": "toward_flight")" + timed,
            "shot's turn is toward_flight_minus_90 or none, not 'toward_flight'");
}

// The player's walk is decoded, so none of it is defaulted: without the way it
// is made facing, the sheet starts on a frame nobody read, and without its
// stride it walks at a rate nobody decoded.
void APlayerWithoutItsWalkIsRefused() {
    std::ifstream file(std::string(MAGICPORTALS_PORT_DATA_DIR) + "/art.json", std::ios::binary);
    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string real = buffer.str();
    const auto refusedSaying = [&real](const std::string& from, const std::string& to, const std::string& name,
                                       const std::string& says) {
        std::string text = real;
        const std::size_t at = text.find(from);
        CHECK_MSG(at != std::string::npos && text.find(from, at + 1) == std::string::npos, "once: " + from);
        if (at == std::string::npos) return;
        text.replace(at, from.size(), to);
        const std::filesystem::path path = Scratch() / ("art-walk-" + name + ".json");
        Write(path, std::vector<unsigned char>(text.begin(), text.end()));
        Art::Rules rules;
        std::string error;
        const bool ok = Art::LoadRules(path.string(), rules, error);
        CHECK_MSG(!ok, "refused: " + name);
        CHECK_MSG(error.find(says) != std::string::npos, name + ": " + error);
    };
    const auto refused = [&refusedSaying](const std::string& from, const std::string& to, const std::string& name) {
        refusedSaying(from, to, name, "character needs initial_direction");
    };
    refused("\"initial_direction\": 2,", "", "no initial direction");
    refused("\"initial_direction\": 2,", "\"initial_direction\": 4,", "a direction below the sheet");
    refused("\"initial_direction\": 2,", "\"initial_direction\": 1.5,", "a direction between rows");
    refused("\"stride_ms\": 90,", "", "no stride");
    refused("\"stride_ms\": 90,", "\"stride_ms\": 0,", "a stride of 0");
    refused("\"idle_column\": 0", "\"idle_column\": 4", "an idle column past the sheet");
    // And what a no_gravity level changes, none of it defaulted either.
    const auto weightless = [&refusedSaying](const std::string& from, const std::string& to, const std::string& name) {
        refusedSaying(from, to, name, "character needs no_gravity_sprite");
    };
    weightless("\"no_gravity_sprite\": \"dark_mage_in_space.png\",", "", "no suit");
    weightless("\"no_gravity_sprite\": \"dark_mage_in_space.png\",", "\"no_gravity_sprite\": \"\",", "an empty suit");
    weightless("\"column_stride_ms\": 280,", "", "no zero-g stride");
    weightless("\"column_stride_ms\": 280,", "\"column_stride_ms\": 0,", "a zero-g stride of 0");
    weightless("\"hover_screen_px\": 1.2,", "", "no hover");
    weightless("\"hover_at_screen_px\": 720,", "\"hover_at_screen_px\": 0,", "a hover at no screen");
    weightless("\"hover_radians_per_second\": 3.0,", "", "no hover rate");
    weightless("\"hover_start_radians\": 4.71238898,", "\"hover_start_radians\": \"PI + PIb\",", "a start not a number");
    weightless("\"hover_wrap_radians\": 6.28318531", "\"hover_wrap_radians\": -1", "a wrap below 0");
    // And its arm out, none of it defaulted.
    const auto arm = [&refusedSaying](const std::string& from, const std::string& to, const std::string& name) {
        refusedSaying(from, to, name, "character needs push.");
    };
    arm("\"reach_frame_width_share\": 0.6,", "", "no reach");
    arm("\"reach_frame_width_share\": 0.6,", "\"reach_frame_width_share\": 0,", "a reach of 0");
    arm("\"offset_px\": [0, -6],", "\"offset_px\": [-6],", "an offset of one number");
    arm("\"air_velocity_share\": 0.86,", "", "no air share");
    arm("\"other\": 3", "\"other\": 4", "a push row past the sheet");
    arm("\"left\": 0,", "\"left\": 0.5,", "a push row between rows");
}

// FrameTimer::set (FrameTimer.angelscript, bytes 23254..23650), as the player's
// column runs on it: the time is always added; a changed range resets to its
// first frame with no time; otherwise ONE frame a call once the time reaches the
// stride, however much time there is, and past the last back to the first.
void AFrameTimerStepsOneFrameACall() {
    Art::FrameTimer timer;
    CHECK_MSG(timer.Set(0, 3, 280.0, true, 16.0) == 0 && timer.timeMs == 0.0 && timer.last == 3,
              "the first set(0, 3) resets the constructor's (0, 0): frame 0, no time");
    CHECK_MSG(timer.Set(0, 3, 280.0, true, 279.0) == 0 && timer.timeMs == 279.0, "279 ms: still frame 0");
    CHECK_MSG(timer.Set(0, 3, 280.0, true, 1.0) == 1 && timer.timeMs == 0.0, "280 ms reached: frame 1, the stride taken off");
    CHECK_MSG(timer.Set(0, 3, 280.0, true, 600.0) == 2 && timer.timeMs == 320.0,
              "600 ms at once is one frame, the rest kept: " + std::to_string(timer.timeMs));
    CHECK_MSG(timer.Set(0, 3, 280.0, true, 0.0) == 3 && timer.timeMs == 40.0, "and the next call takes the next");
    CHECK_MSG(timer.Set(0, 3, 280.0, true, 240.0) == 0 && timer.timeMs == 0.0, "past the last, back to the first");
    CHECK_MSG(timer.Set(0, 3, 90.0, true, 90.0) == 1, "a different stride on the same range does not reset it");
    CHECK_MSG(timer.Set(4, 7, 90.0, true, 500.0) == 4 && timer.timeMs == 0.0, "a different range resets it");
    Art::FrameTimer once;
    once.Set(0, 1, 10.0, false, 0.0);
    once.Set(0, 1, 10.0, false, 10.0);
    CHECK_MSG(once.Set(0, 1, 10.0, false, 15.0) == 1 && once.timeMs == 0.0,
              "not repeating, it holds its last with no time");
}

// MainCharacter on a no_gravity level, one update at a time at the port's tick:
// the column an update draws is the one the update before left, and it turns
// every 280 ms; a walking update draws the walk's own and adds its time twice;
// the hover angle starts at PI + PIb, turns 3 rad a second and wraps at 2 PI.
void AWeightlessPlayerTurnsItsColumnAndHovers() {
    Art::Rules rules;
    std::string error;
    CHECK_MSG(Art::LoadRules(std::string(MAGICPORTALS_PORT_DATA_DIR) + "/art.json", rules, error), error);
    const Art::Character& mage = rules.character;
    const double tickMs = static_cast<double>(1.0f / 60.0f) * 1000.0;
    Art::NoGravityPlayer player;
    CHECK_MSG(!player.moved && player.idleColumn == 0, "made standing on column 0, not yet moved");
    std::vector<int> drawn;
    for (int update = 1; update <= 70; ++update) drawn.push_back(Art::UpdateNoGravity(mage, player, false, tickMs));
    // The first update resets the timer; 17 more updates of 16.67 ms reach 280 on
    // update 18, and update 19 draws it.
    const auto firstDrawing = [&drawn](int column) {
        const auto at = std::find(drawn.begin(), drawn.end(), column);
        return at == drawn.end() ? -1 : static_cast<int>(at - drawn.begin()) + 1;
    };
    CHECK_MSG(firstDrawing(1) == 19 && firstDrawing(2) == 36 && firstDrawing(3) == 53,
              "column 1 first drawn on update " + std::to_string(firstDrawing(1)) + ", 2 on " +
                  std::to_string(firstDrawing(2)) + ", 3 on " + std::to_string(firstDrawing(3)));
    CHECK_MSG(drawn.front() == 0 && drawn[17] == 0 && drawn.back() == 0 && firstDrawing(0) == 1,
              "and back to column 0 by update 70: " + std::to_string(drawn.back()));
    const float angle70 = static_cast<float>(mage.noGravity.hoverStartRadians);
    CHECK_MSG(player.moved && std::fabs(player.angle - std::fmod(angle70 + 70.0f * 3.0f / 60.0f, 6.2831853f)) < 1e-3f,
              "70 updates turn the angle 3.5 rad from PI + PIb, wrapped once: " + std::to_string(player.angle));

    Art::NoGravityPlayer first;
    Art::UpdateNoGravity(mage, first, false, tickMs);
    CHECK_MSG(std::fabs(first.angle - (4.71238898f + 0.05f)) < 1e-5f &&
                  std::fabs(mage.noGravity.HoverUnits(first.angle, 256.0) -
                            1.2 * std::sin(0.05) * 256.0 / 720.0) < 1e-4,
              "the first update already turns it: cos(3 PI / 2 + 0.05), 0.02 units up");

    // Walking: updateFrame's set(0, 3, 90) and linearMotion's set(0, 3, 280) share
    // one timer, so a walking update adds its time twice.
    Art::NoGravityPlayer walker;
    Art::UpdateNoGravity(mage, walker, true, tickMs); // the reset, by updateFrame's set
    CHECK_MSG(walker.timer.timeMs == tickMs, "linearMotion's set adds the same update's time again: " +
                                                 std::to_string(walker.timer.timeMs));
    std::vector<int> walks;
    for (int update = 2; update <= 4; ++update) walks.push_back(Art::UpdateNoGravity(mage, walker, true, tickMs));
    CHECK_MSG(walks == std::vector<int>({0, 0, 1}),
              "so the 90 ms walk column turns on update 4, 50 ms after the reset: " + std::to_string(walks.back()));
}

void APictureWithoutItsEmissiveIsRefused() {
    // The emissive decides how dark a picture is drawn, and a default nobody
    // decoded would be the ambient alone: a black player on a dark level.
    const std::filesystem::path path = Scratch() / "art-no-emissive.json";
    for (const std::string& emissive : {std::string(), std::string(R"(, "emissive": [1, 1])"),
                                        std::string(R"(, "emissive": [1, -0.5, 1])")}) {
        const std::string text = R"({"portal": {"sprite": "a.png", "additive": true, "static": false,
                                                "apply_light": false)" + emissive + R"(},
                                     "shot": {"sprite": "b.png", "additive": true, "emissive": [1, 1, 1],
                                              "static": false, "apply_light": false},
                                     "torch_light": {"sprite": "d.png", "additive": false, "emissive": [0, 0, 0],
                                                     "static": true, "apply_light": true},
                                     "character": {"sprite": "c.png", "additive": false, "emissive": [1, 1, 1],
                                                   "static": false, "apply_light": true}})";
        Write(path, std::vector<unsigned char>(text.begin(), text.end()));
        Art::Rules rules;
        std::string error;
        const bool ok = Art::LoadRules(path.string(), rules, error);
        CHECK_MSG(!ok, "refused: portal" + emissive);
        CHECK_MSG(error.find("portal's emissive") != std::string::npos, error);
    }
}

void APictureWithoutItsLightingIsRefused() {
    // Which lights reach a picture decides whether the player is lit at all, and
    // a light's every number is its .ent's: none of it is defaulted.
    const std::filesystem::path path = Scratch() / "art-lighting.json";
    const auto refused = [&](const std::string& portal, const std::string& shot, const std::string& says) {
        const std::string text = R"({"portal": {"sprite": "a.png", "additive": true, "emissive": [1, 1, 1])" + portal +
                                 R"(}, "shot": {"sprite": "b.png", "additive": true, "emissive": [1, 1, 1],
                                                "static": false, "apply_light": false)" + shot +
                                 R"(}, "torch_light": {"sprite": "d.png", "additive": false, "emissive": [0, 0, 0],
                                                       "static": true, "apply_light": true},
                                     "character": {"sprite": "c.png", "additive": false, "emissive": [1, 1, 1],
                                                     "static": false, "apply_light": true}})";
        Write(path, std::vector<unsigned char>(text.begin(), text.end()));
        Art::Rules rules;
        std::string error;
        const bool ok = Art::LoadRules(path.string(), rules, error);
        CHECK_MSG(!ok, "refused: portal" + portal + " shot" + shot);
        CHECK_MSG(error.find(says) != std::string::npos, error);
    };
    const std::string flags = R"(, "static": false, "apply_light": false)";
    const std::string light = R"(, "light": {"range": 70, "offset": [0, 0, -12], "colour": [0.6, 0.6, 1],
                                  "halo": "halo.bmp", "halo_offset": [0, 0], "halo_size": [50, 50],
                                  "halo_brightness": 0.65}, "z": {"value": 0})";
    refused("", "", "portal needs static and apply_light");
    refused(R"(, "static": false)", "", "portal needs static and apply_light");
    refused(R"(, "static": 0, "apply_light": false)", "", "portal needs static and apply_light");
    refused(flags + R"(, "normal": "")", "", "portal's normal is not a file name");
    // The shot's light, with each of its numbers taken out or broken in turn.
    const auto without = [&light](const std::string& from, const std::string& to) {
        std::string changed = light;
        const std::size_t at = changed.find(from);
        CHECK_MSG(at != std::string::npos, from);
        if (at != std::string::npos) changed.replace(at, from.size(), to);
        return changed;
    };
    refused(flags, without(R"("range": 70, )", ""), "shot's light needs a range above 0");
    refused(flags, without(R"("range": 70)", R"("range": 0)"), "shot's light needs a range above 0");
    refused(flags, without(R"([0, 0, -12])", "[0, -12]"), "shot's light needs");
    refused(flags, without(R"([0.6, 0.6, 1])", "[0.6, -0.6, 1]"), "shot's light needs");
    refused(flags, without(R"("halo": "halo.bmp", )", ""), "shot's light needs");
    refused(flags, without(R"([50, 50])", "[50, 0]"), "shot's light needs");
    refused(flags, without(R"("halo_brightness": 0.65)", R"("halo_brightness": "0.65")"), "shot's light needs");
    refused(flags, without(R"(, "z": {"value": 0})", ""), "shot has a light and no z.value");
}

void TheOriginalsImagesAreCutAsTheEntsSay() {
    int w = 0;
    int h = 0;
    std::string error;
    const bool halo = Sprites::ImageSize(kOriginal + "/entities/portal_halo.png", w, h, error);
    CHECK_MSG(halo && w == 64 && h == 64, "portal_halo.png: " + error);
    const bool sheet = Sprites::ImageSize(kOriginal + "/entities/projectile.png", w, h, error);
    CHECK_MSG(sheet && w == 6 * 64 && h == 64, "projectile.png is six frames of 64 x 64: " + error);
    const bool mage = Sprites::ImageSize(kOriginal + "/entities/magic_portals_hd.png", w, h, error);
    CHECK_MSG(mage && w == 4 * 40 && h == 4 * 56, "magic_portals_hd.png is sixteen frames of 40 x 56: " + error);
    const bool beholder = Sprites::ImageSize(kOriginal + "/entities/beholder.png", w, h, error);
    CHECK_MSG(beholder && w == 2 * 128 && h == 128, "beholder.png is two frames of 128 x 128: " + error);
    const bool spike = Sprites::ImageSize(kOriginal + "/entities/beholder_spike.png", w, h, error);
    CHECK_MSG(spike && w == 16 && h == 32, "beholder_spike.png is 16 x 32: " + error);
    // The player's normal map is sampled with the sheet's own coordinates, so it is
    // cut 4 x 4 like the sheet: sixteen cells of 32 x 32 (step 49). And the shot's
    // halo is halo.bmp, an 8-bit BMP the header reader sizes.
    const bool normal = Sprites::ImageSize(kOriginal + "/entities/normalmaps/normalmap_77.png", w, h, error);
    CHECK_MSG(normal && w == 4 * 32 && h == 4 * 32, "normalmap_77.png is sixteen cells of 32 x 32: " + error);
    const bool halo2 = Sprites::ImageSize(kOriginal + "/entities/halo.bmp", w, h, error);
    CHECK_MSG(halo2 && w == 64 && h == 64, "halo.bmp is 64 x 64: " + error);
    // The timed crystal's dial: eight cells of 32 x 32, and no hd or fullhd twin, so
    // the 1x picture is the one every screen draws.
    const bool timer = Sprites::ImageSize(kOriginal + "/entities/timer.png", w, h, error);
    CHECK_MSG(timer && w == 4 * 32 && h == 2 * 32, "timer.png is eight cells of 32 x 32: " + error);
    std::error_code ec;
    CHECK_MSG(!std::filesystem::exists(kOriginal + "/entities/hd/timer.png", ec) &&
                  !std::filesystem::exists(kOriginal + "/entities/fullhd/timer.png", ec),
              "timer.png has no hd or fullhd twin");
    // The one JPEG a particle system names (K16): its frame header, after an Exif
    // segment and two tables, says 32 x 32.
    const bool explosion = Sprites::ImageSize(kOriginal + "/particles/explosion.JPG", w, h, error);
    CHECK_MSG(explosion && w == 32 && h == 32, "explosion.JPG is 32 x 32: " + error);
}

// The one sheet of the port's own pictures an even split does NOT cut where the
// original does (00_order E1, K5; step 76): hd/minion.png, 310 x 434 texels at
// density 2, is 155 x 217 units and frames of 38 x 54 units - 76 x 108 texels -
// so its uv is its stride over the whole, 38 / 155 and 54 / 217, and column 3
// starts 4.5 texels left of where a quarter of the width would put it.
void TheMinionSheetIsCutWhereTheOriginalCutsIt() {
    Art::Rules rules;
    std::string error;
    CHECK_MSG(Art::LoadRules(kPortData + "/art.json", rules, error), error);
    Sprites::Image image;
    const bool drawn = Sprites::Drawn(TheTiers(), kOriginal + "/entities/" + rules.minion.sprite, image, error);
    CHECK_MSG(drawn && image.tier == "hd" && image.texels == glm::ivec2(310, 434) && image.density == 2.0f &&
                  image.units == glm::ivec2(155, 217),
              "minion.png is drawn from hd/, 310 x 434 at density 2: " + image.path + " " + error);
    Tiers::Cut cut;
    CHECK_MSG(Tiers::FrameCut(image.texels, image.density, rules.minion.columns, rules.minion.rows, cut, error) &&
                  cut.frameUnits == glm::ivec2(38, 54) && cut.frameTexels == glm::dvec2(76.0, 108.0),
              "frames of 38 x 54 units, 76 x 108 texels " + error);
    CHECK_MSG(std::fabs(cut.uvScale.x - 38.0 / 155.0) < 1e-12 && std::fabs(cut.uvScale.y - 54.0 / 217.0) < 1e-12,
              "a frame samples 38 / 155 by 54 / 217 of the texture");
    const glm::dvec2 third = Tiers::UvOffset(cut, 3, 2);
    CHECK_MSG(std::fabs((0.75 - third.x) * 310.0 - 4.5) < 1e-9 && std::fabs(third.y - 108.0 / 217.0) < 1e-12,
              "column 3 at 228 of 310 texels, 4.5 left of an even split's; row 2 at 216 of 434");
    // Drawn at a 720 px screen's 2.8125 px a unit: 106.9 x 151.9 px (G5a-1).
    CHECK_MSG(std::fabs(38.0 * rules.scaleFactor - 106.875) < 1e-9 && std::fabs(54.0 * rules.scaleFactor - 151.875) < 1e-9,
              "106.875 x 151.875 px on the screen");
}

// 00_order 2.3: the pictures the port draws for itself and every particle bitmap, by
// the names its data gives, through the rule a level's art takes (Sprites::Drawn).
// The APK's hd files for the player's two sheets, the beholder, the rolling stone, a
// gravity well's ring and two particle bitmaps are each exactly twice their 1x file,
// so every frame keeps the 1x file's units, and the engine's even split
// (SpriteAnimationComponent) cuts each sheet where Tiers::FrameCut does. The rest,
// a timed crystal's dial among them, have no tier file and draw as named.
void ThePortsOwnPicturesAreDrawnFromTheirTiers() {
    Art::Rules rules;
    std::string error;
    CHECK_MSG(Art::LoadRules(kPortData + "/art.json", rules, error), error);
    Loading::Rules loading;
    CHECK_MSG(Loading::LoadRules(kPortData + "/ui.json", loading, error), error);
    GravityWell::Rules wells;
    CHECK_MSG(GravityWell::LoadRules(kPortData + "/gravitywell.json", wells, error), error);
    const Tiers::Rules tiers = TheTiers();
    const std::string entities = kOriginal + "/entities/";

    struct Sheet {
        std::string name;
        int columns;
        int rows;
        std::string tier;
        glm::ivec2 texels;
        glm::ivec2 frame;
    };
    const Sheet sheets[] = {
        {rules.character.sprite, rules.character.columns, rules.character.rows, "hd", {320, 448}, {40, 56}},
        {rules.character.noGravitySprite, rules.character.columns, rules.character.rows, "hd", {320, 448}, {40, 56}},
        {rules.beholder.sprite, rules.beholder.columns, rules.beholder.rows, "hd", {512, 256}, {128, 128}},
        {rules.shot.sprite, rules.shot.columns, rules.shot.rows, "", {384, 64}, {64, 64}},
        {rules.portal.sprite, rules.portal.columns, rules.portal.rows, "", {64, 64}, {64, 64}},
        {rules.spike.sprite, rules.spike.columns, rules.spike.rows, "", {16, 32}, {16, 32}},
        {"normalmaps/" + rules.character.normal, 1, 1, "", {128, 128}, {128, 128}},
        {"normalmaps/" + rules.torchLight.normal, 1, 1, "", {32, 64}, {32, 64}},
        // The two pictures the port adds for itself since the merge of steps 68-73:
        // a timed crystal's dial, timer.png as named, eight cells of 32 x 32; and a
        // gravity well's ring, hd/white_ring.png, 256 texels at density 2 - the 1x
        // file's 128 units, which scaleToSize then sizes to the well.
        {rules.timer.sprite, rules.timer.columns, rules.timer.rows, "", {128, 64}, {32, 32}},
        {wells.ring.sprite, 1, 1, "hd", {256, 256}, {128, 128}},
    };
    for (const Sheet& sheet : sheets) {
        Sprites::Image image;
        error.clear();
        const bool drawn = Sprites::Drawn(tiers, entities + sheet.name, image, error);
        std::string expected = entities + sheet.name;
        if (!sheet.tier.empty()) {
            const std::size_t slash = expected.rfind('/');
            expected = expected.substr(0, slash) + "/" + sheet.tier + expected.substr(slash);
        }
        CHECK_MSG(drawn && image.path == expected && image.tier == sheet.tier && image.texels == sheet.texels &&
                      image.density == (sheet.tier.empty() ? 1.0f : 2.0f),
                  sheet.name + ": drawn from " + image.path + " " + error);
        Tiers::Cut cut;
        CHECK_MSG(Tiers::FrameCut(image.texels, image.density, sheet.columns, sheet.rows, cut, error) &&
                      cut.frameUnits == sheet.frame,
                  sheet.name + ": a frame of its units " + error);
        // The 1x file's even split, which the port drew the frame at before.
        int w = 0;
        int h = 0;
        CHECK_MSG(Sprites::ImageSize(entities + sheet.name, w, h, error) &&
                      glm::dvec2(w, h) / glm::dvec2(sheet.columns, sheet.rows) == glm::dvec2(sheet.frame),
                  sheet.name + ": the same frame as its 1x file's");
        CHECK_MSG(std::fabs(cut.uvScale.x - 1.0 / sheet.columns) < 1e-12 &&
                      std::fabs(cut.uvScale.y - 1.0 / sheet.rows) < 1e-12,
                  sheet.name + ": cut where an even split cuts it");
    }

    // The halos, and black_halo.ent's picture for the loading screen: no tier file.
    for (const std::string& named : {entities + rules.shot.light->halo, entities + rules.torchLight.light->halo,
                                     kOriginal + "/" + loading.haloSprite}) {
        Sprites::Image image;
        CHECK_MSG(Sprites::Drawn(tiers, named, image, error) && image.path == named && image.density == 1.0f,
                  named + " is drawn as named: " + image.path);
    }

    // Every particle bitmap every .ent names: two have an hd file, each twice its 1x
    // file, and none is unreadable or refused.
    std::vector<std::string> bitmaps;
    std::error_code ec;
    for (std::filesystem::directory_iterator it(kOriginal + "/entities", ec), end; !ec && it != end; it.increment(ec)) {
        if (it->path().extension() != ".ent") continue;
        std::vector<Particles::System> systems;
        if (!Particles::Load(it->path().generic_string(), systems, error)) continue;
        for (const Particles::System& system : systems) {
            if (std::find(bitmaps.begin(), bitmaps.end(), system.bitmap) == bitmaps.end()) {
                bitmaps.push_back(system.bitmap);
            }
        }
    }
    std::sort(bitmaps.begin(), bitmaps.end());
    std::vector<std::string> tiered;
    int unread = 0;
    for (const std::string& bitmap : bitmaps) {
        Sprites::Image image;
        const std::string named = kOriginal + "/particles/" + bitmap;
        if (!Sprites::Drawn(tiers, named, image, error)) {
            ++unread;
            std::printf("  particle bitmap not drawn: %s\n", error.c_str());
            continue;
        }
        if (image.tier.empty()) {
            CHECK_MSG(image.path == named && image.density == 1.0f, bitmap);
            continue;
        }
        tiered.push_back(bitmap + " " + image.tier);
        int w = 0;
        int h = 0;
        CHECK_MSG(Sprites::ImageSize(named, w, h, error) && image.units == glm::ivec2(w, h),
                  bitmap + ": its tier file keeps the 1x file's units");
    }
    std::printf("  %zu particle bitmaps named by the .ent files; drawn from a tier file: %zu\n", bitmaps.size(),
                tiered.size());
    CHECK_MSG(bitmaps.size() >= 20, std::to_string(bitmaps.size()) + " particle bitmaps");
    CHECK_EQ(unread, 0);
    CHECK_MSG(tiered == std::vector<std::string>({"crystal_particle.png hd", "question_mark.png hd"}),
              "crystal_particle.png and question_mark.png draw from hd, and nothing else");

    // The thrown stone, from the converter's copies beside the levels.
    std::error_code converted;
    if (!std::filesystem::is_directory(kLevels + "/../assets/entities/hd", converted)) {
        std::printf("  the thrown stone's tier file SKIPPED - needs the converter's copies beside %s\n",
                    kLevels.c_str());
        return;
    }
    Sprites::Image stone;
    const std::string named = kLevels + "/../assets/entities/rolling_stone.png";
    CHECK_MSG(Sprites::Drawn(tiers, named, stone, error) && stone.tier == "hd" &&
                  stone.texels == glm::ivec2(128, 128) && stone.units == glm::ivec2(64, 64),
              "a thrown rolling_stone.png: its hd copy, 64 units: " + stone.path + " " + error);
}

// The header reader and the renderer's decoder agree on every image the original
// ships: the same size where stb_image reads a header (stbi_info), and a refusal
// where it does not. So no particle system or sprite is sized for a texture that
// draws as the fallback, nor refused for one that would draw.
void EveryOriginalImageIsSizedAsTheRendererReadsIt() {
    int images = 0;
    int jpegs = 0;
    int disagree = 0;
    std::error_code ec;
    for (std::filesystem::recursive_directory_iterator it(kOriginal, ec), end; !ec && it != end; it.increment(ec)) {
        if (!it->is_regular_file(ec)) continue;
        std::string extension = it->path().extension().string();
        for (char& c : extension) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (extension != ".png" && extension != ".bmp" && extension != ".jpg" && extension != ".jpeg") continue;
        ++images;
        if (extension == ".jpg" || extension == ".jpeg") ++jpegs;
        const std::string path = it->path().string();
        int w = 0;
        int h = 0;
        std::string error;
        const bool sized = Sprites::ImageSize(path, w, h, error);
        int sw = 0;
        int sh = 0;
        int channels = 0;
        const bool read = stbi_info(path.c_str(), &sw, &sh, &channels) != 0;
        if (sized != read || (sized && (w != sw || h != sh))) {
            ++disagree;
            CHECK_MSG(false, path + ": the reader says " + (sized ? std::to_string(w) + " x " + std::to_string(h) : error) +
                                 ", stb_image " + (read ? std::to_string(sw) + " x " + std::to_string(sh) : "no size"));
        }
    }
    std::printf("  the original's images: %d (%d JPEG), %d sized otherwise than stb_image reads them\n", images, jpegs,
                disagree);
    CHECK_MSG(images > 1000 && jpegs == 2, "the original ships over 1,000 images, two of them JPEGs: " +
                                               std::to_string(images) + ", " + std::to_string(jpegs));
    CHECK_EQ(disagree, 0);
}

// ---- the converted levels -----------------------------------------------------

bool LoadLevel(const std::string& level, std::vector<Sprites::Sprite>& out) {
    Tscn::Scene scene;
    std::string error;
    const bool ok = Tscn::Load(kLevels + "/" + level + ".tscn", scene, error) &&
                    Sprites::Find(scene, kLevels + "/..", TheTiers(), out, error);
    CHECK_MSG(ok, level + ": " + error);
    return ok;
}

const Sprites::Sprite* Of(const std::vector<Sprites::Sprite>& sprites, const std::string& node) {
    for (const Sprites::Sprite& sprite : sprites) {
        if (sprite.node == node) return &sprite;
    }
    return nullptr;
}

std::string FileOf(const Sprites::Sprite* sprite) {
    return sprite != nullptr ? std::filesystem::path(sprite->texture).filename().string() : std::string();
}

// The folder the drawn file sits in: "fullhd" or "hd" for a tier file, "entities" for the 1x file.
std::string FolderOf(const Sprites::Sprite* sprite) {
    return sprite != nullptr ? std::filesystem::path(sprite->texture).parent_path().filename().string()
                             : std::string();
}

void Level8sArt() {
    std::vector<Sprites::Sprite> sprites;
    if (!LoadLevel("level8", sprites)) return;
    CHECK_EQ(sprites.size(), std::size_t{23});

    bool inOrder = true;
    bool present = true;
    int added = 0;
    for (std::size_t i = 0; i < sprites.size(); ++i) {
        if (sprites[i].order != static_cast<int>(i) || (i > 0 && sprites[i - 1].zIndex > sprites[i].zIndex)) {
            inOrder = false;
        }
        std::error_code ec;
        if (!std::filesystem::is_regular_file(sprites[i].texture, ec)) present = false;
        if (sprites[i].additive) ++added;
    }
    CHECK_MSG(inOrder, "numbered from the back, and never a lower z_index in front of a higher");
    CHECK_MSG(present, "every image the level names is where res:// says");
    CHECK_MSG(added == 0, "level8 has no glow");
    CHECK_MSG(!sprites.empty() && sprites.front().node == "sky_629", "the sky, at z_index -100, is drawn first");

    // Each drawn from the tier file the original draws (tiers.json), at its units.
    // The fullhd sky is 1024 x 512 over a 455 x 256 1x file, so it is 512 u wide:
    // the one size the tiers change (step 66). The hd files are exactly twice
    // their 1x files, so they keep the 1x size; the rest have no tier file.
    const Sprites::Sprite* sky = Of(sprites, "sky_629");
    CHECK_MSG(sky != nullptr && FileOf(sky) == "icy_sky.png" && FolderOf(sky) == "fullhd" && sky->tier == "fullhd" &&
                  sky->texels == glm::ivec2(1024, 512) && sky->sizePx == glm::dvec2(512.0, 256.0) &&
                  sky->atPx == glm::dvec2(227.5, 128.0) && sky->zIndex == -100,
              "the sky, fullhd/icy_sky.png at 512 x 256: " + (sky != nullptr ? sky->texture : std::string()));
    const Sprites::Sprite* wall = Of(sprites, "wall_w3_ent_832");
    CHECK(wall != nullptr && FileOf(wall) == "wall_w3.png" && FolderOf(wall) == "entities" && wall->tier.empty() &&
          wall->sizePx == glm::dvec2(256.0, 256.0) && wall->atPx == glm::dvec2(640.0, 128.0) && wall->zIndex == -20 &&
          wall->offsetPx == glm::dvec2(0.0));
    const Sprites::Sprite* torch = Of(sprites, "light_ent_648");
    CHECK_MSG(torch != nullptr && FileOf(torch) == "torch_small.png" && FolderOf(torch) == "hd" &&
                  torch->texels == glm::ivec2(64, 128) && torch->sizePx == glm::dvec2(32.0, 64.0) &&
                  torch->offsetPx == glm::dvec2(0.0, 16.0) && torch->zIndex == -16,
              "the torch, hd, its offset the level's");
    const Sprites::Sprite* doorway = Of(sprites, "door_bg_ent_596");
    CHECK_MSG(doorway != nullptr && FileOf(doorway) == "black.bmp" && doorway->tier.empty() &&
                  doorway->sizePx == glm::dvec2(40.0, 100.0) && doorway->zIndex == -18,
              "the one BMP the converter copies, sized from its header");
    const Sprites::Sprite* wallToBreak = Of(sprites, "breakable_wall_625");
    CHECK(wallToBreak != nullptr && FileOf(wallToBreak) == "breakable_wall_sand.png" && FolderOf(wallToBreak) == "hd" &&
          wallToBreak->sizePx == glm::dvec2(32.0, 128.0) && wallToBreak->zIndex == -2);
    const Sprites::Sprite* crystal = Of(sprites, "crystal_ent_790");
    CHECK(crystal != nullptr && FileOf(crystal) == "crystal.png" && FolderOf(crystal) == "hd" &&
          crystal->texels == glm::ivec2(64, 64) && crystal->sizePx == glm::dvec2(32.0, 32.0) &&
          crystal->atPx == glm::dvec2(83.0, 32.0) && crystal->zIndex == 0);

    // Two platforms at one z_index: the file's order decides.
    const Sprites::Sprite* first = Of(sprites, "moving_platform_single_ent_604");
    const Sprites::Sprite* second = Of(sprites, "moving_platform_single_ent_603");
    CHECK_MSG(first != nullptr && second != nullptr && first->zIndex == second->zIndex && first->order < second->order,
              "at one z_index, the one written first is drawn first");
}

void Level0sStaticPortalsAreAdded() {
    std::vector<Sprites::Sprite> sprites;
    if (!LoadLevel("level0", sprites)) return;
    CHECK_EQ(sprites.size(), std::size_t{18});
    int added = 0;
    for (const Sprites::Sprite& sprite : sprites) {
        if (sprite.additive) ++added;
    }
    CHECK_EQ(added, 4);
    for (const char* name : {"portal_static_604", "portal_static_603", "portal_static_582", "portal_static_583"}) {
        const Sprites::Sprite* portal = Of(sprites, name);
        CHECK_MSG(portal != nullptr && portal->additive && FileOf(portal) == "portal_halo.png" && portal->tier.empty() &&
                      portal->sizePx == glm::dvec2(64.0, 64.0),
                  std::string(name) + " is the halo, added");
    }
}

void AnOffsetAndATurn() {
    std::vector<Sprites::Sprite> boss;
    if (LoadLevel("level31a", boss)) {
        const Sprites::Sprite* dragon = Of(boss, "dragon_ent_1279");
        CHECK_MSG(dragon != nullptr && dragon->offsetPx == glm::dvec2(48.0, -7.0) && dragon->zIndex == 4,
                  "the one sprite offset sideways");
    }
    std::vector<Sprites::Sprite> tutorial;
    if (LoadLevel("level1", tutorial)) {
        const Sprites::Sprite* block = Of(tutorial, "block00_ent_598");
        CHECK_MSG(block != nullptr && Near(block->rotation, 0.7854), "an eighth of a turn, as its node has");
    }
}

// THE PARTICLE SYSTEMS THE LEVELS PLACE (00_order 8.3, planning step 91): what
// each placement's own <Particles> block holds, read as the layer reads it -
// the eth_particles path out of Lighting::Read, the sidecar through
// Particles::Load - and nothing else, since a node that names no sidecar draws no
// system. Counted over the 128 levels the converter wrote (step 90), and four
// carriers pinned value for value where the level states what its entity's .ent
// does not (systems_9_10 9c (3)).
void ThePlacementsCarryTheSystemsTheirLevelsState() {
    const Tiers::Rules tiers = TheTiers();
    int levels = 0;
    int carriers = 0;
    int systems = 0;
    int added = 0;
    int mixed = 0; // AM_PIXEL
    int twoSystems = 0;
    int unreadable = 0;
    std::set<std::string> files;
    std::map<std::string, std::vector<Particles::System>> spots; // "level/node" -> its systems
    const std::set<std::string> spotNodes = {"level7/blue_light_ent_930", "level1/portal_static_582",
                                             "level7b/key_ent_1335", "level0c/door_ent_2079"};
    for (const char* suffix : {"", "a", "b", "c"}) {
        for (int i = 0; i < 32; ++i) {
            const std::string level = "level" + std::to_string(i) + suffix;
            Tscn::Scene scene;
            Lighting::Scene look;
            std::string error;
            const bool read = Tscn::Load(kLevels + "/" + level + ".tscn", scene, error) &&
                              Lighting::Read(scene, kLevels + "/..", tiers, look, error);
            CHECK_MSG(read, level + ": " + error);
            if (!read) continue;
            ++levels;
            for (const auto& [node, l] : look.nodes) {
                if (l.particles.empty()) continue;
                ++carriers;
                files.insert(l.particles);
                std::vector<Particles::System> held;
                std::string why;
                if (!Particles::Load(l.particles, held, why) || held.empty()) {
                    ++unreadable;
                    CHECK_MSG(false, level + "/" + node + ": " + l.particles + " " + why);
                    continue;
                }
                systems += static_cast<int>(held.size());
                if (held.size() == 2) ++twoSystems;
                for (const Particles::System& s : held) {
                    if (s.alphaMode == Particles::kAlphaAdd) ++added;
                    if (s.alphaMode == Particles::kAlphaPixel) ++mixed;
                }
                if (spotNodes.count(level + "/" + node) != 0) spots[level + "/" + node] = held;
            }
        }
    }
    std::printf("  placed particles: %d levels, %d carriers naming %zu sidecars, %d systems (%d added, %d mixed), "
                "%d carriers with two\n",
                levels, carriers, files.size(), systems, added, mixed, twoSystems);
    CHECK_EQ(levels, 128);
    CHECK_EQ(unreadable, 0);
    CHECK_EQ(carriers, 840);
    CHECK_EQ(systems, 897);
    CHECK_EQ(added, 887);
    CHECK_EQ(mixed, 10);
    CHECK_EQ(twoSystems, 57);
    CHECK_EQ(files.size(), std::size_t{24});

    // 1-08's blue lamp: an orb its .ent lacks (blue_light.ent holds no system).
    const std::vector<Particles::System>& lamp = spots["level7/blue_light_ent_930"];
    CHECK_EQ(lamp.size(), std::size_t{1});
    if (lamp.size() == 1) {
        const Particles::System& orb = lamp[0];
        CHECK_MSG(orb.bitmap == "hard_halo.png" && orb.count == 4 && orb.additive && orb.lifeTimeMs == 1000.0 &&
                      orb.randomLifeTimeMs == 0.0 && orb.size == 30.0 && orb.startPoint == glm::dvec2(0.0, -24.0) &&
                      orb.startPointZ == 24.0 && orb.colour0 == glm::dvec4(0.3, 0.5, 1.0, 1.0),
                  "level7 #930: 4 hard_halo.png, added, life 1000, size 30, 24 above its node at z 24, (0.3, 0.5, 1)");
    }
    // 1-02's static portal: 4 rings (its .ent's 3) and an iris that lives 1 ms (100).
    const std::vector<Particles::System>& portal = spots["level1/portal_static_582"];
    CHECK_EQ(portal.size(), std::size_t{2});
    if (portal.size() == 2) {
        CHECK_MSG(portal[0].bitmap == "portal_particle.png" && portal[0].count == 4 && portal[0].additive &&
                      portal[0].lifeTimeMs == 900.0 && portal[0].randomLifeTimeMs == 100.0,
                  "level1 #582 sys0: 4 rings, life 900 +- 100");
        CHECK_MSG(portal[1].bitmap == "portal.png" && portal[1].count == 1 &&
                      portal[1].alphaMode == Particles::kAlphaPixel && portal[1].lifeTimeMs == 1.0,
                  "level1 #582 sys1: the iris, mixed, life 1");
    }
    // A key of chapter 3: two warmer sparkles, blue 0.7 where key.ent has 1.
    const std::vector<Particles::System>& key = spots["level7b/key_ent_1335"];
    CHECK_EQ(key.size(), std::size_t{2});
    for (const Particles::System& s : key) {
        CHECK_MSG(s.bitmap == "sparkles.bmp" && s.colour0 == glm::dvec4(1.0, 1.0, 0.7, 1.0),
                  "level7b #1335: sparkles.bmp born (1, 1, 0.7)");
    }
    // 4-01's door, the rocket: two systems door.ent does not have.
    const std::vector<Particles::System>& rocket = spots["level0c/door_ent_2079"];
    CHECK_EQ(rocket.size(), std::size_t{2});
    if (rocket.size() == 2) {
        CHECK_MSG(rocket[0].bitmap == "sparkles.bmp" && rocket[0].count == 24 && rocket[0].size == 99.0 &&
                      rocket[0].startPoint == glm::dvec2(0.0, 118.0),
                  "level0c #2079 sys0: 24 sparkles, size 99, 118 below its node");
        CHECK_MSG(rocket[1].bitmap == "fire.png" && rocket[1].count == 12 && rocket[1].columns == 4 &&
                      rocket[1].startPoint == glm::dvec2(0.0, 22.0),
                  "level0c #2079 sys1: 12 flames on a 4-frame cut, 22 below its node");
    }
}

} // namespace

// ---- the original's sounds, decoded -------------------------------------------

// One of the original's own mp3s, through the engine's decoder.
//
// Here rather than in test_audio because the file is the ORIGINAL's: this suite
// already knows where its assets are and already skips when they are absent,
// and test_audio links the engine alone and has no path to them.
//
// Asserts the SHAPE rather than particular numbers - channels, a rate, 16-bit,
// a duration above zero - because the engine's contract is what matters here
// and a sample rate copied out of a file I had not measured would be a pin on
// nothing. What it really proves is that the decode path runs at all: both
// toolchains build it, and no suite in the tree had ever decoded a byte.
void TheOriginalsMp3sDecode() {
    const std::string path = kOriginal + "/soundfx/door_open.mp3";
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec)) {
        std::printf("test_mp_sprites: the mp3 decode SKIPPED - no %s.\n", path.c_str());
        return;
    }

    Supersonic::AudioClip clip;
    std::string error;
    const bool ok = Supersonic::AudioClip::Load(path, clip, error);

#if defined(_WIN32)
    CHECK_MSG(ok, "door_open.mp3 did not decode: " + error);
    if (!ok) return;
    CHECK(clip.valid());
    CHECK_MSG(clip.channels >= 1 && clip.channels <= 2,
              "channels " + std::to_string(clip.channels));
    CHECK(clip.sampleRate > 0);
    CHECK_EQ(static_cast<int>(clip.bitsPerSample), 16);
    CHECK_MSG(clip.durationSeconds() > 0.0f,
              "duration " + std::to_string(clip.durationSeconds()));
#else
    // Media Foundation is Windows only, so elsewhere this must fail with a
    // reason rather than half-decode or crash.
    CHECK_MSG(!ok, "mp3 must not decode without Media Foundation");
    CHECK(!error.empty());
    CHECK(!clip.valid());
#endif

    // And a sound the engine does not read is refused by name, on every
    // platform, rather than being guessed at.
    Supersonic::AudioClip other;
    std::string otherError;
    CHECK(!Supersonic::AudioClip::Load(kOriginal + "/soundfx/door_open.flac", other, otherError));
    CHECK(!otherError.empty());
}

// ---- the particle systems the .ent files carry --------------------------------

// crystal.ent and fire32.ent, read out of the original's own files. Two
// systems chosen because they differ in every way that matters: one sparkle on
// a still 1x1 bitmap that plays its sheet by age, and thirty-two flames on a
// four-frame cut that pick a frame at random and live a fixed number of times.
void TheOriginalsParticlesAreRead() {
    std::vector<Particles::System> systems;
    std::string error;
    CHECK_MSG(Particles::Load(kOriginal + "/entities/crystal.ent", systems, error), error);
    CHECK_EQ(static_cast<int>(systems.size()), 1);
    if (systems.size() == 1) {
        const Particles::System& crystal = systems.front();
        CHECK_MSG(crystal.bitmap == "sparkles.bmp", "its bitmap is " + crystal.bitmap);
        CHECK_EQ(crystal.count, 2);
        CHECK(crystal.additive);            // alphaMode 1, AM_ADD
        CHECK_EQ(crystal.animationMode, 1); // PLAY_ANIMATION
        CHECK_EQ(crystal.repeat, 0);
        CHECK(!crystal.allAtOnce);
        CHECK_NEAR(static_cast<float>(crystal.lifeTimeMs), 1700.0f);
        CHECK_NEAR(static_cast<float>(crystal.randomLifeTimeMs), 1050.0f);
        CHECK_NEAR(static_cast<float>(crystal.size), 12.0f);
        CHECK_NEAR(static_cast<float>(crystal.randomizeSize), 5.0f);
        CHECK_NEAR(static_cast<float>(crystal.growth), -0.05f);
        CHECK_NEAR(static_cast<float>(crystal.maxSize), 1000.0f);
        CHECK_NEAR(static_cast<float>(crystal.angleDir), 2.0f);
        CHECK_NEAR(static_cast<float>(crystal.randAngle), 2.0f);
        CHECK_NEAR(static_cast<float>(crystal.randAngleStart), 360.0f);
        CHECK_NEAR(static_cast<float>(crystal.startPoint.y), -2.5f);
        CHECK_NEAR(static_cast<float>(crystal.randStartPoint.x), 12.0f);
        CHECK_NEAR(static_cast<float>(crystal.randStartPoint.y), 7.0f);
        CHECK_NEAR(static_cast<float>(crystal.colour0.b), 1.0f);
        CHECK_NEAR(static_cast<float>(crystal.colour1.a), 0.0f);
        CHECK_EQ(crystal.Frames(), 1);
    }

    CHECK_MSG(Particles::Load(kOriginal + "/entities/fire32.ent", systems, error), error);
    CHECK_EQ(static_cast<int>(systems.size()), 1);
    if (systems.size() == 1) {
        const Particles::System& fire = systems.front();
        CHECK_MSG(fire.bitmap == "fire.png", "its bitmap is " + fire.bitmap);
        CHECK_EQ(fire.count, 32);
        CHECK_EQ(fire.animationMode, 2); // PICK_RANDOM_FRAME
        CHECK_EQ(fire.repeat, 2);
        CHECK_EQ(fire.columns, 4);
        CHECK_EQ(fire.rows, 1);
        CHECK_EQ(fire.Frames(), 4);
        CHECK_NEAR(static_cast<float>(fire.lifeTimeMs), 450.0f);
        CHECK_NEAR(static_cast<float>(fire.size), 64.0f);
        CHECK_NEAR(static_cast<float>(fire.growth), -2.5f);
        CHECK_NEAR(static_cast<float>(fire.direction.y), -2.3f);
        CHECK_NEAR(static_cast<float>(fire.randomizeDir.x), 0.7f);
        CHECK_NEAR(static_cast<float>(fire.randAngle), 11.8f);
    }

    // An entity may carry more than one, and a chapter-1 emitter does:
    // portal_static holds two, so a reader that took only the first would draw
    // half of every static portal and say nothing about the rest.
    CHECK_MSG(Particles::Load(kOriginal + "/entities/portal_static.ent", systems, error), error);
    CHECK_EQ(static_cast<int>(systems.size()), 2);
    for (const Particles::System& system : systems) {
        CHECK(!system.bitmap.empty());
        CHECK(system.count > 0);
    }
}

// Every .ent of the original's reads, 102 of them carry a system, and every
// bitmap one names is really there.
//
// The last of those is the check worth having: the particle bitmaps live in
// the original's `particles/` directory, a THIRD place beside its entities and
// its sprites, and looking in the wrong one is a mistake this port has already
// made twice with art that then went missing in silence.
void AnEntityWithoutParticlesSaysSoWithoutFailing() {
    int files = 0;
    int withSystem = 0; // files carrying at least one
    int systemsRead = 0;
    int missingBitmaps = 0;
    std::string firstError;
    std::string firstMissing;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(kOriginal + "/entities", ec)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".ent") continue;
        ++files;
        std::vector<Particles::System> systems;
        std::string error;
        if (!Particles::Load(entry.path().string(), systems, error)) {
            if (firstError.empty()) firstError = error;
            continue;
        }
        if (systems.empty()) continue; // 109 of them carry none, which is no error
        ++withSystem;
        systemsRead += static_cast<int>(systems.size());
        for (const Particles::System& system : systems) {
            const std::string bitmap = kOriginal + "/particles/" + system.bitmap;
            if (!std::filesystem::is_regular_file(bitmap, ec)) {
                ++missingBitmaps;
                if (firstMissing.empty()) firstMissing = system.bitmap;
            }
        }
    }
    CHECK_MSG(firstError.empty(), "an .ent would not read: " + firstError);
    CHECK_EQ(files, 190);
    // 81 files carry systems and there are 102 of them: 21 entities hold two,
    // which is why the reader returns all of an entity's rather than its first.
    CHECK_EQ(withSystem, 81);
    CHECK_EQ(systemsRead, 102);
    CHECK_MSG(missingBitmaps == 0,
              "the original's particles/ is missing " + std::to_string(missingBitmaps) + " bitmap(s), first " +
                  firstMissing);

    // And a file that is not the original's is refused rather than mangled.
    const std::filesystem::path scratch = Scratch() / "not-utf16.ent";
    Write(scratch, {'<', 'E', 't', 'h', 'a', 'n', 'o', 'n', '>'});
    std::vector<Particles::System> systems;
    std::string error;
    CHECK(!Particles::Load(scratch.string(), systems, error));
    CHECK(!error.empty());
    CHECK(systems.empty());
    CHECK(!Particles::Load((Scratch() / "no-such-file.ent").string(), systems, error));
}

// ---- a particle, moved and drawn as ETHParticleManager moves and draws it --------
//
// Synthetic systems and a generator that always answers the middle of its range,
// so every spread is zero and every number below is the arithmetic alone.

const Particles::Random kMiddle = [](double from, double to) { return (from + to) * 0.5; };

bool NearD(double a, double b, double eps = 1e-9) {
    return std::fabs(a - b) <= eps;
}

bool NearP(const glm::dvec2& a, const glm::dvec2& b, double eps = 1e-9) {
    return NearD(a.x, b.x, eps) && NearD(a.y, b.y, eps);
}

std::string ShowP(const glm::dvec2& p) {
    return "(" + std::to_string(p.x) + ", " + std::to_string(p.y) + ")";
}

// light.ent's flame, as the file states it (the reader pins the real one above).
Particles::System TorchFlame() {
    Particles::System s;
    s.bitmap = "fire.png";
    s.count = 12;
    s.alphaMode = Particles::kAlphaAdd;
    s.additive = true;
    s.animationMode = 2;
    s.lifeTimeMs = 450.0;
    s.size = 28.0;
    s.growth = -1.4;
    s.maxSize = 1000.0;
    s.direction = glm::dvec2(0.0, -1.6);
    s.startPoint = glm::dvec2(0.0, -12.0);
    s.colour0 = glm::dvec4(1.0, 0.4, 0.2, 1.0);
    s.colour1 = s.colour0;
    s.luminance = glm::dvec3(0.0);
    s.columns = 4;
    return s;
}

// 2a: a particle starts at its entity's POSITION plus its start point, turned by
// the entity's angle - not at the centre of the entity's picture. The torch of
// 1-1 stands at (288, 64) with its picture hung 16 below (offset (0, 16)), so its
// flame starts at (288, 52), where the port used to start it at (288, 68).
void AParticleStartsAtItsEntitysPosition() {
    const Particles::System flame = TorchFlame();
    Particles::Particle p;
    Particles::Reset(flame, p, Particles::Owner{glm::dvec2(288.0, 64.0), 0.0}, kMiddle);
    CHECK_MSG(NearP(p.atPx, glm::dvec2(288.0, 52.0)) && NearP(p.bornPx, p.atPx),
              "the flame starts 12 above the node: " + ShowP(p.atPx));
    CHECK_MSG(NearP(p.velocityPx, glm::dvec2(0.0, -1.6)), "and rises: " + ShowP(p.velocityPx));

    // The turn is Multiply(v, RotateZ(a)): (x cos + y sin, -x sin + y cos) in
    // +y-down pixels. At 90 degrees right becomes up and up becomes left, which is
    // counter-clockwise on the screen.
    CHECK_MSG(NearP(Particles::Turn(glm::dvec2(10.0, 0.0), 90.0), glm::dvec2(0.0, -10.0), 1e-12),
              "right turned 90 is up: " + ShowP(Particles::Turn(glm::dvec2(10.0, 0.0), 90.0)));
    CHECK_MSG(NearP(Particles::Turn(glm::dvec2(0.0, -10.0), 90.0), glm::dvec2(-10.0, 0.0), 1e-12),
              "up turned 90 is left");
    CHECK_MSG(Particles::Turn(glm::dvec2(3.25, -12.0), 0.0) == glm::dvec2(3.25, -12.0), "unturned is exact");

    // An entity at 90 (the one light_wall of the 128 levels that is turned): the
    // start point, its spread and the direction turn with it; gravity does not.
    Particles::System turned = flame;
    turned.startPoint = glm::dvec2(12.0, 0.0);
    turned.gravity = glm::dvec2(0.0, 0.5);
    turned.angleStart = 10.0;
    Particles::Particle q;
    Particles::Reset(turned, q, Particles::Owner{glm::dvec2(100.0, 100.0), 90.0}, kMiddle);
    CHECK_MSG(NearP(q.atPx, glm::dvec2(100.0, 88.0), 1e-12), "(12, 0) turned 90 is 12 above: " + ShowP(q.atPx));
    CHECK_MSG(NearP(q.velocityPx, glm::dvec2(-1.6, 0.0), 1e-12), "rising turned 90 is leftward: " + ShowP(q.velocityPx));
    CHECK_MSG(NearD(q.angleDeg, 100.0), "its angle starts at angleStart plus the entity's: " + std::to_string(q.angleDeg));
    const bool active = Particles::Step(turned, q, 0, 1, Particles::Owner{glm::dvec2(100.0, 100.0), 90.0}, 1000.0 / 60.0,
                                        false, kMiddle);
    CHECK_MSG(!active, "a particle not yet released is not active");
    CHECK_MSG(q.released, "released on its first frame, index 0 of 1");
    CHECK_MSG(NearP(q.velocityPx, glm::dvec2(-1.6, 0.5), 1e-12), "gravity is added unturned: " + ShowP(q.velocityPx));
}

// 2b: the angle turns counter-clockwise on the screen when it grows, and the
// engine's +z turn is counter-clockwise too, so the rotation the quad takes is
// +angle. The port used to hand it over negated, and 1-1's portals spun the
// wrong way (plan_port gap H: the original at -95 to -119 degrees a second,
// clockwise positive).
void AParticleTurnsCounterClockwise() {
    Particles::System ring;
    ring.count = 3;
    ring.lifeTimeMs = 900.0;
    ring.size = 110.0;
    ring.growth = -2.0;
    ring.minSize = 2.0;
    ring.maxSize = 9100.0;
    ring.angleDir = 1.8; // portal_static.ent's rings
    ring.colour0 = glm::dvec4(0.0, 0.0, 0.0, 1.0);
    ring.colour1 = glm::dvec4(0.5, 0.4, 1.0, 0.0);
    Particles::Particle p;
    const Particles::Owner owner{glm::dvec2(74.0, 166.0), 0.0};
    Particles::Reset(ring, p, owner, kMiddle);
    Particles::Step(ring, p, 0, 3, owner, 1000.0 / 60.0, false, kMiddle); // released; turns 1.8
    const float first = Particles::WorldRotation(p);
    for (int frame = 0; frame < 30; ++frame) Particles::Step(ring, p, 0, 3, owner, 1000.0 / 60.0, false, kMiddle);
    const float later = Particles::WorldRotation(p);
    const double perSecond = static_cast<double>(later - first) / 0.5 * 180.0 / 3.14159265358979323846;
    CHECK_MSG(NearD(perSecond, 108.0, 1e-3), "108 degrees a second, counter-clockwise: " + std::to_string(perSecond));
    CHECK_MSG(later > first, "the engine's rotation grows: counter-clockwise on the screen");
    // The quad's own axis: +x turned by the rotation points up the screen (+y in
    // the engine) for a quarter turn.
    Particles::Particle quarter;
    quarter.angleDeg = 90.0;
    CHECK_MSG(NearD(Particles::WorldRotation(quarter), 3.14159265358979323846 / 2.0, 1e-6),
              "a quarter turn is +pi/2 about +z: " + std::to_string(Particles::WorldRotation(quarter)));
}

// 2c: an added particle is drawn with alpha 1 and its colour lerped; an
// alpha-blended one keeps its alpha and takes min(1, luminance + ambient).
void AnAddedParticleIgnoresItsAlpha() {
    Particles::System sparkle; // crystal.ent's
    sparkle.count = 2;
    sparkle.lifeTimeMs = 1000.0;
    sparkle.size = 12.0;
    sparkle.maxSize = 1000.0;
    sparkle.colour0 = glm::dvec4(0.8, 0.8, 1.0, 1.0);
    sparkle.colour1 = glm::dvec4(0.0, 0.0, 0.0, 0.0);
    Particles::Particle p;
    p.released = true;
    p.size = 12.0;
    p.colour = glm::dvec4(0.4, 0.4, 0.5, 0.5);
    glm::dvec4 c = Particles::DrawColour(sparkle, p, glm::dvec3(0.01));
    CHECK_MSG(NearD(c.r, 0.4) && NearD(c.g, 0.4) && NearD(c.b, 0.5) && c.a == 1.0,
              "added: the lerped colour, alpha 1, no ambient: " + std::to_string(c.a));
    CHECK_MSG(Particles::Drawn(sparkle, p, false), "and drawn");

    Particles::System water = sparkle; // gutter_mouth.ent's
    water.alphaMode = Particles::kAlphaPixel;
    water.additive = false;
    water.luminance = glm::dvec3(0.45);
    water.colour0 = glm::dvec4(1.0);
    c = Particles::DrawColour(water, p, glm::dvec3(0.3, 0.3, 0.35));
    CHECK_MSG(NearD(c.r, 0.4 * 0.75) && NearD(c.b, 0.5 * 0.8) && NearD(c.a, 0.5),
              "mixed: times min(1, 0.45 + ambient), its alpha kept: " + std::to_string(c.r) + " " + std::to_string(c.a));
    c = Particles::DrawColour(water, p, glm::dvec3(0.9));
    CHECK_MSG(NearD(c.r, 0.4) && NearD(c.a, 0.5), "and never lifted past 1");

    // Not drawn once its colour's alpha is spent - in either blend (:375).
    p.colour.a = 0.0;
    CHECK(!Particles::Drawn(water, p, false));
    CHECK(!Particles::Drawn(sparkle, p, false));
    p.colour.a = 0.5;
    p.size = 0.0;
    CHECK_MSG(!Particles::Drawn(sparkle, p, false), "nor at no size");
    p.size = 12.0;
    p.elapsedMs = 1200.0;
    p.lifeMs = 1000.0;
    CHECK_MSG(Particles::Drawn(sparkle, p, false) && !Particles::Drawn(sparkle, p, true),
              "past its life it is drawn only while its system is not killed");
    sparkle.repeat = 1;
    p.repeats = 1;
    CHECK_MSG(!Particles::Drawn(sparkle, p, false), "and never once its lives are spent");

    CHECK_MSG(Particles::Drawable(sparkle) && Particles::Drawable(water), "added and mixed are drawn");
    Particles::System modulate = sparkle;
    modulate.alphaMode = Particles::kAlphaModulate;
    modulate.additive = false;
    CHECK_MSG(!Particles::Drawable(modulate), "a multiply is refused, not mixed");
}

// 2d: a system draws in its entity's own slot, after the halo's quarter and
// before the half the layer puts what stands between two slots at, one after
// another in the file's order.
void ASystemDrawsInsideItsEntitysSlot() {
    CHECK_MSG(NearD(Particles::SlotFraction(0, 1), 0.375), "one system: 3/8");
    CHECK_MSG(NearD(Particles::SlotFraction(0, 2), 1.0 / 3.0) && NearD(Particles::SlotFraction(1, 2), 5.0 / 12.0),
              "two: 1/3 and 5/12");
    for (int n = 1; n <= 8; ++n) {
        double last = 0.25;
        for (int t = 0; t < n; ++t) {
            const double f = Particles::SlotFraction(t, n);
            CHECK_MSG(f > last && f < 0.5, "system " + std::to_string(t) + " of " + std::to_string(n) + " at " +
                                               std::to_string(f) + ", after " + std::to_string(last));
            last = f;
        }
    }
}

// 2e: the quad is square whatever the cell is; tesla_shock_black_bg.png is cut
// 5 x 2 from 256 x 64, a 51.2 x 32 cell, and is still drawn size by size.
void AParticlesQuadIsSquare() {
    Particles::Particle p;
    p.size = 40.0;
    CHECK_MSG(Particles::QuadPx(p) == glm::dvec2(40.0, 40.0), "40 by 40: " + ShowP(Particles::QuadPx(p)));
}

// ETHEntity::Scale on a system (ETHParticleSystem.cpp:27-40): every length times the
// scale, and no time, angle or colour. And the layer's shortcut - making the pool
// from the scaled system, where the original scales a pool it has made and run
// for a frame - gives the same particles for a system whose start point, spreads
// and direction are 0, as portal_static's rings are: sizes and their growth are
// linear in the scale, and the draws are the same draws over scaled ranges.
void AScaledSystemIsEthanonsScale() {
    Particles::System s = TorchFlame();
    s.gravity = glm::dvec2(0.5, -0.25);
    s.randomizeDir = glm::dvec2(2.0, 4.0);
    s.randStartPoint = glm::dvec2(6.0, 8.0);
    s.randomizeSize = 10.0;
    s.minSize = 2.0;
    s.randomLifeTimeMs = 30.0;
    s.angleStart = 15.0;
    s.randAngleStart = 245.0;
    s.angleDir = 1.8;
    s.randAngle = 0.5;
    Particles::System scaled = s;
    Particles::Scale(scaled, 0.8);
    CHECK_MSG(NearP(scaled.gravity, s.gravity * 0.8) && NearP(scaled.direction, s.direction * 0.8) &&
                  NearP(scaled.randomizeDir, s.randomizeDir * 0.8) && NearP(scaled.startPoint, s.startPoint * 0.8) &&
                  NearP(scaled.randStartPoint, s.randStartPoint * 0.8),
              "gravity, direction, their spreads and the start point scale");
    CHECK_MSG(NearD(scaled.size, s.size * 0.8) && NearD(scaled.randomizeSize, s.randomizeSize * 0.8) &&
                  NearD(scaled.growth, s.growth * 0.8) && NearD(scaled.minSize, s.minSize * 0.8) &&
                  NearD(scaled.maxSize, s.maxSize * 0.8),
              "the size, its spread, its growth and its bounds scale");
    CHECK_MSG(scaled.lifeTimeMs == s.lifeTimeMs && scaled.randomLifeTimeMs == s.randomLifeTimeMs &&
                  scaled.angleStart == s.angleStart && scaled.randAngleStart == s.randAngleStart &&
                  scaled.angleDir == s.angleDir && scaled.randAngle == s.randAngle && scaled.colour0 == s.colour0 &&
                  scaled.colour1 == s.colour1 && scaled.luminance == s.luminance && scaled.count == s.count &&
                  scaled.columns == s.columns && scaled.rows == s.rows,
              "no time, angle, colour, count or cut does");

    // portal_static.ent's rings, and the same rings as its script leaves them.
    Particles::System ring;
    ring.count = 3;
    ring.lifeTimeMs = 900.0;
    ring.randomLifeTimeMs = 300.0;
    ring.size = 110.0;
    ring.growth = -2.0;
    ring.minSize = 2.0;
    ring.maxSize = 9100.0;
    ring.angleDir = 1.8;
    ring.randAngleStart = 245.0;
    ring.colour0 = glm::dvec4(0.0, 0.0, 0.0, 1.0);
    ring.colour1 = glm::dvec4(0.5, 0.4, 1.0, 0.0);
    Particles::System scaledRing = ring;
    Particles::Scale(scaledRing, 0.8);
    CHECK_MSG(scaledRing.size == 88.0 && NearD(scaledRing.growth, -1.6) && NearD(scaledRing.minSize, 1.6),
              "the rings are born at 88 and shrink 1.6 a frame to 1.6");

    // A generator whose k-th draw lies (k mod 16) / 16 along its range: one each, counted.
    const auto sequence = [](int& k) {
        return [&k](double from, double to) {
            ++k;
            return from + (to - from) * static_cast<double>(k % 16) / 16.0;
        };
    };
    int drawsA = 0;
    int drawsB = 0;
    const Particles::Random a = sequence(drawsA);
    const Particles::Random b = sequence(drawsB);
    const Particles::Owner owner{glm::dvec2(334.0, 200.0), 0.0};
    const double tick = 1000.0 / 60.0;
    // The original's order: the pool made and run a frame at the file's scale, then
    // scaled, particle and system (ETHParticleManager.cpp:444-451, .h:160-164).
    std::vector<Particles::Particle> original = Particles::MakePool(ring, 3, owner, a);
    for (int i = 0; i < 3; ++i) Particles::Step(ring, original[static_cast<std::size_t>(i)], i, 3, owner, tick, false, a);
    for (Particles::Particle& p : original) {
        p.size *= 0.8;
        p.velocityPx *= 0.8;
    }
    // The layer's: made from the scaled system, run the same frame.
    std::vector<Particles::Particle> port = Particles::MakePool(scaledRing, 3, owner, b);
    for (int i = 0; i < 3; ++i) Particles::Step(scaledRing, port[static_cast<std::size_t>(i)], i, 3, owner, tick, false, b);
    bool same = drawsA == drawsB;
    for (int frame = 0; frame < 240 && same; ++frame) {
        for (int i = 0; i < 3; ++i) {
            const Particles::Particle& o = original[static_cast<std::size_t>(i)];
            const Particles::Particle& q = port[static_cast<std::size_t>(i)];
            if (!NearD(o.size, q.size, 1e-9) || !NearP(o.atPx, q.atPx, 1e-9) || !NearD(o.angleDeg, q.angleDeg, 1e-9) ||
                o.released != q.released || o.repeats != q.repeats || o.colour != q.colour) {
                same = false;
            }
        }
        for (int i = 0; i < 3; ++i) {
            Particles::Step(scaledRing, original[static_cast<std::size_t>(i)], i, 3, owner, tick, false, a);
            Particles::Step(scaledRing, port[static_cast<std::size_t>(i)], i, 3, owner, tick, false, b);
        }
        same = same && drawsA == drawsB;
    }
    CHECK_MSG(same, "four seconds of the rings: scaling the made pool and making the pool scaled agree, draw for draw");
}

// And the loop's own order, which the port once had wrong: a renewed particle is
// drawn in its colour at birth and at frame 0 of a played sheet on the frame it is
// renewed; it counts as active by what the frame before left; the motion is capped
// at 250 ms and the age is not; a killed system still releases, and renews nothing.
void TheLoopKeepsEthanonsOrder() {
    Particles::System s;
    s.count = 4;
    s.lifeTimeMs = 110.0;
    s.size = 10.0;
    s.maxSize = 100.0;
    s.direction = glm::dvec2(1.0, 0.0);
    s.colour0 = glm::dvec4(1.0, 0.0, 0.0, 1.0);
    s.colour1 = glm::dvec4(0.0, 0.0, 1.0, 1.0);
    s.columns = 4;
    s.animationMode = 1;
    const Particles::Owner owner{glm::dvec2(0.0), 0.0};
    const double tick = 1000.0 / 60.0;

    std::vector<Particles::Particle> pool = Particles::MakePool(s, 4, owner, kMiddle);
    CHECK_EQ(static_cast<int>(pool.size()), 4);
    // Staggered: index i is released once its age passes (110 + 0) * i / 4.
    int releasedAfterOne = 0;
    for (int i = 0; i < 4; ++i) {
        Particles::Step(s, pool[static_cast<std::size_t>(i)], i, 4, owner, tick, false, kMiddle);
        if (pool[static_cast<std::size_t>(i)].released) ++releasedAfterOne;
    }
    CHECK_MSG(releasedAfterOne == 1, "one frame of 16.7 ms releases index 0 only: " + std::to_string(releasedAfterOne));

    Particles::Particle p = pool[0];
    CHECK_MSG(NearD(p.elapsedMs, 0.0) && NearP(p.atPx, glm::dvec2(1.0, 0.0)), "released at 0 and moved one step");
    // Five more frames: 83.3 ms of a life of 110, at 3/4 of the sheet.
    for (int f = 0; f < 5; ++f) CHECK(Particles::Step(s, p, 0, 4, owner, tick, false, kMiddle));
    CHECK_MSG(p.frame == 3 && NearD(p.colour.r, 1.0 - 83.3333333333 / 110.0, 1e-6), "frame 3, colour lerped by age");
    // The sixth: 100 ms is inside its life; the seventh, 116.7, renews it.
    CHECK(Particles::Step(s, p, 0, 4, owner, tick, false, kMiddle));
    CHECK_MSG(p.repeats == 0, "inside its life it is not renewed");
    CHECK(Particles::Step(s, p, 0, 4, owner, tick, false, kMiddle));
    CHECK_MSG(p.repeats == 1 && p.elapsedMs == 0.0 && p.frame == 0 && p.colour == s.colour0 &&
                  NearP(p.atPx, glm::dvec2(0.0)),
              "renewed: colour at birth, frame 0, back at its start: frame " + std::to_string(p.frame) + " at " +
                  ShowP(p.atPx));

    // A frame of a whole second moves 250 ms' worth and ages the whole second.
    Particles::Particle slow = pool[1];
    slow.released = true;
    slow.elapsedMs = 0.0;
    slow.lifeMs = 5000.0;
    slow.atPx = glm::dvec2(0.0);
    Particles::Step(s, slow, 1, 4, owner, 1000.0, false, kMiddle);
    CHECK_MSG(NearP(slow.atPx, glm::dvec2(15.0, 0.0)) && NearD(slow.elapsedMs, 1000.0),
              "capped motion, whole age: " + ShowP(slow.atPx));

    // Size 0 as the last frame left it is not active, whatever this frame does.
    Particles::Particle shrunk = slow;
    shrunk.size = 0.0;
    CHECK_MSG(!Particles::Step(s, shrunk, 1, 4, owner, tick, false, kMiddle), "no size, not active");

    // Killed: an unreleased particle is still released; a life that ends is not renewed.
    Particles::Particle waiting = pool[3];
    Particles::Step(s, waiting, 3, 4, owner, 200.0, true, kMiddle);
    CHECK_MSG(waiting.released, "a killed system still releases");
    Particles::Particle ending = slow;
    ending.elapsedMs = 4990.0;
    const bool counted = Particles::Step(s, ending, 1, 4, owner, tick, true, kMiddle);
    CHECK_MSG(counted && ending.repeats == 1 && ending.elapsedMs > ending.lifeMs && !Particles::Drawn(s, ending, true),
              "killed at the end of its life: counted, spent, not renewed, not drawn");
}

// The random numbers are drawn in the original's statement order: ResetParticle's
// turn rate, life, size, direction x then y (ETHParticleManager.cpp:483-488), then
// PositionParticle's angle, start x then y (:517-519), then the frame (:502). kMiddle
// answers every draw alike, so it cannot see the order; this generator answers the
// k-th draw (k + 1) / 16 of the way along its range, so a y drawn before its x
// would put different numbers on both.
void TheRandomNumbersAreDrawnInEthanonsOrder() {
    Particles::System s;
    s.count = 1;
    s.lifeTimeMs = 500.0;
    s.size = 20.0;
    s.maxSize = 100.0;
    s.direction = glm::dvec2(1.0, -1.0);
    s.startPoint = glm::dvec2(10.0, 20.0);
    s.randAngle = 2.0;
    s.randomLifeTimeMs = 4.0;
    s.randomizeSize = 6.0;
    s.randomizeDir = glm::dvec2(8.0, 10.0);
    s.randAngleStart = 12.0;
    s.randStartPoint = glm::dvec2(14.0, 16.0);
    s.columns = 2;
    s.animationMode = 2; // PICK_RANDOM_FRAME: one more draw, last
    const Particles::Owner owner{glm::dvec2(100.0, 200.0), 0.0};

    std::vector<std::pair<double, double>> asked;
    const Particles::Random sequence = [&asked](double from, double to) {
        const double along = static_cast<double>(asked.size() + 1) / 16.0;
        asked.emplace_back(from, to);
        return from + (to - from) * along;
    };

    Particles::Particle p;
    Particles::Reset(s, p, owner, sequence);
    const std::vector<std::pair<double, double>> order{{-1.0, 1.0}, {-2.0, 2.0}, {-3.0, 3.0}, {-4.0, 4.0}, {-5.0, 5.0},
                                                       {0.0, 12.0}, {-7.0, 7.0}, {-8.0, 8.0}, {0.0, 2.0}};
    CHECK_MSG(asked == order, "Reset asks for its nine ranges in the original's order, " +
                                  std::to_string(asked.size()) + " asked");
    CHECK_MSG(NearD(p.angleDirDeg, -0.875) && NearD(p.lifeMs, 498.5) && NearD(p.size, 18.125),
              "turn rate, life and size take draws 1 to 3");
    CHECK_MSG(NearP(p.velocityPx, glm::dvec2(-1.0, -2.875)),
              "direction x takes draw 4 and y draw 5: " + ShowP(p.velocityPx));
    CHECK_MSG(NearD(p.angleDeg, 4.5), "the start angle takes draw 6: " + std::to_string(p.angleDeg));
    CHECK_MSG(NearP(p.atPx, glm::dvec2(109.125, 220.0)), "start x takes draw 7 and y draw 8: " + ShowP(p.atPx));
    CHECK_MSG(p.frame == 1, "and the frame draw 9: " + std::to_string(p.frame));

    // A first release positions only: the angle, then x, then y.
    asked.clear();
    Particles::Release(s, p, owner, sequence);
    const std::vector<std::pair<double, double>> released{{0.0, 12.0}, {-7.0, 7.0}, {-8.0, 8.0}};
    CHECK_MSG(asked == released, "Release asks for three ranges, angle then x then y");
    CHECK_MSG(NearD(p.angleDeg, 0.75), "the angle takes the first draw: " + std::to_string(p.angleDeg));
    CHECK_MSG(NearP(p.atPx, glm::dvec2(104.75, 215.0)), "x takes the second draw and y the third: " + ShowP(p.atPx));
}

// The census the blend rules rest on, over all 190 files: 102 systems, 75 added,
// 23 mixed and 4 multiplied; every added bitmap without alpha, which is what makes
// an added particle's alpha of 1 exact rather than close; every mixed bitmap with
// it; and every system with a <Luminance>.
int PngColourType(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    std::vector<unsigned char> head(26, 0);
    if (!file.read(reinterpret_cast<char*>(head.data()), 26)) return -1;
    if (head[0] != 0x89 || head[1] != 'P') return -1;
    return head[25];
}

void EveryAddedBitmapIsWithoutAlpha() {
    int systemsRead = 0;
    int added = 0;
    int mixed = 0;
    int multiplied = 0;
    int addedWithAlpha = 0;
    int mixedWithoutAlpha = 0;
    std::string firstWrong;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(kOriginal + "/entities", ec)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".ent") continue;
        std::vector<Particles::System> systems;
        std::string error;
        if (!Particles::Load(entry.path().string(), systems, error)) continue;
        for (const Particles::System& system : systems) {
            ++systemsRead;
            const std::string bitmap = kOriginal + "/particles/" + system.bitmap;
            const std::string ext = std::filesystem::path(system.bitmap).extension().string();
            // A PNG says in its IHDR (colour types 4 and 6 carry alpha, and none of
            // the game's is paletted); the game's BMPs are 8-bit grey and its JPG
            // cannot carry any.
            int type = PngColourType(bitmap);
            const bool alpha = type == 4 || type == 6;
            if (type < 0 && ext != ".bmp" && ext != ".JPG" && ext != ".jpg") {
                if (firstWrong.empty()) firstWrong = system.bitmap + " is none of PNG, BMP or JPG";
            }
            if (system.alphaMode == Particles::kAlphaAdd) {
                ++added;
                if (alpha) {
                    ++addedWithAlpha;
                    if (firstWrong.empty()) firstWrong = system.bitmap + " is added and carries alpha";
                }
            } else if (system.alphaMode == Particles::kAlphaPixel) {
                ++mixed;
                if (!alpha) {
                    ++mixedWithoutAlpha;
                    if (firstWrong.empty()) firstWrong = system.bitmap + " is mixed and carries none";
                }
            } else if (system.alphaMode == Particles::kAlphaModulate) {
                ++multiplied;
            }
        }
    }
    CHECK_EQ(systemsRead, 102);
    CHECK_EQ(added, 75);
    CHECK_EQ(mixed, 23);
    CHECK_EQ(multiplied, 4);
    CHECK_MSG(addedWithAlpha == 0 && mixedWithoutAlpha == 0, firstWrong);

    // And the three the layer draws differently, read from the files.
    std::vector<Particles::System> systems;
    std::string error;
    CHECK_MSG(Particles::Load(kOriginal + "/entities/portal_static.ent", systems, error) && systems.size() == 2, error);
    if (systems.size() == 2) {
        CHECK(systems[0].alphaMode == Particles::kAlphaAdd && systems[0].luminance == glm::dvec3(1.0));
        CHECK_MSG(systems[1].alphaMode == Particles::kAlphaPixel && !systems[1].additive && systems[1].bitmap == "portal.png",
                  "portal_static's iris is mixed");
    }
    CHECK_MSG(Particles::Load(kOriginal + "/entities/gutter_mouth.ent", systems, error) && systems.size() == 1, error);
    if (systems.size() == 1) {
        CHECK_MSG(systems[0].alphaMode == Particles::kAlphaPixel &&
                      NearP(glm::dvec2(systems[0].luminance.r, systems[0].luminance.b), glm::dvec2(0.45), 1e-9),
                  "gutter_mouth's water is mixed at luminance 0.45");
    }
    CHECK_MSG(Particles::Load(kOriginal + "/entities/light.ent", systems, error) && systems.size() == 1, error);
    if (systems.size() == 1) {
        CHECK(systems[0].luminance == glm::dvec3(0.0));
        CHECK_MSG(NearP(systems[0].startPoint, glm::dvec2(0.0, -12.0)) && systems[0].randStartPoint == glm::dvec2(0.0),
                  "the torch's flame starts 12 above its entity, with no spread");
    }
}

// ---- a pool that is over, and a system made at its entity's scale ----------------

// ETHParticleManager's m_finished: every particle past its last life. A system of
// repeat 1 and two particles: over only once BOTH have lived once, and from then on
// nothing of it is drawn. A system of repeat 0 is never over, killed or not.
void APoolIsOverWhenEveryParticleIsSpent() {
    Particles::System s;
    s.count = 2;
    s.repeat = 1;
    s.lifeTimeMs = 40.0;
    s.size = 10.0;
    s.maxSize = 100.0;
    s.colour0 = glm::dvec4(1.0);
    s.colour1 = glm::dvec4(1.0);
    const Particles::Owner owner{glm::dvec2(0.0), 0.0};
    const double tick = 1000.0 / 60.0;
    std::vector<Particles::Particle> pool = Particles::MakePool(s, 2, owner, kMiddle);
    CHECK_MSG(!Particles::Finished(s, pool), "a new pool is not over");
    int frames = 0;
    bool firstSpentAlone = false;
    while (!Particles::Finished(s, pool) && frames < 60) {
        for (std::size_t i = 0; i < pool.size(); ++i) {
            Particles::Step(s, pool[i], static_cast<int>(i), 2, owner, tick, false, kMiddle);
        }
        ++frames;
        if (pool[0].repeats >= 1 && pool[1].repeats < 1) firstSpentAlone = true;
    }
    CHECK_MSG(firstSpentAlone, "the first particle is spent while the second still lives");
    // Index 0 is released on the first frame and spent on the fourth, its age 50
    // past a life of 40; index 1 is released on the second (20 ms in) and spent on
    // the fifth - and the pool is over from then.
    CHECK_MSG(frames == 5, "over after 5 frames: " + std::to_string(frames));
    for (const Particles::Particle& p : pool) CHECK_MSG(!Particles::Drawn(s, p, false), "nothing of it is drawn");

    // Repeat 0 is forever: its particles are renewed, and a killed one lives on
    // past its life without ever counting as spent.
    Particles::System forever = s;
    forever.repeat = 0;
    std::vector<Particles::Particle> lasting = Particles::MakePool(forever, 2, owner, kMiddle);
    for (int f = 0; f < 120; ++f) {
        for (std::size_t i = 0; i < lasting.size(); ++i) {
            Particles::Step(forever, lasting[i], static_cast<int>(i), 2, owner, tick, f >= 30, kMiddle);
        }
    }
    CHECK_MSG(!Particles::Finished(forever, lasting), "repeat 0 never ends, killed or not");
    CHECK_MSG(Particles::Finished(s, {}), "an empty pool is over, as a loop over nothing draws nothing");
}

// Scaled is Scale on a copy, and the start point's z is a length like its x and y
// (ETHParticleSystem.cpp:33 scales the vector3).
void AScaledCopyLeavesItsSystemAsItWas() {
    Particles::System s = TorchFlame();
    s.startPointZ = 10.0;
    const Particles::System copy = Particles::Scaled(s, 1.5);
    Particles::System inPlace = s;
    Particles::Scale(inPlace, 1.5);
    CHECK_MSG(copy.size == inPlace.size && copy.growth == inPlace.growth && copy.startPoint == inPlace.startPoint &&
                  copy.direction == inPlace.direction && copy.maxSize == inPlace.maxSize,
              "the same numbers as Scale");
    CHECK_MSG(copy.startPointZ == 15.0 && inPlace.startPointZ == 15.0, "the start point's z scales");
    CHECK_MSG(s.size == 28.0 && s.startPointZ == 10.0, "and the system it was made from is untouched");
}

// ---- an entity the script adds, read out of its .ent (Art::LoadEnt) --------------

// The .ent files are UTF-16 LE with a byte-order mark; this writes ASCII as one.
std::vector<unsigned char> Utf16(const std::string& text) {
    std::vector<unsigned char> bytes = {0xFF, 0xFE};
    for (const char c : text) {
        bytes.push_back(static_cast<unsigned char>(c));
        bytes.push_back(0);
    }
    return bytes;
}

std::string EntFile(const std::string& entityAttributes, const std::string& children) {
    return "<?xml version=\"1.0\" ?>\n<Ethanon>\n    <Entity" + entityAttributes + ">\n" + children +
           "    </Entity>\n</Ethanon>\n";
}

// One whole particle system, every field stated, as all 102 of the original's are.
std::string OneSystem(int repeat) {
    return "<ParticleSystem particles=\"2\" allAtOnce=\"0\" alphaMode=\"1\" repeat=\"" + std::to_string(repeat) +
           "\" animationMode=\"1\" boundingSphere=\"8\" lifeTime=\"100\" randomLifeTime=\"0\" angleDir=\"0\" "
           "randAngle=\"0\" size=\"8\" randomizeSize=\"0\" growth=\"0\" minSize=\"0\" maxSize=\"100\" "
           "angleStart=\"0\" randAngleStart=\"0\"><Bitmap>spark.png</Bitmap><Gravity x=\"0\" y=\"0\" />"
           "<Direction x=\"0\" y=\"0\" /><RandomizeDir x=\"0\" y=\"0\" /><SpriteCut x=\"3\" y=\"2\" />"
           "<StartPoint x=\"0\" y=\"0\" z=\"6\" /><RandStartPoint x=\"0\" y=\"0\" />"
           "<Color0 r=\"1\" g=\"1\" b=\"1\" a=\"1\" /><Color1 r=\"1\" g=\"1\" b=\"1\" a=\"1\" />"
           "<Luminance r=\"1\" g=\"1\" b=\"1\" /></ParticleSystem>";
}

bool ReadEnt(const std::string& name, const std::string& text, Art::Entity& out, std::string& error) {
    const std::filesystem::path path = Scratch() / name;
    Write(path, Utf16(text));
    return Art::LoadEnt(path.string(), out, error);
}

// What the file does not state is the ENGINE's default, not a refusal: portal.ent
// states no <Scale>, fireball.ent no <Sprite>. What it states wrongly is refused.
void AnEntReadsAsTheEngineReadsIt() {
    Art::Entity bare;
    std::string error;
    CHECK_MSG(ReadEnt("bare.ent", EntFile("", ""), bare, error), error);
    CHECK_MSG(bare.sprite.empty() && bare.normal.empty() && bare.systems.empty() && !bare.light.has_value(),
              "nothing stated: no picture, no systems, no light");
    CHECK_MSG(bare.scale == glm::dvec2(1.0) && bare.pivotPx == glm::dvec2(0.0) && bare.columns == 1 &&
                  bare.rows == 1 && bare.blendMode == 0 && !bare.additive,
              "Scale (1, 1), PivotAdjust (0, 0), SpriteCut (1, 1), AM_PIXEL (ETHEntityProperties.cpp:79-109)");
    CHECK_MSG(bare.emissive == glm::dvec3(0.0) && !bare.isStatic && !bare.applyLight,
              "EmissiveColor black, not static, applies no light");
    CHECK_MSG(!bare.Temporary(), "no system: never temporary, as IsTemporary's existent count says");

    // A light with nothing stated but a halo: ETHLight(true)'s range 256, halo 64,
    // brightness 1, white, at the entity.
    Art::Entity lit;
    CHECK_MSG(ReadEnt("lit.ent", EntFile("", "<Light><HaloBitmap>halo.bmp</HaloBitmap></Light>"), lit, error), error);
    CHECK_MSG(lit.light.has_value(), "an unstated light is active");
    if (lit.light) {
        CHECK_MSG(lit.light->range == 256.0 && lit.light->haloSize == glm::dvec2(64.0) &&
                      lit.light->haloBrightness == 1.0 && lit.light->colour == glm::dvec3(1.0) &&
                      lit.light->offset == glm::dvec3(0.0) && lit.light->halo == "halo.bmp",
                  "ETHLight's defaults (ETHLight.cpp:27-38)");
    }
    // Kept only active and with a range; a light with no <HaloBitmap> has no halo.
    Art::Entity off;
    CHECK_MSG(ReadEnt("off.ent", EntFile("", "<Light active=\"0\" range=\"90\" />"), off, error), error);
    CHECK_MSG(!off.light.has_value(), "an inactive light is dropped (ETHEntityProperties.cpp:343)");
    CHECK_MSG(ReadEnt("dark.ent", EntFile("", "<Light range=\"0\" />"), off, error), error);
    CHECK_MSG(!off.light.has_value(), "and a light of range 0");
    Art::Entity haloless;
    CHECK_MSG(ReadEnt("haloless.ent",
                      EntFile("", "<Light range=\"78\"><Position x=\"0\" y=\"0\" z=\"14\" /></Light>"), haloless,
                      error),
              error);
    CHECK_MSG(haloless.light.has_value() && haloless.light->halo.empty() && haloless.light->range == 78.0 &&
                  haloless.light->offset.z == 14.0,
              "a light that names no halo bitmap lights and has no halo");

    // Only the entity's OWN children: a <SpriteCut> inside its particle system
    // and a <Position> inside its collision are not its.
    Art::Entity own;
    CHECK_MSG(ReadEnt("own.ent",
                      EntFile(" blendMode=\"1\" static=\"1\" applyLight=\"1\"",
                              "<Particles>" + OneSystem(1) + "</Particles><Collision><Position x=\"5\" y=\"5\" "
                              "z=\"5\" /></Collision><Light range=\"50\"><Position x=\"1\" y=\"2\" z=\"3\" />"
                              "</Light><Scale x=\"1.4\" y=\"1.2\" />"),
                      own, error),
              error);
    CHECK_MSG(own.columns == 1 && own.rows == 1, "the system's cut is not the entity's");
    CHECK_MSG(own.light && own.light->offset == glm::dvec3(1.0, 2.0, 3.0) &&
                  own.light->haloOffset == glm::dvec2(1.0, 2.0),
              "the light's own position, and its halo there unscaled");
    CHECK_MSG(own.additive && own.blendMode == 1 && own.isStatic && own.applyLight, "its attributes");
    CHECK_MSG(own.systems.size() == 1 && own.systems.front().columns == 3 && own.systems.front().startPointZ == 6.0,
              "its system, as Particles reads it");
    CHECK_MSG(own.Temporary(), "no sprite and every system repeats: temporary");
    CHECK_MSG(own.scale == glm::dvec2(1.4, 1.2) && NearP(own.ScaleAt(2.0), glm::dvec2(2.8, 2.4)) &&
                  NearD(own.ParticleScale(2.0), 2.6),
              "<Scale> per axis; AddEntity's x 2 makes it (2.8, 2.4), its systems at the mean 2.6");

    // A sprite, or a system that lives forever, keeps it until its script deletes it.
    Art::Entity pictured;
    CHECK_MSG(ReadEnt("pictured.ent", EntFile("", "<Sprite>x.png</Sprite><Particles>" + OneSystem(1) + "</Particles>"),
                      pictured, error),
              error);
    CHECK_MSG(pictured.sprite == "x.png" && !pictured.Temporary(), "with a sprite: not temporary");
    Art::Entity lasting;
    CHECK_MSG(ReadEnt("lasting.ent", EntFile("", "<Particles>" + OneSystem(1) + OneSystem(0) + "</Particles>"),
                      lasting, error),
              error);
    CHECK_MSG(lasting.systems.size() == 2 && !lasting.Temporary(), "one system of repeat 0: not temporary");

    // Refused, each by what is wrong.
    const auto refused = [&](const char* name, const std::string& text, const char* named) {
        Art::Entity out;
        out.sprite = "untouched";
        std::string why;
        const bool ok = ReadEnt(name, text, out, why);
        CHECK_MSG(!ok && why.find(named) != std::string::npos && out.sprite == "untouched",
                  std::string(name) + " is refused by name, leaving what it was given: " + why);
    };
    refused("scale.ent", EntFile("", "<Scale x=\"big\" y=\"1\" />"), "Scale");
    refused("blend.ent", EntFile(" blendMode=\"add\"", ""), "blendMode");
    refused("cut.ent", EntFile("", "<SpriteCut x=\"0\" y=\"1\" />"), "SpriteCut");
    refused("range.ent", EntFile("", "<Light range=\"far\" />"), "range");
    refused("root.ent", "<?xml version=\"1.0\" ?>\n<Entity></Entity>\n", "<Ethanon><Entity>");
    refused("open.ent", "<Ethanon><Entity><Sprite>x.png</Entity></Ethanon>", "closed by");
    refused("three.ent", EntFile("", "<Particles>" + OneSystem(1) + OneSystem(1) + OneSystem(1) + "</Particles>"),
            "reads two");
    std::string brokenSystem = OneSystem(1);
    brokenSystem.replace(brokenSystem.find(" z=\"6\""), 6, "");
    refused("nozed.ent", EntFile("", "<Particles>" + brokenSystem + "</Particles>"), "StartPoint z");
    {
        const std::filesystem::path path = Scratch() / "ascii.ent";
        Write(path, {'<', 'E', 't', 'h', 'a', 'n', 'o', 'n', '>'});
        Art::Entity out;
        std::string why;
        CHECK_MSG(!Art::LoadEnt(path.string(), out, why) && why.find("UTF-16") != std::string::npos,
                  "a file not in the original's UTF-16 is refused: " + why);
    }
}

// The five the visuals plan names (00_order step 6.2; system_7 7a), and the one it
// scales: each read out of the original's own file.
void TheEffectsAreTheirEnts() {
    const auto load = [](const char* name) {
        Art::Entity entity;
        std::string error;
        CHECK_MSG(Art::LoadEnt(kOriginal + "/entities/" + name, entity, error), error);
        return entity;
    };

    // suck_effect.ent: no sprite, one system that lives once - temporary, freed by
    // the engine when it is over.
    const Art::Entity suck = load("suck_effect.ent");
    CHECK_MSG(suck.sprite.empty() && suck.systems.size() == 1 && suck.Temporary() && !suck.light,
              "suck_effect: no sprite, one system, temporary, no light");
    if (suck.systems.size() == 1) {
        const Particles::System& s = suck.systems.front();
        CHECK_MSG(s.bitmap == "inverted_explosion.bmp" && s.count == 1 && s.repeat == 1 && s.columns == 10 &&
                      s.rows == 1 && s.additive && s.size == 64.0 && s.lifeTimeMs == 500.0,
                  "its burst: inverted_explosion.bmp 10 x 1, one particle, once, 64 across for 500 ms");
    }

    // fireball.ent: no sprite, but its one system repeats 0, so it is NOT
    // temporary - its script deletes it. <Scale> 1.2, and a light whose 60-unit
    // halo is drawn 72 across (system_7 S11).
    const Art::Entity fireball = load("fireball.ent");
    CHECK_MSG(fireball.sprite.empty(), "fireball: no sprite");
    CHECK_MSG(fireball.systems.size() == 1 && fireball.systems.front().repeat == 0, "one system, repeat 0");
    CHECK_MSG(!fireball.Temporary(), "not temporary: ETHCallback_fireball deletes it");
    CHECK_MSG(fireball.scale == glm::dvec2(1.2) && fireball.pivotPx == glm::dvec2(4.0, 0.0) &&
                  fireball.additive && fireball.emissive == glm::dvec3(0.3, 0.3, 0.8),
              "<Scale> 1.2, PivotAdjust (4, 0), blendMode 1, emissive (0.3, 0.3, 0.8)");
    CHECK_MSG(fireball.columns == 1 && fireball.rows == 1, "its system's SpriteCut is not its own");
    CHECK_MSG(fireball.light.has_value(), "fireball has a light");
    if (fireball.light) {
        const Lighting::Light& light = *fireball.light;
        CHECK_MSG(light.range == 50.0 && light.colour == glm::dvec3(1.0, 0.5, 0.2) &&
                      light.offset == glm::dvec3(0.0, 0.0, 1.0) && light.halo == "halo.bmp" &&
                      light.haloSize == glm::dvec2(60.0) && light.haloBrightness == 1.0,
                  "range 50, (1, 0.5, 0.2), 1 above it; halo.bmp, 60 across - its Light's Position, not its "
                  "Collision's");
        const Lighting::Light drawn = Lighting::OwnerScaled(light, fireball.ScaleAt(1.0));
        CHECK_MSG(NearP(drawn.haloSize, glm::dvec2(72.0), 1e-9) && NearD(drawn.range, 60.0),
                  "added at getScale(): its halo 72 across and its light's range 60");
    }
    if (!fireball.systems.empty()) {
        const Particles::System& s = fireball.systems.front();
        CHECK_MSG(s.bitmap == "explosion.JPG" && s.count == 5 && s.startPointZ == 4.0,
                  "explosion.JPG, five, 4 above the entity");
        CHECK_MSG(NearD(fireball.ParticleScale(1.0), 1.2), "its system made at 1.2");
    }

    // red_eyes.ent: the player's eyes. A system that never ends, owned by the
    // player and never freed.
    const Art::Entity eyes = load("red_eyes.ent");
    CHECK_MSG(eyes.sprite.empty() && eyes.systems.size() == 1 && !eyes.Temporary() && !eyes.light,
              "red_eyes: no sprite, one system, not temporary, no light");
    if (eyes.systems.size() == 1) {
        const Particles::System& s = eyes.systems.front();
        CHECK_MSG(s.bitmap == "hard_halo.png" && s.count == 12 && s.repeat == 0 && s.startPointZ == 2.0 &&
                      s.colour0 == glm::dvec4(1.0, 0.1, 0.1, 1.0),
                  "hard_halo.png, twelve, forever, 2 above, red");
    }

    // portal.ent: a sprite and two systems, the second alpha-blended. The file
    // states no <Scale>, <SpriteCut> or <PivotAdjust>: the engine's defaults.
    const Art::Entity portal = load("portal.ent");
    CHECK_MSG(portal.sprite == "portal_halo.png" && portal.additive && portal.systems.size() == 2 &&
                  !portal.Temporary() && !portal.light,
              "portal: portal_halo.png added, two systems, not temporary, no light");
    CHECK_MSG(portal.scale == glm::dvec2(1.0) && portal.columns == 1 && portal.rows == 1 &&
                  portal.pivotPx == glm::dvec2(0.0),
              "no <Scale>, <SpriteCut> or <PivotAdjust>: (1, 1), 1 x 1, (0, 0)");
    if (portal.systems.size() == 2) {
        CHECK_MSG(portal.systems[0].bitmap == "portal_spiral.png" && portal.systems[0].alphaMode == 1 &&
                      portal.systems[0].count == 4 && portal.systems[0].startPointZ == 10.0,
                  "sys0 the spiral, added, four, 10 above");
        CHECK_MSG(portal.systems[1].bitmap == "portal.png" && portal.systems[1].alphaMode == Particles::kAlphaPixel,
                  "sys1 the iris, alpha-blended (a0)");
    }

    // enemy_shoot.ent: the minions' shot. A sheet of six and a light with a halo,
    // and no systems - so never temporary.
    const Art::Entity shoot = load("enemy_shoot.ent");
    CHECK_MSG(shoot.sprite == "projectile_transparent.png" && shoot.columns == 6 && shoot.rows == 1 &&
                  shoot.systems.empty() && !shoot.Temporary() && shoot.applyLight && !shoot.additive,
              "enemy_shoot: projectile_transparent.png 6 x 1, no systems, applies light, mixed");
    CHECK_MSG(shoot.pivotPx == glm::dvec2(4.0, 0.0), "PivotAdjust (4, 0)");
    CHECK_MSG(shoot.light && shoot.light->range == 64.0 && shoot.light->colour == glm::dvec3(2.0, 0.0, 0.0) &&
                  shoot.light->offset == glm::dvec3(0.0, 0.0, -28.0) && shoot.light->halo == "halo.bmp" &&
                  shoot.light->haloSize == glm::dvec2(146.0) && shoot.light->haloBrightness == 0.25,
              "its light: range 64, (2, 0, 0), 28 below; halo.bmp 146 across at 0.25");

    // explosion_large.ent, what a barrel's blast adds (explode #23): <Scale> 1.4,
    // so both systems are MADE at 1.4 - every length, no time.
    const Art::Entity large = load("explosion_large.ent");
    CHECK_MSG(large.scale == glm::dvec2(1.4) && large.systems.size() == 2 && large.Temporary(),
              "explosion_large: <Scale> 1.4, two systems, temporary");
    CHECK_MSG(large.light && large.light->halo.empty() && large.light->range == 191.0 &&
                  large.light->offset.z == 14.0,
              "a light of range 191, 14 above, and no halo: it names no bitmap");
    if (large.systems.size() == 2) {
        const double made = large.ParticleScale(1.0);
        CHECK_MSG(NearD(made, 1.4), "made at 1.4");
        const Particles::System burst = Particles::Scaled(large.systems[0], made);
        CHECK_MSG(NearD(burst.size, 179.2) && NearD(burst.growth, 3.64) && NearD(burst.maxSize, 350.0) &&
                      burst.minSize == 0.0 && burst.randomizeSize == 0.0,
                  "sys0: 128 -> 179.2 across, growth 2.6 -> 3.64, up to 250 -> 350");
        CHECK_MSG(burst.lifeTimeMs == 450.0 && burst.randomLifeTimeMs == 355.0 && burst.randAngleStart == 360.0 &&
                      burst.count == 3 && burst.columns == 4 && burst.rows == 3,
                  "and no time, angle, count or cut");
        const Particles::System ring = Particles::Scaled(large.systems[1], made);
        CHECK_MSG(NearD(ring.size, 2.8) && NearD(ring.growth, 10.5) && NearD(ring.maxSize, 350.0) &&
                      ring.lifeTimeMs == 500.0,
                  "sys1, the ring: 2 -> 2.8, growth 7.5 -> 10.5");
        CHECK_MSG(large.systems[0].size == 128.0, "the entity's own systems stay as the file states them");
    }
}

// Counted over the 190 files by an independent reading (Python's XML parser and
// the engine's rules, step 82): 51 are temporary, 19 keep a light, 9 of those a
// halo, 121 name a sprite and 9 a <Scale> other than 1 (the barrel bombs 1.3,
// black_halo 4, eclipse and wall02_blur 2, explosion_large 1.4, fireball 1.2,
// both small explosions 0.8).
constexpr int kTemporaryEnts = 51;
constexpr int kLitEnts = 19;
constexpr int kHaloedEnts = 9;
constexpr int kPicturedEnts = 121;
constexpr int kScaledEnts = 9;

// Every .ent of the original's reads as an entity, its systems exactly as
// Particles::Load reads them; and the pictures art.json copies out of seven of them
// are what LoadEnt reads there - but for one number, below.
void EveryEntReadsAsAnEntity() {
    int files = 0;
    int temporary = 0;
    int lit = 0;
    int haloed = 0;
    int pictured = 0;
    int scaled = 0;
    int mismatched = 0;
    std::string firstError;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(kOriginal + "/entities", ec)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".ent") continue;
        ++files;
        Art::Entity entity;
        std::string error;
        if (!Art::LoadEnt(entry.path().string(), entity, error)) {
            if (firstError.empty()) firstError = error;
            continue;
        }
        std::vector<Particles::System> systems;
        Particles::Load(entry.path().string(), systems, error);
        if (systems.size() != entity.systems.size()) ++mismatched;
        if (entity.Temporary()) ++temporary;
        if (entity.light) ++lit;
        if (entity.light && !entity.light->halo.empty()) ++haloed;
        if (!entity.sprite.empty()) ++pictured;
        if (entity.scale != glm::dvec2(1.0)) ++scaled;
    }
    CHECK_MSG(firstError.empty(), "an .ent would not read as an entity: " + firstError);
    CHECK_EQ(files, 190);
    CHECK_EQ(mismatched, 0);
    std::printf("  LoadEnt: %d files; %d temporary, %d with a light (%d with a halo), %d with a sprite, "
                "%d with a <Scale> other than 1\n",
                files, temporary, lit, haloed, pictured, scaled);
    CHECK_EQ(temporary, kTemporaryEnts);
    CHECK_EQ(lit, kLitEnts);
    CHECK_EQ(haloed, kHaloedEnts);
    CHECK_EQ(pictured, kPicturedEnts);
    CHECK_EQ(scaled, kScaledEnts);

    Art::Rules rules;
    std::string error;
    CHECK_MSG(Art::LoadRules(std::string(MAGICPORTALS_PORT_DATA_DIR) + "/art.json", rules, error), error);
    const auto same = [&](const char* file, const Art::Picture& copied, bool haloOffset) {
        Art::Entity read;
        std::string why;
        CHECK_MSG(Art::LoadEnt(kOriginal + "/entities/" + file, read, why), why);
        bool lightSame = read.light.has_value() == copied.light.has_value();
        if (lightSame && read.light) {
            const Lighting::Light& a = *read.light;
            const Lighting::Light& b = *copied.light;
            lightSame = a.range == b.range && a.offset == b.offset && a.colour == b.colour && a.halo == b.halo &&
                        (!haloOffset || a.haloOffset == b.haloOffset) && a.haloSize == b.haloSize &&
                        a.haloBrightness == b.haloBrightness;
        }
        CHECK_MSG(read.sprite == copied.sprite && read.additive == copied.additive &&
                      read.columns == copied.columns && read.rows == copied.rows &&
                      read.emissive == copied.emissive && read.isStatic == copied.isStatic &&
                      read.applyLight == copied.applyLight && read.normal == copied.normal && lightSame,
                  std::string("art.json's picture of ") + file + " is what LoadEnt reads there");
    };
    same("portal.ent", rules.portal, true);
    same("projectile.ent", rules.shot, true);
    same("light_from_projectile.ent", rules.torchLight, false);
    same("dark_mage.ent", rules.character, true);
    same("beholder.ent", rules.beholder, true);
    same("beholder_spike.ent", rules.spike, true);
    same("timer.ent", rules.timer, true);
    // The one number that is not: a lit torch's halo. DrawHalo draws it at the
    // light's own position (ETHRenderEntity.cpp:376-386), 12 above the torch's point
    // in light_from_projectile.ent, as the converter places every level light's
    // (tscn.py:328); art.json's torch_light carries a halo_offset of (0, 0), so the
    // port draws that halo 12 units low. Found by this reader, recorded in step 82
    // and left for the step that draws the lit torch (00_order 7f-5): not a change
    // this step makes, whose gate is no pixel change.
    {
        Art::Entity torch;
        std::string why;
        CHECK_MSG(Art::LoadEnt(kOriginal + "/entities/light_from_projectile.ent", torch, why) && torch.light &&
                      torch.light->haloOffset == glm::dvec2(0.0, -12.0),
                  "light_from_projectile.ent's halo stands at its light's (0, -12)");
    }
    // And the pivots art.json copies.
    Art::Entity read;
    std::string why;
    CHECK(Art::LoadEnt(kOriginal + "/entities/projectile.ent", read, why) &&
          read.pivotPx == glm::dvec2(rules.shot.pivotXPx, rules.shot.pivotYPx));
    CHECK(Art::LoadEnt(kOriginal + "/entities/dark_mage.ent", read, why) &&
          read.pivotPx == glm::dvec2(rules.character.pivotXPx, rules.character.pivotYPx));
    CHECK(Art::LoadEnt(kOriginal + "/entities/beholder_spike.ent", read, why) &&
          read.pivotPx == glm::dvec2(rules.spike.pivotXPx, rules.spike.pivotYPx));
}

int main() {
    AnImageSaysItsSizeInItsHeader();
    AnythingElseIsRefusedByName();
    AJpegSaysItsSizeInItsFrameHeader();
    AJpegTheRendererCannotReadIsRefused();
    TheCanvasOrderIsZThenTheFile();
    WhatTheReaderDoesNotDrawIsNamed();
    ASpriteIsDrawnFromItsTier();
    ASpriteIsDrawnAtItsScale();
    ThePortalAndTheShotAreTheirEnts();
    TheShotTurnsItsHeadOntoItsFlight();
    AStaticPortalWithoutItsScriptIsRefused();
    TheAntiportalBlinksRedAndTurns();
    AnAntiportalWithoutItsBlinkIsRefused();
    TheTimerIsTimerEnt();
    ATimerWithoutItsClockIsRefused();
    TheLightWallGhostIsDashedDoorEnt();
    AGhostWithoutItsCallbackIsRefused();
    TheMinionIsMinionEnt();
    AMinionWithoutItsFramingIsRefused();
    AMinionIsFramedAsItsScriptFramesIt();
    APulseGoesThereAndBack();
    ASheetThatDoesNotSayHowFastIsRefused();
    APlayerWithoutItsWalkIsRefused();
    AFrameTimerStepsOneFrameACall();
    AWeightlessPlayerTurnsItsColumnAndHovers();
    APictureWithoutItsEmissiveIsRefused();
    APictureWithoutItsLightingIsRefused();
    AParticleStartsAtItsEntitysPosition();
    AParticleTurnsCounterClockwise();
    AnAddedParticleIgnoresItsAlpha();
    ASystemDrawsInsideItsEntitysSlot();
    AParticlesQuadIsSquare();
    AScaledSystemIsEthanonsScale();
    TheLoopKeepsEthanonsOrder();
    TheRandomNumbersAreDrawnInEthanonsOrder();
    APoolIsOverWhenEveryParticleIsSpent();
    AScaledCopyLeavesItsSystemAsItWas();
    AnEntReadsAsTheEngineReadsIt();

    std::error_code original;
    if (std::filesystem::is_directory(kOriginal + "/entities", original)) {
        TheOriginalsImagesAreCutAsTheEntsSay();
        ThePortsOwnPicturesAreDrawnFromTheirTiers();
        TheMinionSheetIsCutWhereTheOriginalCutsIt();
        EveryOriginalImageIsSizedAsTheRendererReadsIt();
        TheOriginalsParticlesAreRead();
        AnEntityWithoutParticlesSaysSoWithoutFailing();
        TheEffectsAreTheirEnts();
        EveryEntReadsAsAnEntity();
        EveryAddedBitmapIsWithoutAlpha();
        TheOriginalsMp3sDecode();
    } else {
        std::printf("test_mp_sprites: the original's images SKIPPED - needs its extracted assets at %s.\n",
                    kOriginal.c_str());
    }

    std::error_code ec;
    if (!std::filesystem::is_directory(kLevels, ec)) {
        std::printf("test_mp_sprites: the levels' part SKIPPED - needs the converted levels at %s.\n"
                    "  They live outside this repository; configure with -DSUPERSONIC_MAGICPORTALS_LEVELS=...\n",
                    kLevels.c_str());
        return ::test::summary("test_mp_sprites", 50);
    }
    Level8sArt();
    Level0sStaticPortalsAreAdded();
    AnOffsetAndATurn();
    ThePlacementsCarryTheSystemsTheirLevelsState();
    return ::test::summary("test_mp_sprites", 70);
}

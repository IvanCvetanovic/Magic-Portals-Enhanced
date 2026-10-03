#include "sim/Art.hpp"

#include "core/DetMath.hpp"
#include "core/Json.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <exception>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace MagicPortals::Art {

namespace {

namespace Json = Supersonic::Json;

// art.json black_bar's two sides (BarSide), as the file names them.
constexpr const char* kBarAboveTop = "above_top";
constexpr const char* kBarBelowBottom = "below_bottom";

bool WholeBelow(const Json::Value& value, int below, int& out) {
    if (!value.IsNumber()) return false;
    const double n = value.AsNumber();
    if (n < 0.0 || n != std::floor(n) || n >= below) return false;
    out = static_cast<int>(n);
    return true;
}

bool WholeAtLeastOne(const Json::Value& value, int& out) {
    if (!value.IsNumber()) return false;
    const double n = value.AsNumber();
    if (n < 1.0 || n != std::floor(n) || n > 64.0) return false;
    out = static_cast<int>(n);
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

// Three numbers, none below zero: an .ent's <EmissiveColor> r, g and b.
bool Emissive(const Json::Value& value, glm::dvec3& out) {
    if (!value.IsArray() || value.AsArray().size() != 3) return false;
    for (std::size_t i = 0; i < 3; ++i) {
        const Json::Value& channel = value.AsArray()[i];
        if (!channel.IsNumber() || !(channel.AsNumber() >= 0.0)) return false;
        out[static_cast<glm::length_t>(i)] = channel.AsNumber();
    }
    return true;
}

bool Finite(const Json::Value& value) {
    return value.IsNumber() && std::isfinite(value.AsNumber());
}

bool Numbers(const Json::Value& value, std::size_t count, double* out) {
    if (!value.IsArray() || value.AsArray().size() != count) return false;
    for (std::size_t i = 0; i < count; ++i) {
        if (!Finite(value.AsArray()[i])) return false;
        out[i] = value.AsArray()[i].AsNumber();
    }
    return true;
}

// A picture's <Light>, every number stated: a range above zero (ETHEntityProperties.cpp
// keeps no other light), the offset in three, a colour of three none below zero,
// and its halo - a file, where it stands, how big and how bright. And the depth its
// height is measured from, which the light cannot be placed without.
bool ReadLight(const Json::Value& entry, const Json::Value& z, const std::string& name, Picture& into,
               std::string& error) {
    Lighting::Light light;
    double offset[3] = {0.0, 0.0, 0.0};
    double colour[3] = {0.0, 0.0, 0.0};
    double haloOffset[2] = {0.0, 0.0};
    double haloSize[2] = {0.0, 0.0};
    const bool ok = entry.IsObject() && Finite(entry["range"]) && entry["range"].AsNumber() > 0.0 &&
                    Numbers(entry["offset"], 3, offset) && Numbers(entry["colour"], 3, colour) &&
                    colour[0] >= 0.0 && colour[1] >= 0.0 && colour[2] >= 0.0 && entry["halo"].IsString() &&
                    !entry["halo"].AsString("").empty() && Numbers(entry["halo_offset"], 2, haloOffset) &&
                    Numbers(entry["halo_size"], 2, haloSize) && haloSize[0] > 0.0 && haloSize[1] > 0.0 &&
                    Finite(entry["halo_brightness"]) && entry["halo_brightness"].AsNumber() >= 0.0;
    if (!ok) {
        error = name + "'s light needs a range above 0, offset, colour, halo, halo_offset, halo_size and "
                       "halo_brightness";
        return false;
    }
    if (!z.IsObject() || !Finite(z["value"])) {
        error = name + " has a light and no z.value to measure its height from";
        return false;
    }
    light.range = entry["range"].AsNumber();
    light.offset = glm::dvec3(offset[0], offset[1], offset[2]);
    light.colour = glm::dvec3(colour[0], colour[1], colour[2]);
    light.halo = entry["halo"].AsString("");
    light.haloOffset = glm::dvec2(haloOffset[0], haloOffset[1]);
    light.haloSize = glm::dvec2(haloSize[0], haloSize[1]);
    light.haloBrightness = entry["halo_brightness"].AsNumber();
    into.light = light;
    into.z = z["value"].AsNumber();
    return true;
}

// `plays` false for a sheet whose frames are chosen rather than played, which
// then need not say how fast.
bool ReadPicture(const Json::Value& entry, const std::string& name, Picture& out, std::string& error,
                 bool plays = true) {
    if (!entry.IsObject() || !entry.Has("sprite") || !entry["sprite"].IsString() || !entry.Has("additive") ||
        !entry["additive"].IsBool()) {
        error = name + " needs sprite and additive";
        return false;
    }
    Picture read;
    read.sprite = entry["sprite"].AsString("");
    read.additive = entry["additive"].AsBool();
    // Required: a picture without one would be drawn at the ambient alone, which
    // for the player on a dark level is black - a default nobody decoded.
    if (!Emissive(entry["emissive"], read.emissive)) {
        error = name + "'s emissive is not three numbers, none below zero";
        return false;
    }
    // Which lights reach it, and none by default: a picture that said nothing
    // would be unlit by a rule nobody read out of its .ent.
    if (!entry.Has("static") || !entry["static"].IsBool() || !entry.Has("apply_light") ||
        !entry["apply_light"].IsBool()) {
        error = name + " needs static and apply_light, each true or false";
        return false;
    }
    read.isStatic = entry["static"].AsBool();
    read.applyLight = entry["apply_light"].AsBool();
    if (entry.Has("normal")) {
        if (!entry["normal"].IsString() || entry["normal"].AsString("").empty()) {
            error = name + "'s normal is not a file name";
            return false;
        }
        read.normal = entry["normal"].AsString("");
    }
    if (entry.Has("light") && !ReadLight(entry["light"], entry["z"], name, read, error)) return false;
    if ((entry.Has("columns") && !WholeAtLeastOne(entry["columns"], read.columns)) ||
        (entry.Has("rows") && !WholeAtLeastOne(entry["rows"], read.rows))) {
        error = name + "'s columns and rows are whole numbers from 1";
        return false;
    }
    // A sheet of more than one frame plays, so it has to say how fast.
    if (plays && read.Frames() > 1) {
        const Json::Value& animation = entry["animation"];
        if (!animation.IsObject() || !animation.Has("frames_per_second") ||
            !animation["frames_per_second"].IsNumber() || animation["frames_per_second"].AsNumber() <= 0.0) {
            error = name + " has " + std::to_string(read.Frames()) +
                    " frames and no animation.frames_per_second above zero";
            return false;
        }
        read.framesPerSecond = animation["frames_per_second"].AsNumber();
    }
    out = std::move(read);
    return true;
}

// What the shot adds to its picture, none of it defaulted: a scale nobody decoded
// would draw it at the wrong size, and a stride nobody decoded is the 15 fps
// guess this replaced.
bool ReadShot(const Json::Value& entry, Shot& out, std::string& error) {
    Shot read;
    if (!ReadPicture(entry, "shot", static_cast<Picture&>(read), error, false)) return false;
    const Json::Value& scale = entry["scale"];
    const Json::Value& animation = entry["animation"];
    glm::dvec2 pivot(0.0);
    if (!scale.IsObject() || !Finite(scale["value"]) || scale["value"].AsNumber() <= 0.0 ||
        !Pair(entry["pivot_px"], pivot) || !std::isfinite(pivot.x) || !std::isfinite(pivot.y) ||
        !entry["turn"].IsString() || !animation.IsObject() || !Finite(animation["stride_ms"]) ||
        animation["stride_ms"].AsNumber() <= 0.0) {
        error = "shot needs scale.value above 0, pivot_px, turn and animation.stride_ms above 0";
        return false;
    }
    const std::string turn = entry["turn"].AsString("");
    if (turn != "toward_flight_minus_90" && turn != "none") {
        error = "shot's turn is toward_flight_minus_90 or none, not '" + turn + "'";
        return false;
    }
    read.scale = scale["value"].AsNumber();
    read.pivotXPx = pivot.x;
    read.pivotYPx = pivot.y;
    read.turnsToFlight = turn == "toward_flight_minus_90";
    read.strideMs = animation["stride_ms"].AsNumber();
    read.framesPerSecond = 1000.0 / read.strideMs;
    out = std::move(read);
    return true;
}

bool ReadPulse(const Json::Value& entry, Pulse& out) {
    Pulse read;
    if (!entry.IsObject() || !Pair(entry["from"], read.fromScale) || !Pair(entry["to"], read.toScale) ||
        !entry["stride_ms"].IsNumber() || entry["stride_ms"].AsNumber() <= 0.0) {
        return false;
    }
    read.strideMs = entry["stride_ms"].AsNumber();
    out = read;
    return true;
}

} // namespace

glm::dvec2 Pulse::ScaleAt(double elapsedMs) const {
    // bounce(): the count of strides so far says which way it is going, and how
    // far through this one it is, eased by smoothEnd, says where.
    const double strides = std::floor(elapsedMs / strideMs);
    double bias = (elapsedMs - strides * strideMs) / strideMs;
    if (std::fmod(strides, 2.0) == 1.0) bias = 1.0 - bias;
    const double eased = Supersonic::DetMath::sin(static_cast<float>(bias) * 1.570796327f);
    return fromScale + (toScale - fromScale) * eased;
}

double LightWallGhost::AlphaAt(double angleDeg) const {
    // SetAlpha(cos(degreeToRadian(angle)) x amplitude + base) (ETHCallback_light_wall_ghost
    // ins 49-71). The script's angle and its cosine are floats, so the port's are:
    // 0.1 to 0.3 over the 1.2 s the 300 degrees a second take.
    const double radians = angleDeg * 3.14159265358979323846 / 180.0;
    return static_cast<double>(Supersonic::DetMath::cos(static_cast<float>(radians))) * alphaAmplitude + alphaBase;
}

std::vector<glm::dvec2> BlackBars::Centres(const glm::dvec2& sceneMax) const {
    // halfBlackBarSize = blackBarSize x 0.5 (manageSceneBounds ins 51-57); x is
    // half.x or half.x x 2 (ins 96, 159 MULIf 2f), y is -half.y above the scene
    // (ins 59-80, 84-106) or sceneMax.y + half.y below it (ins 116-124, 147-155).
    const glm::dvec2 half = sizeUnits * 0.5;
    std::vector<glm::dvec2> centres;
    centres.reserve(bars.size());
    for (const BlackBar& bar : bars) {
        const double y = bar.side == BarSide::AboveTop ? -half.y : sceneMax.y + half.y;
        centres.emplace_back(half.x * static_cast<double>(bar.xHalfWidths), y);
    }
    return centres;
}

float Shot::AngleDegrees(const glm::dvec2& directionPx) const {
    if (!turnsToFlight) return 0.0f;
    // ETHCallback_projectile #102-111: getAngle(dir), radianToDegree, SUBIf 90f,
    // SetAngle - in floats, as the script's are.
    constexpr float kPi = 3.141592654f; // gs2d::math::constant::PI
    float radians = Supersonic::DetMath::atan2(static_cast<float>(directionPx.x), static_cast<float>(directionPx.y));
    if (radians < 0.0f) radians += 2.0f * kPi;
    return radians / (kPi * 2.0f) * 360.0f - 90.0f;
}

glm::dvec2 Shot::CentrePx(const glm::dvec2& atPx, float angleDegrees) const {
    // pivotAdjust x scale, turned by -DegreeToRadian(angle) in the screen's y-down
    // axes (which is counter-clockwise as it is seen), and taken off the position.
    const glm::dvec2 pivot = glm::dvec2(pivotXPx, pivotYPx) * scale;
    float s = 0.0f;
    float c = 1.0f;
    Supersonic::DetMath::sincos(-angleDegrees * (3.141592654f / 180.0f), s, c);
    const double sd = static_cast<double>(s);
    const double cd = static_cast<double>(c);
    return atPx - glm::dvec2(cd * pivot.x - sd * pivot.y, sd * pivot.x + cd * pivot.y);
}

glm::dvec3 Antiportal::ColourAt(double elapsedMs) const {
    if (!(strideMs > 0.0)) return from;
    // blinkColor ins 55-75, in the order it runs them: which leg this is
    // (elapsedTime / stride), which way it runs (that count odd), and how far along
    // it is (the remainder over the stride). The two clocks do NOT run at one rate: the
    // original sums a FLOOR of each frame's milliseconds as a uint (ETHEngine.cpp:147
    // truncates the loop's float, and libApplication.so's Update(float) calls
    // __aeabi_f2uiz before SetLastFrameElapsedTime), so at 60 Hz it gains 16 ms a frame.
    // The caller's clock is that sum since planning step 147 (sim/ScriptClock): the
    // level's age gains the script's 16 ms a tick, so the blink runs at the original's rate.
    const double ms = std::max(0.0, elapsedMs);
    const double legs = std::floor(ms / strideMs);
    double bias = (ms - legs * strideMs) / strideMs;
    if (std::fmod(legs, 2.0) == 1.0) bias = 1.0 - bias;
    return from + (to - from) * bias;
}

glm::dvec3 Blink::At(double elapsedMs) const {
    if (!(strideMs > 0.0)) return from;
    // blinkColor's ins 55-75, which blinkEmissive repeats instruction for
    // instruction: the leg, the way it runs, how far along it is. The clock is
    // the caller's (art.json non_teleportable _clock, antiportal's _source): the script's floor.
    const double ms = std::max(0.0, elapsedMs);
    const double legs = std::floor(ms / strideMs);
    double bias = (ms - legs * strideMs) / strideMs;
    if (std::fmod(legs, 2.0) == 1.0) bias = 1.0 - bias;
    return from + (to - from) * bias;
}

bool KeyColour::Paints(const std::string& bareEntity) const {
    return std::find(entities.begin(), entities.end(), bareEntity) != entities.end();
}

std::string KeyColour::SpriteFor(const std::string& entityName, const std::string& hue) const {
    // GetEntityName() + suffix (setKeyColor #26-46, #88-108): the name as the level
    // spells it, so a .ent spelling would name a file that is not there.
    for (const auto& [named, suffix] : sprites) {
        if (named == hue) return entityName + suffix;
    }
    return {};
}

NonTeleportable::Blinks NonTeleportable::For(const std::string& bareEntity, bool teleportable) const {
    // manageNonTeleportable #1-10 leaves a teleportable crate alone; #23-24 picks
    // the emissive for a zeroEmissive caller and the colour for the rest.
    if (teleportable) return Blinks::Nothing;
    for (const auto& [entity, zeroEmissive] : callbacks) {
        if (entity == bareEntity) return zeroEmissive ? Blinks::Emissive : Blinks::Colour;
    }
    return Blinks::Nothing;
}

int Timer::FrameAt(double elapsedMs, double timeMs) const {
    if (!(timeMs > 0.0)) return frames - 1;
    // uTOf, DIVf, MULIf 8f and fTOi (ETHCallback_timer ins 233-239), then
    // max(frame, 0) and min(7) (ins 242-248): single precision, as the script's
    // floats are, so a cell turns on the same millisecond.
    const float at = static_cast<float>(elapsedMs) / static_cast<float>(timeMs) * static_cast<float>(frames);
    return static_cast<int>(std::clamp(at, 0.0f, static_cast<float>(frames - 1)));
}

double Timer::LegMs(double elapsedMs, double timeMs) const {
    return std::max(pulseMinLegMs, timeMs - elapsedMs);
}

glm::dvec2 Timer::PulseAt(double elapsedMs, double timeMs) const {
    // bounce(thisEntity, V2_ONE, vector2(1.15, 1.15), max(400, timeLeft)) (ins
    // 204-231): its blinkElapsedTime starts at 0 and takes the same frame times
    // as elapsedTime, so the two are one clock. Only the leg changes each frame.
    Pulse pulse;
    pulse.fromScale = glm::dvec2(pulseFrom);
    pulse.toScale = glm::dvec2(pulseTo);
    pulse.strideMs = LegMs(elapsedMs, timeMs);
    return pulse.ScaleAt(elapsedMs);
}

double Timer::DecayOver(double seconds) const {
    return std::pow(shrinkPerFrame, seconds * decayFramesPerSecond);
}

bool Timer::Gone(double scaleX) const {
    // GetScale().x < 0.1f (ETHCallback_timer ins 184-199). GetScale is what
    // SetScale stored, and bounce stored g_scale.scale(the pulse) (ins 90-102):
    // the port's scale x m_scaleFactor. Only this test is in those units; the
    // port draws the cell at its own.
    return scaleX * screenPxPerUnit < goneBelowScale;
}

double NoGravityMotion::HoverUnits(float angle, double viewUnitsTall) const {
    // SetPivotAdjust(originalPivotAdjust + vector2(0, cos(angle) * 1.2)): screen
    // pixels, since the pivot is kept times the entity's scale and drawn times it
    // again; units as a screen hoverAtScreenPx tall shows them.
    return hoverScreenPx * static_cast<double>(Supersonic::DetMath::cos(angle)) * viewUnitsTall / hoverAtScreenPx;
}

const std::string& PlayerSheet(const Character& mage, bool noGravity) {
    return noGravity ? mage.noGravitySprite : mage.sprite;
}

int FrameTimer::Set(int firstFrame, int lastFrame, double strideMs, bool repeat, double elapsedMs) {
    timeMs += elapsedMs;
    if (firstFrame != first || lastFrame != last) {
        frame = firstFrame;
        first = firstFrame;
        last = lastFrame;
        timeMs = 0.0;
        return frame;
    }
    if (timeMs >= strideMs) {
        ++frame;
        timeMs -= strideMs;
        if (frame > last) {
            if (repeat) {
                frame = first;
            } else {
                frame = last;
                timeMs = 0.0;
            }
        }
    }
    return frame;
}

int UpdateNoGravity(const Character& mage, NoGravityPlayer& player, bool walking, double elapsedMs) {
    const int lastColumn = mage.columns - 1;
    // SideScrollerCharacter::updateFrame: a walk draws its own set; a stand draws
    // the idle column as the update before left it.
    const int drawn = walking ? player.timer.Set(0, lastColumn, mage.strideMs, true, elapsedMs) : player.idleColumn;
    // MainCharacter::linearMotion.
    const NoGravityMotion& motion = mage.noGravity;
    if (!player.moved) {
        player.angle = static_cast<float>(motion.hoverStartRadians);
        player.moved = true;
    }
    player.angle += static_cast<float>(motion.hoverRadiansPerSecond * elapsedMs / 1000.0);
    if (player.angle > static_cast<float>(motion.hoverWrapRadians)) {
        player.angle -= static_cast<float>(motion.hoverWrapRadians);
    }
    player.idleColumn = player.timer.Set(0, lastColumn, motion.columnStrideMs, true, elapsedMs);
    return drawn;
}

void UpdateMinion(const Minion& art, MinionPose& pose, MinionArm arm, bool right, double elapsedMs,
                  double sinceArrivalMs) {
    const int stored = pose.dirRow < 0 ? art.startRow : pose.dirRow;
    switch (arm) {
    case MinionArm::Walk:
        // ins 1122-1162: dirRow from the way it walks, stored, then
        // SetFrame(timer.set(first, last, stride, true), dirRow).
        pose.dirRow = right ? art.rightRow : art.leftRow;
        pose.row = pose.dirRow;
        pose.column = pose.timer.Set(art.firstColumn, art.lastColumn, art.strideMs, true, elapsedMs);
        return;
    case MinionArm::Hold:
        // ins 1202-1205: SetFrame(0, GetUInt('dirRow')), the timer untouched.
        pose.row = stored;
        pose.column = art.holdColumn;
        return;
    case MinionArm::Search:
        // ins 1243-1321: elapsedTime % 4000 > 2000 looks right, else left, and that
        // is stored as dirRow before SetFrame(0, dirRow). The time is a uint.
        pose.dirRow = std::fmod(std::floor(std::max(0.0, sinceArrivalMs)), art.searchPeriodMs) > art.searchRightAfterMs
                          ? art.rightRow
                          : art.leftRow;
        pose.row = pose.dirRow;
        pose.column = art.holdColumn;
        return;
    case MinionArm::None:
    case MinionArm::Settled:
        return;
    }
}

bool LoadRules(const std::string& path, Rules& out, std::string& error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = path + ": cannot open";
        return false;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string text = buffer.str();
    Json::Parser parser(text);
    Json::Value root;
    if (!parser.Parse(root)) {
        error = path + ": " + parser.Error();
        return false;
    }
    if (!root.IsObject() || !root.Has("portal") || !root.Has("shot") || !root.Has("character")) {
        error = path + ": portal, shot and character are each an object";
        return false;
    }
    Rules read;
    std::string why;
    if (!ReadPicture(root["portal"], "portal", read.portal, why) ||
        !ReadShot(root["shot"], read.shot, why) ||
        // The player's rate is its stride, read with the rest of its walk below.
        !ReadPicture(root["character"], "character", static_cast<Picture&>(read.character), why, false)) {
        error = path + ": " + why;
        return false;
    }
    // What the player's sheet adds: which way it faces when it is made, where it
    // stands, which row walks which way, which column it stands on, and a
    // column's time. None has a default: a start facing or a stride nobody
    // decoded is the picture the footage refused.
    const Json::Value& character = root["character"];
    Character& mage = read.character;
    const Json::Value& pivot = character["pivot_px"];
    const Json::Value& rowsBy = character["rows_by_direction"];
    const Json::Value& walk = character["animation"];
    if (!character.Has("initial_direction") ||
        !WholeBelow(character["initial_direction"], mage.rows, mage.initialDirection) || !pivot.IsArray() ||
        pivot.AsArray().size() != 2 || !pivot.AsArray()[0].IsNumber() || !pivot.AsArray()[1].IsNumber() ||
        !rowsBy.IsObject() || !rowsBy.Has("left") || !WholeBelow(rowsBy["left"], mage.rows, mage.leftRow) ||
        !rowsBy.Has("right") || !WholeBelow(rowsBy["right"], mage.rows, mage.rightRow) || !walk.IsObject() ||
        !walk.Has("idle_column") || !WholeBelow(walk["idle_column"], mage.columns, mage.idleColumn) ||
        !Finite(walk["stride_ms"]) || !(walk["stride_ms"].AsNumber() > 0.0)) {
        error = path + ": character needs initial_direction, pivot_px, rows_by_direction.left and .right and "
                       "animation.idle_column, each inside its sheet, and animation.stride_ms above 0";
        return false;
    }
    mage.pivotXPx = pivot.AsArray()[0].AsNumber();
    mage.pivotYPx = pivot.AsArray()[1].AsNumber();
    mage.strideMs = walk["stride_ms"].AsNumber();
    mage.framesPerSecond = 1000.0 / mage.strideMs;
    // Its depth, the decoded one (R10): without it the player is ordered among the
    // art, and lit, by a number nobody read.
    if (!character["z"].IsObject() || !Finite(character["z"]["value"])) {
        error = path + ": character needs z.value, the depth GameCharacter adds it at";
        return false;
    }
    mage.z = character["z"]["value"].AsNumber();

    // Its arm out: the ray detectPushing casts and the rows findFinalDirection
    // draws while it meets something. None defaulted - without them a wall is
    // walked into on the walking row, which the decode refuses.
    const Json::Value& push = character["push"];
    const Json::Value& pushRows = push["rows"];
    Push& arm = mage.push;
    double offset[2] = {0.0, 0.0};
    if (!push.IsObject() || !Finite(push["reach_frame_width_share"]) ||
        !(push["reach_frame_width_share"].AsNumber() > 0.0) || !Numbers(push["offset_px"], 2, offset) ||
        !pushRows.IsObject() || !pushRows.Has("left") || !WholeBelow(pushRows["left"], mage.rows, arm.leftRow) ||
        !pushRows.Has("other") || !WholeBelow(pushRows["other"], mage.rows, arm.otherRow)) {
        error = path + ": character needs push.reach_frame_width_share above 0, "
                       "push.offset_px as two numbers, and push.rows.left and .other, each inside its sheet";
        return false;
    }
    arm.reachFrameWidthShare = push["reach_frame_width_share"].AsNumber();
    arm.offsetPx = glm::dvec2(offset[0], offset[1]);

    // And what a no_gravity level changes: the sheet it wears, the column that
    // turns while it stands, and its hover. None is defaulted - without them the
    // suit is the sheet the footage refused on 18 levels.
    const Json::Value& weightless = character["no_gravity_motion"];
    const auto positive = [](const Json::Value& value) { return Finite(value) && value.AsNumber() > 0.0; };
    if (!character["no_gravity_sprite"].IsString() || character["no_gravity_sprite"].AsString("").empty() ||
        !weightless.IsObject() || !positive(weightless["column_stride_ms"]) ||
        !positive(weightless["hover_screen_px"]) || !positive(weightless["hover_at_screen_px"]) ||
        !positive(weightless["hover_radians_per_second"]) || !Finite(weightless["hover_start_radians"]) ||
        !positive(weightless["hover_wrap_radians"])) {
        error = path + ": character needs no_gravity_sprite, and no_gravity_motion's column_stride_ms, "
                       "hover_screen_px, hover_at_screen_px, hover_radians_per_second and hover_wrap_radians "
                       "above 0 and a hover_start_radians";
        return false;
    }
    mage.noGravitySprite = character["no_gravity_sprite"].AsString("");
    NoGravityMotion& motion = mage.noGravity;
    motion.columnStrideMs = weightless["column_stride_ms"].AsNumber();
    motion.hoverScreenPx = weightless["hover_screen_px"].AsNumber();
    motion.hoverAtScreenPx = weightless["hover_at_screen_px"].AsNumber();
    motion.hoverRadiansPerSecond = weightless["hover_radians_per_second"].AsNumber();
    motion.hoverStartRadians = weightless["hover_start_radians"].AsNumber();
    motion.hoverWrapRadians = weightless["hover_wrap_radians"].AsNumber();

    // Chapter 1's boss and its spikes, whose frames are chosen, not played.
    if (!root.Has("beholder") || !root.Has("spike")) {
        error = path + ": beholder and spike are each an object";
        return false;
    }
    if (!ReadPicture(root["beholder"], "beholder", static_cast<Picture&>(read.beholder), why, false) ||
        !ReadPicture(root["spike"], "spike", static_cast<Picture&>(read.spike), why, false)) {
        error = path + ": " + why;
        return false;
    }
    const Json::Value& pulse = root["beholder"]["pulse"];
    if (!pulse.IsObject() || !ReadPulse(pulse["seeking"], read.beholder.seeking) ||
        !ReadPulse(pulse["hurt"], read.beholder.hurt) || !ReadPulse(pulse["dead"], read.beholder.dead)) {
        error = path + ": beholder.pulse needs seeking, hurt and dead, each with from, to and a stride_ms above 0";
        return false;
    }
    glm::dvec2 spikePivot(0.0);
    if (!Pair(root["spike"]["pivot_px"], spikePivot)) {
        error = path + ": spike.pivot_px is not two numbers";
        return false;
    }
    read.spike.pivotXPx = spikePivot.x;
    read.spike.pivotYPx = spikePivot.y;

    // A static portal as its callback redraws it. Every number is the bytecode's,
    // and none has a default: a scale of 1 or a white tint would be the port's
    // old picture, which is the one the footage refuses.
    const Json::Value& portalStatic = root["static_portal"];
    StaticPortal& statics = read.staticPortal;
    if (!portalStatic.IsObject() || !portalStatic["entity"].IsString() || portalStatic["entity"].AsString("").empty() ||
        !Finite(portalStatic["scale"]) || !(portalStatic["scale"].AsNumber() > 0.0) || !portalStatic["red"].IsString() ||
        portalStatic["red"].AsString("").empty() || !Emissive(portalStatic["tint_red"], statics.tintRed) ||
        !Emissive(portalStatic["tint_otherwise"], statics.tintOtherwise)) {
        error = path + ": static_portal needs entity, a scale above 0, red, and tint_red and tint_otherwise, "
                       "each three numbers none below 0";
        return false;
    }
    statics.entity = portalStatic["entity"].AsString("");
    statics.scale = portalStatic["scale"].AsNumber();
    statics.red = portalStatic["red"].AsString("");

    // The ring round a no-portal field as its callback blinks and turns it. None of
    // it has a default either: without the blink the port draws the white ring the
    // footage refuses, and a spin of 0 would be a picture the script turns and the
    // port does not.
    const Json::Value& anti = root["antiportal"];
    Antiportal& antiportal = read.antiportal;
    const Json::Value& blink = anti["blink"];
    if (!anti.IsObject() || !anti["entity"].IsString() || anti["entity"].AsString("").empty() ||
        anti["entity"].AsString("").find('.') != std::string::npos || !blink.IsObject() ||
        !Emissive(blink["from"], antiportal.from) || !Emissive(blink["to"], antiportal.to) ||
        !Finite(blink["stride_ms"]) || !(blink["stride_ms"].AsNumber() > 0.0) ||
        !Finite(anti["spin_deg_per_s"])) {
        error = path + ": antiportal needs entity, a name without a dot, and blink with from and to, each three "
                       "numbers none below 0, a stride_ms above 0 and a finite spin_deg_per_s";
        return false;
    }
    antiportal.entity = anti["entity"].AsString("");
    antiportal.strideMs = blink["stride_ms"].AsNumber();
    antiportal.spinDegPerS = anti["spin_deg_per_s"].AsNumber();

    // The dial behind a timed crystal: timer.ent's picture, whose cell is chosen
    // rather than played, and the numbers its script runs it by. None has a
    // default: without them the port would draw no dial, or the fade the
    // original never had.
    if (!root.Has("timer")) {
        error = path + ": timer is an object";
        return false;
    }
    const Json::Value& timerEntry = root["timer"];
    if (!ReadPicture(timerEntry, "timer", static_cast<Picture&>(read.timer), why, false)) {
        error = path + ": " + why;
        return false;
    }
    Timer& timer = read.timer;
    const auto between = [&timerEntry](const char* key, double above, double atMost) {
        return Finite(timerEntry[key]) && timerEntry[key].AsNumber() > above && timerEntry[key].AsNumber() <= atMost;
    };
    const Json::Value& decayRate = timerEntry["decay_frames_per_second"];
    const bool clockOk = WholeAtLeastOne(timerEntry["frames"], timer.frames) && timer.frames <= timer.Frames() &&
                         between("alpha", 0.0, 1.0) && Finite(timerEntry["z_offset"]) &&
                         timerEntry["z_offset"].AsNumber() == std::floor(timerEntry["z_offset"].AsNumber()) &&
                         between("pulse_from", 0.0, 1e9) && between("pulse_to", 0.0, 1e9) &&
                         between("pulse_min_leg_ms", 0.0, 1e9) && between("shrink_per_frame", 0.0, 1.0) &&
                         timerEntry["shrink_per_frame"].AsNumber() < 1.0 && between("gone_below_scale", 0.0, 1.0) &&
                         timerEntry["gone_below_scale"].AsNumber() < 1.0 && decayRate.IsObject() &&
                         Finite(decayRate["value"]) && decayRate["value"].AsNumber() > 0.0 &&
                         between("screen_px_per_unit", 0.0, 1e9);
    if (!clockOk) {
        error = path + ": timer needs frames from 1 to its cells, alpha above 0 and at most 1, a whole z_offset, "
                       "pulse_from, pulse_to and pulse_min_leg_ms above 0, shrink_per_frame and gone_below_scale "
                       "between 0 and 1, decay_frames_per_second.value above 0 and screen_px_per_unit above 0";
        return false;
    }
    timer.alpha = timerEntry["alpha"].AsNumber();
    timer.zOffset = static_cast<int>(timerEntry["z_offset"].AsNumber());
    timer.pulseFrom = timerEntry["pulse_from"].AsNumber();
    timer.pulseTo = timerEntry["pulse_to"].AsNumber();
    timer.pulseMinLegMs = timerEntry["pulse_min_leg_ms"].AsNumber();
    timer.shrinkPerFrame = timerEntry["shrink_per_frame"].AsNumber();
    timer.decayFramesPerSecond = decayRate["value"].AsNumber();
    timer.goneBelowScale = timerEntry["gone_below_scale"].AsNumber();
    timer.screenPxPerUnit = timerEntry["screen_px_per_unit"].AsNumber();

    // What a script's raw depth is among the level's art. No default: a factor of 1
    // is the reading the decode refused, and puts the player in front of 99
    // placements the original draws over it.
    const Json::Value& depth = root["depth"];
    if (!depth.IsObject() || !Finite(depth["scale_factor"]) || !(depth["scale_factor"].AsNumber() > 0.0)) {
        error = path + ": depth needs a scale_factor above 0";
        return false;
    }
    read.scaleFactor = depth["scale_factor"].AsNumber();

    // The minions and knights: minion.ent's picture, whose frame is chosen by its
    // script rather than played, and every number that script frames it by. None
    // has a default: without them it is drawn on rows, columns or feet nobody read.
    if (!root.Has("minion")) {
        error = path + ": minion is an object";
        return false;
    }
    const Json::Value& minionEntry = root["minion"];
    if (!ReadPicture(minionEntry, "minion", static_cast<Picture&>(read.minion), why, false)) {
        error = path + ": " + why;
        return false;
    }
    Minion& minion = read.minion;
    glm::dvec2 minionPivot(0.0);
    const Json::Value& minionWalk = minionEntry["walk"];
    const Json::Value& minionRows = minionEntry["rows_by_direction"];
    const Json::Value& search = minionEntry["search"];
    const Json::Value& black = minionEntry["black_knight"];
    const bool minionOk =
        Pair(minionEntry["pivot_px"], minionPivot) && minionEntry["z"].IsObject() &&
        Finite(minionEntry["z"]["value"]) && Finite(minionEntry["feet_below_entity_px"]) &&
        minionEntry["feet_below_entity_px"].AsNumber() >= 0.0 && minionWalk.IsObject() &&
        WholeBelow(minionWalk["first_column"], minion.columns, minion.firstColumn) &&
        WholeBelow(minionWalk["last_column"], minion.columns, minion.lastColumn) &&
        minion.firstColumn <= minion.lastColumn && Finite(minionWalk["stride_ms"]) &&
        minionWalk["stride_ms"].AsNumber() > 0.0 &&
        WholeBelow(minionEntry["hold_column"], minion.columns, minion.holdColumn) &&
        WholeBelow(minionEntry["start_row"], minion.rows, minion.startRow) && minionRows.IsObject() &&
        WholeBelow(minionRows["left"], minion.rows, minion.leftRow) &&
        WholeBelow(minionRows["right"], minion.rows, minion.rightRow) && search.IsObject() &&
        Finite(search["period_ms"]) && search["period_ms"].AsNumber() > 0.0 && Finite(search["right_after_ms"]) &&
        search["right_after_ms"].AsNumber() >= 0.0 &&
        search["right_after_ms"].AsNumber() < search["period_ms"].AsNumber() &&
        minionEntry["darkest_zeroes_emissive"].IsBool() && black.IsObject() && black["entity"].IsString() &&
        !black["entity"].AsString("").empty() && Emissive(black["colour"], minion.blackColour) &&
        Emissive(black["emissive"], minion.blackEmissive);
    if (!minionOk) {
        error = path + ": minion needs pivot_px, z.value, feet_below_entity_px from 0, walk.first_column and "
                       ".last_column in order and a walk.stride_ms above 0, hold_column, start_row and "
                       "rows_by_direction.left and .right inside its sheet, search.period_ms above 0 with "
                       "search.right_after_ms inside it, darkest_zeroes_emissive, and black_knight's entity, "
                       "colour and emissive";
        return false;
    }
    minion.pivotXPx = minionPivot.x;
    minion.pivotYPx = minionPivot.y;
    minion.z = minionEntry["z"]["value"].AsNumber();
    minion.feetBelowEntityPx = minionEntry["feet_below_entity_px"].AsNumber();
    minion.strideMs = minionWalk["stride_ms"].AsNumber();
    minion.searchPeriodMs = search["period_ms"].AsNumber();
    minion.searchRightAfterMs = search["right_after_ms"].AsNumber();
    minion.darkestZeroesEmissive = minionEntry["darkest_zeroes_emissive"].AsBool();
    minion.blackEntity = black["entity"].AsString("");

    // The exit door's second sheet and the glow behind it. None has a default:
    // without the names the port would swap nothing, and a highlight with no depth
    // of its own would be drawn over the door it is meant to stand behind.
    if (!root.Has("door")) {
        error = path + ": door is an object";
        return false;
    }
    const Json::Value& doorEntry = root["door"];
    Door& door = read.door;
    const Json::Value& highlight = doorEntry["highlight"];
    if (!ReadPicture(highlight, "door.highlight", door.highlight, why, false)) {
        error = path + ": " + why;
        return false;
    }
    const auto name = [&doorEntry](const char* key, std::string& into) {
        if (!doorEntry[key].IsString() || doorEntry[key].AsString("").empty()) return false;
        into = doorEntry[key].AsString("");
        return true;
    };
    const bool doorOk = name("entity", door.entity) && name("background_entity", door.backgroundEntity) &&
                        name("shadowless_flag", door.shadowlessFlag) &&
                        name("shadowless_sprite", door.shadowlessSprite) && Finite(highlight["z"]);
    if (!doorOk) {
        error = path + ": door needs entity, background_entity, shadowless_flag and shadowless_sprite, each a "
                       "name, and highlight.z";
        return false;
    }
    door.highlightZ = highlight["z"].AsNumber();

    // The ghost of a taken wall: dashed_door.ent's picture and the two numbers its
    // callback runs it by. None has a default either - an alpha or a pulse the port
    // invented would be a breath nobody decoded, and a ghost with no depth of its
    // own would be drawn in front of the wall it stands behind.
    if (!root.Has("light_wall_ghost")) {
        error = path + ": light_wall_ghost is an object";
        return false;
    }
    const Json::Value& ghostEntry = root["light_wall_ghost"];
    LightWallGhost& ghost = read.lightWallGhost;
    if (!ReadPicture(ghostEntry, "light_wall_ghost", static_cast<Picture&>(ghost), why, false)) {
        error = path + ": " + why;
        return false;
    }
    const bool ghostOk = Finite(ghostEntry["z_offset"]) && Finite(ghostEntry["alpha_base"]) &&
                         ghostEntry["alpha_base"].AsNumber() > 0.0 && ghostEntry["alpha_base"].AsNumber() <= 1.0 &&
                         Finite(ghostEntry["alpha_amplitude"]) && ghostEntry["alpha_amplitude"].AsNumber() > 0.0 &&
                         ghostEntry["alpha_amplitude"].AsNumber() <= ghostEntry["alpha_base"].AsNumber() &&
                         Finite(ghostEntry["spin_deg_per_s"]) && ghostEntry["spin_deg_per_s"].AsNumber() != 0.0 &&
                         ReadPulse(ghostEntry["pulse"], ghost.pulse);
    if (!ghostOk) {
        error = path + ": light_wall_ghost needs a finite z_offset, an alpha_base above 0 and at most 1, an "
                       "alpha_amplitude above 0 and at most alpha_base, a spin_deg_per_s that is not 0, and a "
                       "pulse with from, to and a stride_ms above 0";
        return false;
    }
    ghost.zOffset = ghostEntry["z_offset"].AsNumber();
    ghost.alphaBase = ghostEntry["alpha_base"].AsNumber();
    ghost.alphaAmplitude = ghostEntry["alpha_amplitude"].AsNumber();
    ghost.spinDegPerSec = ghostEntry["spin_deg_per_s"].AsNumber();

    // Chapter 3's boss: ghost.ent's picture, whose cell is chosen by its script,
    // and its pivot. Without the pivot it would hang off its point as nobody read.
    if (!root.Has("ghost")) {
        error = path + ": ghost is an object";
        return false;
    }
    const Json::Value& bossEntry = root["ghost"];
    if (!ReadPicture(bossEntry, "ghost", static_cast<Picture&>(read.ghost), why, false)) {
        error = path + ": " + why;
        return false;
    }
    glm::dvec2 bossPivot(0.0);
    if (!Pair(bossEntry["pivot_px"], bossPivot)) {
        error = path + ": ghost needs pivot_px";
        return false;
    }
    read.ghost.pivotXPx = bossPivot.x;
    read.ghost.pivotYPx = bossPivot.y;

    // The black bars round the scene: the entity manageSceneBounds adds, the bar it
    // measures their centres in, and the four it adds. None has a default: a bar
    // the port placed by its own guess would hide what the original shows, or show
    // what it hides.
    BlackBars& blackBars = read.blackBars;
    const Json::Value& barsEntry = root["black_bar"];
    bool barsOk = root.Has("black_bar") && barsEntry.IsObject() && barsEntry["entity"].IsString() &&
                  !barsEntry["entity"].AsString("").empty() && Pair(barsEntry["size"], blackBars.sizeUnits) &&
                  std::isfinite(blackBars.sizeUnits.x) && std::isfinite(blackBars.sizeUnits.y) &&
                  blackBars.sizeUnits.x > 0.0 && blackBars.sizeUnits.y > 0.0 && barsEntry["bars"].IsArray() &&
                  !barsEntry["bars"].AsArray().empty();
    if (barsOk) {
        for (const Json::Value& entry : barsEntry["bars"].AsArray()) {
            BlackBar bar;
            const std::string side = entry["side"].AsString("");
            if (!entry.IsObject() || !WholeAtLeastOne(entry["x_half_widths"], bar.xHalfWidths) ||
                (side != kBarAboveTop && side != kBarBelowBottom)) {
                barsOk = false;
                break;
            }
            bar.side = side == kBarAboveTop ? BarSide::AboveTop : BarSide::BelowBottom;
            blackBars.bars.push_back(bar);
        }
    }
    if (!barsOk) {
        error = path + ": black_bar needs an entity, a size of two numbers above 0, and bars, each with "
                       "x_half_widths a whole number from 1 and a side of '" +
                std::string(kBarAboveTop) + "' or '" + kBarBelowBottom + "'";
        return false;
    }
    blackBars.entity = barsEntry["entity"].AsString("");

    // The ring a shock agent adds round itself: the entity, and what its two
    // callbacks do to it. None has a default - a colour, a depth, a turn or a
    // jitter the port chose would be a ring nobody decoded - and a jitter whose
    // factor could reach zero or below would draw no ring or a mirrored one.
    ShockRing& ring = read.shockRing;
    const Json::Value& ringEntry = root["shock_ring"];
    double ringColour[3] = {0.0, 0.0, 0.0};
    const Json::Value& ringZ = ringEntry["z"];
    const Json::Value& ringJitter = ringEntry["jitter"];
    const bool ringOk = root.Has("shock_ring") && ringEntry.IsObject() && ringEntry["entity"].IsString() &&
                        !ringEntry["entity"].AsString("").empty() && Numbers(ringEntry["colour"], 3, ringColour) &&
                        ringColour[0] >= 0.0 && ringColour[1] >= 0.0 && ringColour[2] >= 0.0 && ringZ.IsObject() &&
                        Finite(ringZ["value"]) && ringZ["scaled"].IsBool() && Finite(ringEntry["spin_deg_per_s"]) &&
                        ringJitter.IsObject() && Finite(ringJitter["from"]) && ringJitter["from"].AsNumber() > 0.0 &&
                        Finite(ringJitter["span"]) && ringJitter["span"].AsNumber() >= 0.0;
    if (!ringOk) {
        error = path + ": shock_ring needs an entity, a colour of three numbers none below 0, a z with a value "
                       "and scaled, a spin_deg_per_s, and a jitter with a from above 0 and a span from 0";
        return false;
    }
    ring.entity = ringEntry["entity"].AsString("");
    ring.colour = glm::dvec3(ringColour[0], ringColour[1], ringColour[2]);
    ring.z = ringZ["value"].AsNumber();
    ring.zScaled = ringZ["scaled"].AsBool();
    ring.spinDegPerS = ringEntry["spin_deg_per_s"].AsNumber();
    ring.jitterFrom = ringJitter["from"].AsNumber();
    ring.jitterSpan = ringJitter["span"].AsNumber();
    // Its agent's own bounce (ETHCallback_shock_agent #241-263): a pulse that goes
    // somewhere, as the beholder's do, or a row that decoded nothing.
    if (!ReadPulse(ringEntry["agent_pulse"], ring.agentPulse) || ring.agentPulse.fromScale == ring.agentPulse.toScale) {
        error = path + ": shock_ring.agent_pulse needs from and to, two different pairs, and a stride_ms above 0";
        return false;
    }

    // What a carried diamond's and an unlit torch's own callbacks do to them
    // (planning step 146): SetAlpha(0.1f) on every carried update, and
    // AddToAngle(unitsPerSecond(190)) on every unlit one. Both required.
    const Json::Value& carried = root["carried_diamond"]["alpha"];
    if (!Finite(carried) || !(carried.AsNumber() >= 0.0 && carried.AsNumber() < 1.0)) {
        error = path + ": carried_diamond.alpha is the SetAlpha a carried diamond is drawn at, in [0, 1)";
        return false;
    }
    read.carriedDiamond.alpha = carried.AsNumber();
    const Json::Value& spin = root["unlit_torch"]["spin_deg_per_s"];
    if (!Finite(spin) || spin.AsNumber() == 0.0) {
        error = path + ": unlit_torch.spin_deg_per_s is light_off's AddToAngle, in degrees a second, not 0";
        return false;
    }
    read.unlitTorch.spinDegPerS = spin.AsNumber();

    // What three callbacks do to pictures the levels place. None has a default:
    // without them the port draws what the level saved - the anti-portal agent's
    // face on 20 of 22 fire agents, every key and door yellow, every crate still.
    // An entity is named as its callback is, without the .ent; a file by its name
    // alone, since SetSprite looks for it in the entity directory.
    const auto plainName = [](const Json::Value& value) {
        return value.IsString() && !value.AsString("").empty() && value.AsString("").find('.') == std::string::npos;
    };
    const auto plainFile = [](const Json::Value& value) {
        return value.IsString() && !value.AsString("").empty() &&
               value.AsString("").find_first_of("/\\") == std::string::npos;
    };
    const Json::Value& fireEntry = root["fire_agent"];
    if (!fireEntry.IsObject() || !plainName(fireEntry["entity"]) || !plainFile(fireEntry["sprite"])) {
        error = path + ": fire_agent needs an entity, a name without a dot, and a sprite, a file name without a folder";
        return false;
    }
    read.fireAgent.entity = fireEntry["entity"].AsString("");
    read.fireAgent.sprite = fireEntry["sprite"].AsString("");

    KeyColour& keys = read.keyColour;
    const Json::Value& keyEntry = root["key_colour"];
    bool keysOk = keyEntry.IsObject() && !keyEntry["entities"].AsArray().empty() && keyEntry["datum"].IsString() &&
                  !keyEntry["datum"].AsString("").empty() && !keyEntry["sprites"].AsObject().empty() &&
                  Emissive(keyEntry["colour"], keys.colour);
    if (keysOk) {
        for (const Json::Value& entity : keyEntry["entities"].AsArray()) {
            keysOk = keysOk && plainName(entity);
            keys.entities.push_back(entity.AsString(""));
        }
        for (const auto& [colour, suffix] : keyEntry["sprites"].AsObject()) {
            keysOk = keysOk && !colour.empty() && plainFile(suffix);
            keys.sprites.emplace_back(colour, suffix.AsString(""));
        }
    }
    if (!keysOk) {
        error = path + ": key_colour needs entities, names without a dot, a datum, sprites giving each colour a "
                       "suffix without a folder, and a colour of three numbers none below 0";
        return false;
    }
    keys.datum = keyEntry["datum"].AsString("");

    NonTeleportable& crates = read.nonTeleportable;
    const Json::Value& crateEntry = root["non_teleportable"];
    const Json::Value& crateColour = crateEntry["colour"];
    const Json::Value& crateEmissive = crateEntry["emissive"];
    bool cratesOk = crateEntry.IsObject() && !crateEntry["callbacks"].AsArray().empty() &&
                    crateEntry["datum"].IsString() && !crateEntry["datum"].AsString("").empty() &&
                    Finite(crateEntry["stride_ms"]) && crateEntry["stride_ms"].AsNumber() > 0.0 &&
                    Emissive(crateColour["from"], crates.colour.from) && Emissive(crateColour["to"], crates.colour.to) &&
                    Emissive(crateEmissive["from"], crates.emissive.from) &&
                    Emissive(crateEmissive["to"], crates.emissive.to);
    if (cratesOk) {
        for (const Json::Value& entry : crateEntry["callbacks"].AsArray()) {
            const std::string entity = entry["entity"].AsString("");
            const bool twice = std::any_of(crates.callbacks.begin(), crates.callbacks.end(),
                                           [&entity](const auto& known) { return known.first == entity; });
            if (!entry.IsObject() || !plainName(entry["entity"]) || !entry["zero_emissive"].IsBool() || twice) {
                cratesOk = false;
                break;
            }
            crates.callbacks.emplace_back(entity, entry["zero_emissive"].AsBool());
        }
    }
    if (!cratesOk) {
        error = path + ": non_teleportable needs callbacks, each an entity named once without a dot and its "
                       "zero_emissive, a datum, a stride_ms above 0, and colour and emissive, each with a from and "
                       "a to of three numbers none below 0";
        return false;
    }
    crates.datum = crateEntry["datum"].AsString("");
    crates.colour.strideMs = crateEntry["stride_ms"].AsNumber();
    crates.emissive.strideMs = crates.colour.strideMs;
    out = std::move(read);
    return true;
}

// ---- an entity the script adds, read out of its .ent ----------------------------

namespace {

// One element of an .ent, as TinyXML holds it: its name, its attributes in the
// order written, the text directly inside it, and its child elements. Enough XML
// for the files Ethanon's editor writes - a declaration, elements, double-quoted
// attributes, text and CDATA - and anything else is refused rather than guessed at.
struct Element {
    std::string name;
    std::vector<std::pair<std::string, std::string>> attributes;
    std::string text;
    std::vector<Element> children;

    // TiXmlNode::FirstChild(name): the first DIRECT child of that name, so a
    // <SpriteCut> or <Position> inside a particle system or a light is never the
    // entity's own.
    const Element* Child(const std::string& childName) const {
        for (const Element& child : children) {
            if (child.name == childName) return &child;
        }
        return nullptr;
    }
    const std::string* Attribute(const std::string& attributeName) const {
        for (const auto& [key, value] : attributes) {
            if (key == attributeName) return &value;
        }
        return nullptr;
    }
};

class XmlReader {
public:
    explicit XmlReader(const std::string& text) : m_text(text) {}

    // The document's one root element, after its declaration and any comment.
    bool Document(Element& root, std::string& why) {
        SkipMisc();
        if (!Parse(root, why, 0)) return false;
        SkipMisc();
        if (m_at != m_text.size()) {
            why = "text after the root element";
            return false;
        }
        return true;
    }

private:
    static bool NameChar(char c) {
        return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_' || c == '-' || c == '.' || c == ':';
    }

    void SkipSpace() {
        while (m_at < m_text.size() && std::isspace(static_cast<unsigned char>(m_text[m_at])) != 0) ++m_at;
    }

    bool StartsWith(const char* what) const { return m_text.compare(m_at, std::strlen(what), what) == 0; }

    // Space, the <?xml ... ?> declaration and <!-- comments -->.
    void SkipMisc() {
        while (true) {
            SkipSpace();
            const char* close = StartsWith("<?") ? "?>" : StartsWith("<!--") ? "-->" : nullptr;
            if (close == nullptr) return;
            const std::size_t end = m_text.find(close, m_at);
            m_at = end == std::string::npos ? m_text.size() : end + std::strlen(close);
        }
    }

    bool Name(std::string& out) {
        const std::size_t from = m_at;
        while (m_at < m_text.size() && NameChar(m_text[m_at])) ++m_at;
        out = m_text.substr(from, m_at - from);
        return !out.empty();
    }

    bool Parse(Element& out, std::string& why, int depth) {
        if (depth > 32) {
            why = "elements nested deeper than any .ent";
            return false;
        }
        if (m_at >= m_text.size() || m_text[m_at] != '<') {
            why = "an element was expected";
            return false;
        }
        ++m_at;
        if (!Name(out.name)) {
            why = "an element without a name";
            return false;
        }
        while (true) {
            SkipSpace();
            if (StartsWith("/>")) {
                m_at += 2;
                return true;
            }
            if (StartsWith(">")) {
                ++m_at;
                break;
            }
            std::string key;
            if (!Name(key)) {
                why = "<" + out.name + "> has an attribute without a name";
                return false;
            }
            SkipSpace();
            if (!StartsWith("=\"")) {
                why = "<" + out.name + ">'s " + key + " is not written =\"...\"";
                return false;
            }
            m_at += 2;
            const std::size_t end = m_text.find('"', m_at);
            if (end == std::string::npos) {
                why = "<" + out.name + ">'s " + key + " is never closed";
                return false;
            }
            out.attributes.emplace_back(key, m_text.substr(m_at, end - m_at));
            m_at = end + 1;
        }
        // Its content: text, comments and child elements, to its own end tag.
        while (true) {
            const std::size_t open = m_text.find('<', m_at);
            if (open == std::string::npos) {
                why = "<" + out.name + "> is never closed";
                return false;
            }
            out.text += m_text.substr(m_at, open - m_at);
            m_at = open;
            if (StartsWith("<!--")) {
                SkipMisc();
                continue;
            }
            // A CDATA section is text, taken as written: light_wall.ent and
            // minion.ent write their collision polygons in one.
            if (StartsWith("<![CDATA[")) {
                const std::size_t from = m_at + 9;
                const std::size_t end = m_text.find("]]>", from);
                if (end == std::string::npos) {
                    why = "<" + out.name + ">'s CDATA is never closed";
                    return false;
                }
                out.text += m_text.substr(from, end - from);
                m_at = end + 3;
                continue;
            }
            if (StartsWith("</")) {
                m_at += 2;
                std::string closing;
                if (!Name(closing) || closing != out.name) {
                    why = "<" + out.name + "> is closed by </" + closing + ">";
                    return false;
                }
                SkipSpace();
                if (!StartsWith(">")) {
                    why = "</" + closing + " is never finished";
                    return false;
                }
                ++m_at;
                return true;
            }
            out.children.emplace_back();
            if (!Parse(out.children.back(), why, depth + 1)) return false;
        }
    }

    const std::string& m_text;
    std::size_t m_at = 0;
};

std::string Trimmed(const std::string& text) {
    std::size_t from = 0;
    std::size_t to = text.size();
    while (from < to && std::isspace(static_cast<unsigned char>(text[from])) != 0) ++from;
    while (to > from && std::isspace(static_cast<unsigned char>(text[to - 1])) != 0) --to;
    return text.substr(from, to - from);
}

// An attribute read as TinyXML reads one - QueryFloatAttribute and, for the
// engine's ints and booleans, QueryIntAttribute, whose %d takes the whole part of
// "10.000000" - leaving `into` as it is when the attribute is absent. A stated
// value that is not a number is refused: the engine would keep its default in
// silence, and no file of the original's states one.
bool FloatAttribute(const Element& element, const char* name, double& into, std::string& why) {
    const std::string* value = element.Attribute(name);
    if (value == nullptr) return true;
    try {
        std::size_t used = 0;
        const double parsed = std::stod(*value, &used);
        if (used == 0 || !std::isfinite(parsed)) throw std::invalid_argument(*value);
        into = parsed;
        return true;
    } catch (const std::exception&) {
        why = "<" + element.name + ">'s " + name + " is not a number: '" + *value + "'";
        return false;
    }
}

bool IntAttribute(const Element& element, const char* name, int& into, std::string& why) {
    const std::string* value = element.Attribute(name);
    if (value == nullptr) return true;
    try {
        std::size_t used = 0;
        const int parsed = std::stoi(*value, &used);
        if (used == 0) throw std::invalid_argument(*value);
        into = parsed;
        return true;
    } catch (const std::exception&) {
        why = "<" + element.name + ">'s " + name + " is not a whole number: '" + *value + "'";
        return false;
    }
}

// ReadVector2PropertyFromXmlElement and its kin (ETHEntityProperties.cpp:606-674):
// the first child of that name, each of its components read where stated.
bool ChildFloats(const Element& parent, const char* name, const char* const* keys, double* into, std::size_t count,
                 std::string& why) {
    const Element* child = parent.Child(name);
    if (child == nullptr) return true;
    for (std::size_t i = 0; i < count; ++i) {
        if (!FloatAttribute(*child, keys[i], into[i], why)) return false;
    }
    return true;
}

// The text inside the first child of that name, or `into` as it is.
void ChildText(const Element& parent, const char* name, std::string& into) {
    if (const Element* child = parent.Child(name)) into = Trimmed(child->text);
}

bool ReadEntLight(const Element& entity, std::optional<Lighting::Light>& out, std::string& why) {
    out.reset();
    const Element* element = entity.Child("Light");
    if (element == nullptr) return true;
    // ETHLight(true) (ETHLight.cpp:27-38), then its ReadFromXMLFile (:40-63).
    int active = 1;
    double range = 256.0;
    double haloBrightness = 1.0;
    double haloSize = 64.0;
    double position[3] = {0.0, 0.0, 0.0};
    double colour[3] = {1.0, 1.0, 1.0};
    std::string halo;
    static const char* const kXyz[] = {"x", "y", "z"};
    static const char* const kRgb[] = {"r", "g", "b"};
    if (!IntAttribute(*element, "active", active, why) || !FloatAttribute(*element, "range", range, why) ||
        !FloatAttribute(*element, "haloBrightness", haloBrightness, why) ||
        !FloatAttribute(*element, "haloSize", haloSize, why) ||
        !ChildFloats(*element, "Position", kXyz, position, 3, why) ||
        !ChildFloats(*element, "Color", kRgb, colour, 3, why)) {
        return false;
    }
    ChildText(*element, "HaloBitmap", halo);
    // Kept only active and with a range (ETHEntityProperties.cpp:343).
    if (active == 0 || !(range > 0.0)) return true;
    Lighting::Light light;
    light.range = range;
    light.offset = glm::dvec3(position[0], position[1], position[2]);
    light.colour = glm::dvec3(colour[0], colour[1], colour[2]);
    light.halo = halo;
    // The halo is drawn at the light's position, unscaled (ETHRenderEntity.cpp:376-386),
    // haloSize on both axes: the converter's mapping (tscn.py:313-333).
    light.haloOffset = glm::dvec2(position[0], position[1]);
    light.haloSize = glm::dvec2(haloSize, haloSize);
    light.haloBrightness = haloBrightness;
    out = light;
    return true;
}

} // namespace

bool Entity::Temporary() const {
    // Every system the reader keeps holds particles (Particles::Load refuses one
    // that does not), so each is one IsTemporary counts as existing.
    if (systems.empty() || !sprite.empty()) return false;
    return std::all_of(systems.begin(), systems.end(),
                       [](const Particles::System& system) { return system.repeat > 0; });
}

double Entity::LayerableDepth() const {
    // ETHSpriteEntity::m_layrableMinimumDepth (ETHSpriteEntity.cpp:34): 0.001, so a
    // layerable entity is never at the very back of the range.
    constexpr double kLayerableMinimumDepth = 0.001;
    return std::max(kLayerableMinimumDepth, layerDepth);
}

double Entity::ParticleScale(double scriptScale) const {
    const glm::dvec2 made = ScaleAt(scriptScale);
    return (made.x + made.y) / 2.0;
}

bool LoadEnt(const std::string& path, Entity& out, std::string& error) {
    std::string text;
    if (!Particles::ReadEnt(path, text, error)) return false;
    Element root;
    std::string why;
    XmlReader reader(text);
    if (!reader.Document(root, why)) {
        error = path + ": " + why;
        return false;
    }
    // hDoc.FirstChildElement() is the root, and its first child element the
    // entity (ETHEntityProperties.cpp:154-164). Every .ent names them so.
    if (root.name != "Ethanon" || root.children.empty() || root.children.front().name != "Entity") {
        error = path + ": is not <Ethanon><Entity>";
        return false;
    }
    const Element& entity = root.children.front();

    Entity read;
    int type = static_cast<int>(EntityType::Horizontal);
    int isStatic = 0;
    int applyLight = 0;
    double emissive[4] = {0.0, 0.0, 0.0, 0.0};
    double cut[2] = {1.0, 1.0};
    double scale[2] = {1.0, 1.0};
    double pivot[2] = {0.0, 0.0};
    static const char* const kRgba[] = {"r", "g", "b", "a"};
    static const char* const kXy[] = {"x", "y"};
    if (!IntAttribute(entity, "type", type, why) || !FloatAttribute(entity, "layerDepth", read.layerDepth, why) ||
        !IntAttribute(entity, "static", isStatic, why) || !IntAttribute(entity, "applyLight", applyLight, why) ||
        !IntAttribute(entity, "blendMode", read.blendMode, why) ||
        !ChildFloats(entity, "EmissiveColor", kRgba, emissive, 4, why) ||
        !ChildFloats(entity, "SpriteCut", kXy, cut, 2, why) || !ChildFloats(entity, "Scale", kXy, scale, 2, why) ||
        !ChildFloats(entity, "PivotAdjust", kXy, pivot, 2, why) || !ReadEntLight(entity, read.light, why)) {
        error = path + ": " + why;
        return false;
    }
    // QueryIntAttribute stores any whole number as the type (:228), and nothing the
    // engine does with one outside ENTITY_TYPE is what the file meant: ComputeDepth
    // has no case for it and gives depth 0 (ETHSpriteEntity.cpp:611-632), and
    // ConvertToGSSO gives it EO_DEFAULT where every named type has a centre
    // (ETHEntity.cpp:27-44).
    if (type < static_cast<int>(EntityType::Horizontal) || type > static_cast<int>(EntityType::Layerable)) {
        error = path + ": its type " + std::to_string(type) + " is not an ENTITY_TYPE (ETHEntityProperties.h:82-89)";
        return false;
    }
    read.type = static_cast<EntityType>(type);
    // A cut is read as ints (ReadVector2iPropertyFromXmlElement, :623-638).
    read.columns = static_cast<int>(cut[0]);
    read.rows = static_cast<int>(cut[1]);
    if (read.columns < 1 || read.rows < 1) {
        error = path + ": its SpriteCut is not a grid";
        return false;
    }
    read.isStatic = isStatic != 0;
    read.applyLight = applyLight != 0;
    read.additive = read.blendMode == Particles::kAlphaAdd;
    read.emissive = glm::dvec3(emissive[0], emissive[1], emissive[2]);
    read.scale = glm::dvec2(scale[0], scale[1]);
    read.pivotPx = glm::dvec2(pivot[0], pivot[1]);
    ChildText(entity, "Sprite", read.sprite);
    ChildText(entity, "Normal", read.normal);

    if (!Particles::Parse(text, path, read.systems, error)) return false;
    if (read.systems.size() > 2) {
        error = path + ": carries " + std::to_string(read.systems.size()) +
                " particle systems, and Ethanon reads two (ETH_MAX_PARTICLE_SYS_PER_ENTITY)";
        return false;
    }
    out = std::move(read);
    return true;
}

} // namespace MagicPortals::Art

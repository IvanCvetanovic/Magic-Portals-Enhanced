#include "sim/Puzzle.hpp"

#include "core/Components.hpp"
#include "core/Json.hpp"
#include "sim/Units.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

namespace MagicPortals::Puzzle {

namespace {

// A channel number off metadata. The remake reads a missing one as -1, a channel
// nothing else is on; the port refuses it rather than invent one.
bool ChannelOf(const Tscn::Node& node, const char* key, int& out, std::string& error) {
    const Tscn::Value* value = node.Meta(key);
    double number = 0.0;
    if (value == nullptr || !value->AsNumber(number)) {
        error = node.name + " has no " + key;
        return false;
    }
    out = static_cast<int>(number);
    return true;
}

// GetUInt('stride'), which computeButtonInterpolator passes the interpolator as
// its milliseconds. All 46 placements and button.ent carry 300 (D10 section 2).
bool StrideOf(const Tscn::Node& node, double& out, std::string& error) {
    const Tscn::Value* value = node.Meta("stride");
    if (value == nullptr || !value->AsNumber(out)) {
        error = node.name + " has no stride";
        return false;
    }
    return true;
}

// Where the level puts a node, in the remake's pixels.
glm::dvec2 PositionOf(const Tscn::Node& node) {
    const Tscn::Value* value = node.Find("position");
    if (value == nullptr || value->kind != Tscn::Value::Kind::Vector2) return glm::dvec2(0.0);
    return glm::dvec2(value->numbers[0], value->numbers[1]);
}

// The sink applied: the trigger box and the built body both, since the original
// moves one point (R21). +y is down in the remake's pixels and up in the engine's
// metres, so a sink subtracts.
void Place(entt::registry& registry, Button& button) {
    const float depth = Units::ToMetres(button.sunkPx);
    button.box.centre = button.restCentre - glm::vec2(0.0f, depth);
    if (button.entity == entt::null || !registry.valid(button.entity)) return;
    auto* transform = registry.try_get<Supersonic::TransformComponent>(button.entity);
    if (transform == nullptr) return;
    transform->position = button.restPosition - glm::vec3(0.0f, depth, 0.0f);
}

} // namespace

double Button::DepthPx() const {
    // getCurrentPos returns m_b once m_elapsedTime is past m_time (20104 #7-17),
    // and getBias clamps the fraction before the filter (18855 #9-31).
    if (strideMs <= 0.0 || slideMs > strideMs) return slideToPx;
    const double bias = std::sin(easeRad * std::clamp(slideMs / strideMs, 0.0, 1.0));
    return slideFromPx + (slideToPx - slideFromPx) * bias;
}

void Button::Rebase(entt::registry& registry, const glm::vec2& centre) {
    // The body keeps whatever offset the trigger has from it (no button placement
    // has one) and its own z. The original's rebase is a vector3 SetPosition and
    // moves z too - level31's button_dest is at z -4 against the button's -14 -
    // but the port's bodies are all at z 0 (LevelBuilder::Place) and its picture
    // is drawn at the slot the sprite was built in, so that move shows nowhere.
    const glm::vec2 offset = glm::vec2(restPosition.x, restPosition.y) - restCentre;
    restCentre = centre;
    restPosition = glm::vec3(centre.x + offset.x, centre.y + offset.y, restPosition.z);
    pressed = false;
    slideFromPx = 0.0;
    slideToPx = 0.0;
    slideMs = 0.0;
    sunkPx = 0.0;
    Place(registry, *this);
}

void Channels::Tick(entt::registry& registry, float dt) {
    for (Button& button : buttons) {
        const bool pressed = Trigger::DynamicBodiesIn(registry, button.box) > 0;
        if (pressed != button.pressed) {
            button.pressed = pressed;
            // interpolator.reset(originalPos, pressedPos, stride) on the first
            // qualifying contact, and the mirror of it when the count comes back
            // to 0 (D10 sections 1.3 and 1.4). Reset reads the stored points, not
            // where the button is, so a reversal mid-slide snaps to the endpoint
            // it starts from.
            button.slideFromPx = pressed ? 0.0 : button.sinkPx;
            button.slideToPx = pressed ? button.sinkPx : 0.0;
            button.slideMs = 0.0;
            for (SwitchedDoor& door : doors) {
                if (door.channel == button.channel) door.motion.opening = pressed;
            }
        }
        // ETHCallback_button every frame: interpolator.update() and then
        // SetPositionXY(getCurrentPos()). The contact callback zeroed the clock
        // inside the physics step and this runs later in the same Update
        // (ETHScene.cpp:464, :488), so the first pressed frame is already drawn
        // one frame's ease down, not at 0 (00_order 8.0 rule 6).
        button.slideMs += static_cast<double>(dt) * 1000.0;
        button.sunkPx = button.DepthPx();
        Place(registry, button);
    }
    for (SwitchedDoor& door : doors) door.motion.Tick(registry, door.entity, dt);
}

bool Channels::Pressed(int channel) const {
    for (const Button& button : buttons) {
        if (button.channel == channel && button.pressed) return true;
    }
    return false;
}

const Button* Channels::FindButton(const std::string& name) const {
    for (const Button& button : buttons) {
        if (button.name == name) return &button;
    }
    return nullptr;
}

const SwitchedDoor* Channels::FindDoor(const std::string& name) const {
    for (const SwitchedDoor& door : doors) {
        if (door.name == name) return &door;
    }
    return nullptr;
}

bool LoadRules(const std::string& path, Rules& out, std::string& error) {
    namespace Json = Supersonic::Json;
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
    const auto number = [&](const char* key, double& to) {
        if (!root.IsObject() || !root.Has("button") || !root["button"].IsObject() || !root["button"].Has(key) ||
            !root["button"][key].IsNumber()) {
            error = path + ": button." + std::string(key) + " is missing or not a number";
            return false;
        }
        to = root["button"][key].AsNumber();
        return true;
    };
    Rules read;
    if (!number("sink_units", read.sinkPx) || !number("ease_rad", read.easeRad)) return false;
    out = read;
    return true;
}

bool Wire(const Tscn::Scene& scene, const Roles::Table& roles, const LevelBuilder::Built& built, const Rules& rules,
          Channels& out, std::string& error) {
    out = Channels{};
    for (const Tscn::Node& node : scene.nodes) {
        if (node.parent != ".") continue;
        const std::string role = Roles::RoleOf(roles, node);
        if (role == Roles::kSwitch) {
            Button button;
            button.name = node.name;
            if (!ChannelOf(node, "idx", button.channel, error)) return false;
            if (!Trigger::FromNode(node, button.box, error)) return false;
            if (!StrideOf(node, button.strideMs, error)) return false;
            button.sinkPx = rules.sinkPx;
            button.easeRad = rules.easeRad;
            button.restCentre = button.box.centre;
            const glm::dvec2 at = PositionOf(node);
            button.restPosition = Units::ToWorld(at.x, at.y);
            // Its own body, which LevelBuilder places from the same node: the
            // sensor the sink moves, and what the layer hangs the picture on. A
            // node with no Body is not built and the trigger sinks alone.
            if (const auto found = built.entities.find(node.name); found != built.entities.end()) {
                button.entity = found->second;
            }
            out.buttons.push_back(button);
        } else if (role == Roles::kSwitchedDoor) {
            SwitchedDoor door;
            door.name = node.name;
            if (!ChannelOf(node, "switchIdx", door.channel, error)) return false;
            if (!Mover::DoorFromNode(node, door.motion, error)) return false;
            const auto found = built.entities.find(node.name);
            if (found == built.entities.end()) {
                error = node.name + " was not built";
                return false;
            }
            door.entity = found->second;
            out.doors.push_back(door);
        }
    }
    return true;
}

} // namespace MagicPortals::Puzzle

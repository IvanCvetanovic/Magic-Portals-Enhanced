#include "sim/ContactOrder.hpp"

#include "core/Components.hpp"
#include "core/Json.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <system_error>

namespace MagicPortals::ContactOrder {

namespace Json = Supersonic::Json;
using Supersonic::RigidBodyComponent;

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
    if (!root["levels"].IsObject()) {
        error = path + ": no \"levels\" object";
        return false;
    }
    Rules read;
    for (const auto& [level, entry] : root["levels"].AsObject()) {
        if (!entry.IsArray()) {
            error = path + ": " + level + " is not a list of nodes' names";
            return false;
        }
        if (entry.AsArray().empty()) {
            error = path + ": " + level + " lists no node (an empty list; leave the level out instead)";
            return false;
        }
        std::vector<std::string>& names = read.byLevel[level];
        for (const Json::Value& name : entry.AsArray()) {
            if (!name.IsString() || name.AsString().empty()) {
                error = path + ": " + level + ": a body is a node's name, a non-empty string";
                return false;
            }
            if (std::find(names.begin(), names.end(), name.AsString()) != names.end()) {
                error = path + ": " + level + " lists " + name.AsString() + " twice";
                return false;
            }
            names.push_back(name.AsString());
        }
    }
    out = std::move(read);
    return true;
}

bool Apply(entt::registry& registry, const Rules& rules, const std::string& level,
           const std::unordered_map<std::string, entt::entity>& built, std::string& error) {
    const auto listed = rules.byLevel.find(level);
    if (listed == rules.byLevel.end()) return true;
    for (const std::string& name : listed->second) {
        const std::string head = "contact_order.json: " + level + " lists " + name + ", which ";
        const auto found = built.find(name);
        if (found == built.end() || !registry.valid(found->second)) {
            error = head + "the level does not build";
            return false;
        }
        auto* rigid = registry.try_get<RigidBodyComponent>(found->second);
        if (rigid == nullptr) {
            error = head + "the level builds as a static body or an area, with no rigid body to step";
            return false;
        }
        // A kinematic body is moved by the port, not by the solver, so there is no step order to give it.
        if (rigid->isKinematic) {
            error = head + "the level builds as a kinematic mover, which the port moves and the solver has no order for";
            return false;
        }
        rigid->integrateAfterSolve = true;
    }
    return true;
}

bool CheckLevels(const Rules& rules, const std::filesystem::path& levelsDirectory, std::string& error) {
    std::vector<std::string> keys;
    keys.reserve(rules.byLevel.size());
    for (const auto& [level, names] : rules.byLevel) keys.push_back(level);
    std::sort(keys.begin(), keys.end());
    for (const std::string& level : keys) {
        std::error_code ec;
        if (!std::filesystem::is_regular_file(levelsDirectory / (level + ".tscn"), ec)) {
            error = "contact_order.json: " + level + " is not a level in " + levelsDirectory.string();
            return false;
        }
    }
    return true;
}

} // namespace MagicPortals::ContactOrder

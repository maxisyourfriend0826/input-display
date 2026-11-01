#include "idisp/rules.hpp"

#include <nlohmann/json.hpp>
#include <stdexcept>

namespace idisp {

using nlohmann::json;

static void readRange(const json& j, const char* key, float& lo, float& hi, const std::string& where) {
    if (!j.contains(key) || !j[key].is_array() || j[key].size() != 2)
        throw std::runtime_error(where + ": \"" + key + "\" must be [min, max]");
    lo = j[key][0].get<float>();
    hi = j[key][1].get<float>();
    if (lo > hi) throw std::runtime_error(where + ": \"" + key + "\" min is greater than max");
}

Profile parseProfile(const std::string& text) {
    json j;
    try {
        j = json::parse(text);
    } catch (const json::exception& e) {
        throw std::runtime_error(std::string("invalid JSON: ") + e.what());
    }
    Profile p;
    p.name = j.value("name", "Unnamed profile");
    if (!j.contains("rules") || !j["rules"].is_array())
        throw std::runtime_error("profile needs a \"rules\" array");
    for (const auto& jr : j["rules"]) {
        Rule r;
        r.name = jr.value("name", "Rule");
        r.label = jr.value("label", r.name.substr(0, 1));
        if (!jr.contains("regions") || !jr["regions"].is_array())
            throw std::runtime_error("rule \"" + r.name + "\" needs a \"regions\" array");
        for (const auto& jg : jr["regions"]) {
            Region g;
            g.name = jg.value("name", "");
            const std::string where = "rule \"" + r.name + "\" region \"" + g.name + "\"";
            if (jg.contains("r")) {
                g.ring = true;
                readRange(jg, "r", g.rMin, g.rMax, where);
            } else {
                readRange(jg, "x", g.xMin, g.xMax, where);
                readRange(jg, "y", g.yMin, g.yMax, where);
                g.truncate = jg.value("truncate", false);
            }
            r.regions.push_back(g);
        }
        p.rules.push_back(r);
    }
    return p;
}

std::string serializeProfile(const Profile& p) {
    json j;
    j["name"] = p.name;
    j["rules"] = json::array();
    for (const auto& r : p.rules) {
        json jr{ { "name", r.name }, { "label", r.label }, { "regions", json::array() } };
        for (const auto& g : r.regions) {
            json jg{ { "name", g.name } };
            if (g.ring) {
                jg["r"] = { g.rMin, g.rMax };
            } else {
                jg["x"] = { g.xMin, g.xMax };
                jg["y"] = { g.yMin, g.yMax };
                if (g.truncate) jg["truncate"] = true;
            }
            jr["regions"].push_back(jg);
        }
        j["rules"].push_back(jr);
    }
    return j.dump(2) + "\n";
}

void RuleMonitor::setProfile(const Profile& p) {
    profile_ = p;
    state_.assign(p.rules.size(), State{});
}

void RuleMonitor::onSample(uint64_t timestampNs, StickUnits u) {
    for (size_t i = 0; i < profile_.rules.size(); ++i) {
        State& s = state_[i];
        s.now = profile_.rules[i].hits(u);
        if (s.now) {
            s.sinceFrame = true;
            s.lastHitNs = timestampNs;
            ++s.hits;
        }
    }
}

bool RuleMonitor::activeForFrame(size_t rule, uint64_t nowNs, int holdMs) {
    if (rule >= state_.size()) return false;
    State& s = state_[rule];
    bool active = s.now || s.sinceFrame;
    if (!active && holdMs > 0 && s.hits > 0)
        active = nowNs - s.lastHitNs <= static_cast<uint64_t>(holdMs) * 1'000'000ull;
    s.sinceFrame = false;
    return active;
}

} // namespace idisp

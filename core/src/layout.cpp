#include "idisp/layout.hpp"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <stdexcept>

namespace idisp {

Rect Layout::coordsRect() const {
    if (coords.w > 0) return coords;
    return Rect{ ringCenterX - 104, 278, kCoordsW, kCoordsH };
}

bool safeImageFile(const std::string& n) {
    return !n.empty() && n.size() < 200 && n.find_first_of("/\\:") == std::string::npos && n.find("..") == std::string::npos;
}

const LayoutImage* Layout::image(const std::string& id) const {
    for (const auto& im : images)
        if (im.id == id) return &im;
    return nullptr;
}

std::vector<std::string> Layout::drawOrder() const {
    std::vector<std::string> all;
    for (const auto& im : images) all.push_back(im.id);
    for (const char* b : kButtonNames) all.push_back(b);
    for (const char* id : { kStickR, kStick, kCoords }) all.push_back(id);
    auto known = [&](const std::string& id) { return std::find(all.begin(), all.end(), id) != all.end(); };
    std::vector<std::string> out;
    for (const auto& id : order)
        if (known(id) && std::find(out.begin(), out.end(), id) == out.end()) out.push_back(id);
    std::vector<std::string> bottom;
    for (const auto& id : all) {
        if (std::find(out.begin(), out.end(), id) != out.end()) continue;
        if (isImageId(id)) bottom.push_back(id);
        else out.push_back(id);
    }
    out.insert(out.begin(), bottom.begin(), bottom.end());
    return out;
}

static Rect readRect(const nlohmann::json& r, const std::string& what) {
    if (!r.is_array() || r.size() != 4) throw std::runtime_error(what + " must be [x, y, w, h]");
    return Rect{ r[0].get<int>(), r[1].get<int>(), r[2].get<int>(), r[3].get<int>() };
}

LayoutFile parseLayouts(const std::string& text, bool allowEmpty, std::vector<std::string>* skipped) {
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(text);
    } catch (const nlohmann::json::exception& e) {
        throw std::runtime_error(std::string("invalid JSON: ") + e.what());
    }
    LayoutFile f;
    try {
        f.width = j.value("width", 420);
        f.height = j.value("height", 300);
        const bool hasArray = j.contains("layouts") && j["layouts"].is_array();
        if (!hasArray || (j["layouts"].empty() && !allowEmpty))
            throw std::runtime_error("the file needs a non-empty \"layouts\" array");
        for (const auto& jl : j["layouts"]) {
          try {
            Layout l;
            l.name = jl.value("name", "Layout");
            const std::string where = "layout \"" + l.name + "\"";
            l.width = std::clamp(jl.value("width", f.width), 32, 4096);
            l.height = std::clamp(jl.value("height", f.height), 32, 4096);
            if (jl.contains("ringCenter")) {
                l.ringCenterX = jl["ringCenter"].at(0).get<int>();
                l.ringCenterY = jl["ringCenter"].at(1).get<int>();
            }
            l.stickScale = std::clamp(jl.value("stickScale", 1.0f), 0.1f, 8.0f);
            auto readOpts = [](const nlohmann::json& j, StickOptions& o) {
                o.shape = std::clamp(j.value("shape", -1), -1, kShapeCount - 1);
                o.light = j.value("light", true);
            };
            if (jl.contains("leftStick")) readOpts(jl["leftStick"], l.sticks[0]);
            if (jl.contains("rightStick")) {
                const auto& r = jl["rightStick"];
                if (r.contains("center")) {
                    l.rightX = r["center"].at(0).get<int>();
                    l.rightY = r["center"].at(1).get<int>();
                }
                l.rightScale = std::clamp(r.value("scale", 0.5f), 0.1f, 8.0f);
                readOpts(r, l.sticks[1]);
            } else {
                l.hidden.insert(kStickR);  // layouts from before the right stick
            }
            if (jl.contains("images"))
                for (const auto& ji : jl["images"]) {
                    LayoutImage im;
                    im.id = ji.at("id").get<std::string>();
                    im.file = ji.at("file").get<std::string>();
                    if (!isImageId(im.id) || l.image(im.id)) throw std::runtime_error(where + ": bad or repeated image id");
                    if (!safeImageFile(im.file)) throw std::runtime_error(where + ": image file must be a plain file name");
                    im.rect = readRect(ji.at("rect"), where + " image");
                    l.images.push_back(im);
                }
            if (jl.contains("order"))
                for (const auto& o : jl["order"]) l.order.push_back(o.get<std::string>());
            if (jl.contains("coords")) l.coords = readRect(jl["coords"], where + " coords");
            if (jl.contains("hidden"))
                for (const auto& h : jl["hidden"]) l.hidden.insert(h.get<std::string>());
            for (const char* name : kButtonNames) {
                if (!jl.contains("buttons") || !jl["buttons"].contains(name)) continue;
                l.buttons[name] = readRect(jl["buttons"][name], where + " button " + name);
            }
            f.layouts.push_back(l);
          } catch (const nlohmann::json::exception& e) {
            if (!skipped) throw std::runtime_error(std::string("bad layout value: ") + e.what());
            skipped->push_back(std::string("bad layout value: ") + e.what());
          } catch (const std::runtime_error& e) {
            if (!skipped) throw;
            skipped->push_back(e.what());
          }
        }
    } catch (const nlohmann::json::exception& e) {
        throw std::runtime_error(std::string("bad layout value: ") + e.what());
    }
    return f;
}

std::string serializeLayouts(const std::vector<Layout>& layouts) {
    auto rect = [](const Rect& r) { return nlohmann::json::array({ r.x, r.y, r.w, r.h }); };
    nlohmann::ordered_json out;
    out["layouts"] = nlohmann::ordered_json::array();
    for (const Layout& l : layouts) {
        nlohmann::ordered_json jl;
        jl["name"] = l.name;
        jl["width"] = l.width;
        jl["height"] = l.height;
        jl["ringCenter"] = { l.ringCenterX, l.ringCenterY };
        if (l.stickScale != 1.0f) jl["stickScale"] = l.stickScale;
        auto opts = [](nlohmann::ordered_json& j, const StickOptions& o) {
            j["shape"] = o.shape;
            j["light"] = o.light;
        };
        if (!(l.sticks[0] == StickOptions{})) {
            nlohmann::ordered_json ls;
            opts(ls, l.sticks[0]);
            jl["leftStick"] = ls;
        }
        nlohmann::ordered_json rs;
        rs["center"] = { l.rightX, l.rightY };
        rs["scale"] = l.rightScale;
        opts(rs, l.sticks[1]);
        jl["rightStick"] = rs;
        if (l.coords.w > 0) jl["coords"] = rect(l.coords);
        if (!l.hidden.empty()) jl["hidden"] = l.hidden;
        nlohmann::ordered_json b = nlohmann::ordered_json::object();
        for (const char* name : kButtonNames) {
            auto it = l.buttons.find(name);
            if (it != l.buttons.end()) b[name] = rect(it->second);
        }
        jl["buttons"] = b;
        if (!l.images.empty()) {
            jl["images"] = nlohmann::ordered_json::array();
            for (const auto& im : l.images)
                jl["images"].push_back({ { "id", im.id }, { "file", im.file }, { "rect", rect(im.rect) } });
        }
        if (!l.order.empty()) jl["order"] = l.order;
        out["layouts"].push_back(jl);
    }
    return out.dump(2) + "\n";
}

} // namespace idisp

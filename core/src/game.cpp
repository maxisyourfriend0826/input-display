#include "idisp/game.hpp"

#include <nlohmann/json.hpp>

#include <cctype>
#include <cstring>
#include <stdexcept>

namespace idisp {

using nlohmann::json;

namespace {

[[noreturn]] void fail(const std::string& msg) { throw std::runtime_error(msg); }

// "0x1A", "1A", "-0x10" (CE offsets are hex even without 0x).
int64_t parseHex(std::string s, const std::string& context) {
    bool neg = false;
    if (!s.empty() && (s[0] == '-' || s[0] == '+')) {
        neg = s[0] == '-';
        s.erase(0, 1);
    }
    if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s.erase(0, 2);
    if (s.empty() || s.size() > 16) fail("bad hex number in " + context);
    uint64_t v = 0;
    for (char c : s) {
        if (!std::isxdigit((unsigned char)c)) fail("bad hex number in " + context);
        v = v * 16 + (std::isdigit((unsigned char)c) ? c - '0' : std::tolower((unsigned char)c) - 'a' + 10);
    }
    return neg ? -(int64_t)v : (int64_t)v;
}

std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t"), b = s.find_last_not_of(" \t");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

ValueType parseType(const std::string& t) {
    static const std::map<std::string, ValueType> types = {
        { "float", ValueType::F32 }, { "double", ValueType::F64 }, { "i8", ValueType::I8 },
        { "u8", ValueType::U8 }, { "i16", ValueType::I16 }, { "u16", ValueType::U16 },
        { "i32", ValueType::I32 }, { "u32", ValueType::U32 },
    };
    auto it = types.find(t);
    if (it == types.end()) fail("unknown value type \"" + t + "\" (use float, double, i8, u8, i16, u16, i32 or u32)");
    return it->second;
}

size_t typeSize(ValueType t) {
    switch (t) {
    case ValueType::F64: return 8;
    case ValueType::I8: case ValueType::U8: return 1;
    case ValueType::I16: case ValueType::U16: return 2;
    default: return 4;
    }
}

std::optional<uint32_t> optHex32(const json& j, const char* key, const std::string& ctx) {
    if (!j.contains(key)) return std::nullopt;
    if (!j[key].is_string()) fail(ctx + ": \"" + key + "\" must be a hex string like \"0x6543A1B2\"");
    const int64_t v = parseHex(j[key].get<std::string>(), ctx + " " + key);
    if (v < 0 || v > 0xFFFFFFFFll) fail(ctx + ": \"" + key + "\" is out of range");
    return (uint32_t)v;
}

} // namespace

PointerPath parsePointerPath(const std::vector<std::string>& parts) {
    if (parts.empty()) fail("pointer path is empty");
    PointerPath p;
    const std::string first = trim(parts[0]);
    const size_t plus = first.rfind('+');
    if (plus == std::string::npos || plus == 0) fail("pointer path must start with \"Module.exe+offset\", got \"" + first + "\"");
    p.module = trim(first.substr(0, plus));
    const int64_t off = parseHex(trim(first.substr(plus + 1)), "\"" + first + "\"");
    if (off < 0) fail("module offset can't be negative in \"" + first + "\"");
    p.moduleOffset = (uint64_t)off;
    for (size_t i = 1; i < parts.size(); ++i) p.offsets.push_back(parseHex(trim(parts[i]), "offset \"" + parts[i] + "\""));
    return p;
}

GameDef parseGameDef(const std::string& text) {
    json j;
    try {
        j = json::parse(text);
    } catch (const json::exception& e) {
        fail(std::string("not valid JSON: ") + e.what());
    }
    if (!j.is_object()) fail("game definition must be a JSON object");
    GameDef d;
    for (auto [key, out] : { std::pair{ "id", &d.id }, { "name", &d.name }, { "process", &d.process } }) {
        if (!j.contains(key) || !j[key].is_string() || j[key].get<std::string>().empty())
            fail(std::string("missing \"") + key + "\"");
        *out = j[key].get<std::string>();
    }
    if (j.contains("profile") && j["profile"].is_string()) d.profile = j["profile"].get<std::string>();
    if (!j.contains("versions") || !j["versions"].is_array() || j["versions"].empty()) fail("\"versions\" must be a non-empty array");

    for (const auto& jv : j["versions"]) {
        GameVersion v;
        v.label = jv.value("label", std::string("version ") + std::to_string(d.versions.size() + 1));
        const std::string ctx = "version \"" + v.label + "\"";
        if (jv.contains("match")) {
            const json& m = jv["match"];
            if (!m.is_object()) fail(ctx + ": \"match\" must be an object");
            v.timeDateStamp = optHex32(m, "timeDateStamp", ctx);
            v.sizeOfImage = optHex32(m, "sizeOfImage", ctx);
        }
        if (!jv.contains("values") || !jv["values"].is_object()) fail(ctx + ": missing \"values\"");
        for (const auto& [name, jval] : jv["values"].items()) {
            const std::string vctx = ctx + " value \"" + name + "\"";
            if (!jval.contains("path") || !jval["path"].is_array()) fail(vctx + ": \"path\" must be an array of strings");
            std::vector<std::string> parts;
            for (const auto& s : jval["path"]) {
                if (!s.is_string()) fail(vctx + ": \"path\" must be an array of strings");
                parts.push_back(s.get<std::string>());
            }
            GameValue gv;
            try {
                gv.path = parsePointerPath(parts);
                gv.type = parseType(jval.value("type", std::string("float")));
            } catch (const std::exception& e) {
                fail(vctx + ": " + e.what());
            }
            if (jval.contains("scale")) {
                if (!jval["scale"].is_number()) fail(vctx + ": \"scale\" must be a number");
                gv.scale = jval["scale"].get<double>();
            }
            v.values[name] = gv;
        }
        d.versions.push_back(std::move(v));
    }
    return d;
}

VersionMatch matchVersion(const GameDef& def, uint32_t tds, uint32_t soi) {
    for (const auto& v : def.versions)
        if ((v.timeDateStamp || v.sizeOfImage) && (!v.timeDateStamp || *v.timeDateStamp == tds) &&
            (!v.sizeOfImage || *v.sizeOfImage == soi))
            return { &v, true };
    for (const auto& v : def.versions)
        if (!v.timeDateStamp && !v.sizeOfImage) return { &v, false };
    return {};
}

std::optional<uint64_t> resolveAddress(const PointerPath& p, const ModuleBase& base, const ReadMemory& read,
                                       bool pointers64) {
    const auto b = base(p.module);
    if (!b) return std::nullopt;
    uint64_t addr = *b + p.moduleOffset;
    for (int64_t off : p.offsets) {
        uint64_t ptr = 0;
        if (pointers64) {
            if (!read(addr, &ptr, 8)) return std::nullopt;
        } else {
            uint32_t p32 = 0;
            if (!read(addr, &p32, 4)) return std::nullopt;
            ptr = p32;
        }
        if (ptr == 0) return std::nullopt;  // pointer not set up yet (e.g. in a menu)
        addr = ptr + (uint64_t)off;
    }
    return addr;
}

std::optional<double> readGameValue(const GameValue& v, const ModuleBase& base, const ReadMemory& read,
                                    bool pointers64) {
    const auto addr = resolveAddress(v.path, base, read, pointers64);
    if (!addr) return std::nullopt;
    unsigned char buf[8] = {};
    if (!read(*addr, buf, typeSize(v.type))) return std::nullopt;
    double x = 0;
    switch (v.type) {
    case ValueType::F32: { float f; std::memcpy(&f, buf, 4); x = f; break; }
    case ValueType::F64: { std::memcpy(&x, buf, 8); break; }
    case ValueType::I8: x = (int8_t)buf[0]; break;
    case ValueType::U8: x = buf[0]; break;
    case ValueType::I16: { int16_t i; std::memcpy(&i, buf, 2); x = i; break; }
    case ValueType::U16: { uint16_t i; std::memcpy(&i, buf, 2); x = i; break; }
    case ValueType::I32: { int32_t i; std::memcpy(&i, buf, 4); x = i; break; }
    case ValueType::U32: { uint32_t i; std::memcpy(&i, buf, 4); x = i; break; }
    }
    return x * v.scale;
}

} // namespace idisp

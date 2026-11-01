#include "game_memory.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <tlhelp32.h>
#endif

using namespace idisp;

namespace {

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

std::string hex32(uint32_t v) {
    char b[16];
    std::snprintf(b, sizeof b, "0x%08X", v);
    return b;
}

#ifdef _WIN32
std::string narrow(const wchar_t* w) {
    char buf[MAX_PATH * 3];
    const int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, buf, sizeof buf, nullptr, nullptr);
    return n > 0 ? std::string(buf) : std::string();
}
#endif

} // namespace

struct GameMemory::Impl {
#ifdef _WIN32
    HANDLE process = nullptr;
#endif
    std::map<std::string, uint64_t> modules;  // lower-case name -> base
    bool pointers64 = true;
    const GameDef* def = nullptr;
    const GameVersion* version = nullptr;

    bool read(uint64_t addr, void* out, size_t n) const {
#ifdef _WIN32
        SIZE_T got = 0;
        return process && ReadProcessMemory(process, (LPCVOID)(uintptr_t)addr, out, n, &got) && got == n;
#else
        (void)addr; (void)out; (void)n;
        return false;
#endif
    }

    std::optional<uint64_t> moduleBase(const std::string& name) const {
        auto it = modules.find(lower(name));
        if (it == modules.end()) return std::nullopt;
        return it->second;
    }

    void close() {
#ifdef _WIN32
        if (process) CloseHandle(process);
        process = nullptr;
#endif
        modules.clear();
        def = nullptr;
        version = nullptr;
    }
};

GameMemory::GameMemory() : impl_(std::make_unique<Impl>()) {}
GameMemory::~GameMemory() { impl_->close(); }

void GameMemory::setDefinitions(std::vector<GameDef> defs) {
    detach("");
    defs_ = std::move(defs);
    nextAttachNs_ = 0;
}

bool GameMemory::attached() const { return impl_->version != nullptr; }
const GameDef* GameMemory::game() const { return impl_->def; }

std::optional<double> GameMemory::value(const std::string& name) const {
    auto it = values_.find(name);
    if (it == values_.end()) return std::nullopt;
    return it->second;
}

void GameMemory::detach(const std::string& why) {
    impl_->close();
    if (!values_.empty()) ++changes_;
    values_.clear();
    info_ = {};
    message_ = why;
}

void GameMemory::poll(uint64_t nowNs) {
#ifndef _WIN32
    (void)nowNs;
    message_ = "Reading game memory is Windows-only for now.";
#else
    if (!attached()) {
        if (nowNs >= nextAttachNs_) {
            nextAttachNs_ = nowNs + 1'000'000'000;
            tryAttach();
        }
        if (!attached()) return;
    }
    if (nowNs >= nextAliveCheckNs_) {
        nextAliveCheckNs_ = nowNs + 500'000'000;
        DWORD code = 0;
        if (!GetExitCodeProcess(impl_->process, &code) || code != STILL_ACTIVE) {
            detach("The game closed.");
            return;
        }
    }
    const ModuleBase base = [this](const std::string& m) { return impl_->moduleBase(m); };
    const ReadMemory read = [this](uint64_t a, void* o, size_t n) { return impl_->read(a, o, n); };
    std::map<std::string, double> next;
    for (const auto& [name, v] : impl_->version->values)
        if (auto x = readGameValue(v, base, read, impl_->pointers64)) next[name] = *x;
    if (next != values_) {
        values_ = std::move(next);
        ++changes_;
    }
#endif
}

void GameMemory::tryAttach() {
#ifdef _WIN32
    if (defs_.empty()) {
        message_ = "No game definitions found in assets/games.";
        return;
    }
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return;
    DWORD pid = 0;
    const GameDef* def = nullptr;
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof pe;
    for (BOOL ok = Process32FirstW(snap, &pe); ok && !def; ok = Process32NextW(snap, &pe)) {
        const std::string exe = lower(narrow(pe.szExeFile));
        for (const auto& d : defs_)
            if (lower(d.process) == exe) {
                def = &d;
                pid = pe.th32ProcessID;
                break;
            }
    }
    CloseHandle(snap);
    if (!def) {
        std::string names;
        for (const auto& d : defs_) names += (names.empty() ? "" : ", ") + d.process;
        message_ = "Game not running (looking for " + names + ").";
        return;
    }

    // Read-only access: enough to read memory and see when the game exits.
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!h) {
        message_ = "Found " + def->process + " but could not open it (error " + std::to_string(GetLastError()) +
                   "). If the game runs as administrator, run Input Display the same way.";
        return;
    }
    impl_->process = h;

    snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snap == INVALID_HANDLE_VALUE) {
        impl_->close();
        message_ = "Found " + def->process + " but could not list its modules yet; retrying.";
        return;
    }
    MODULEENTRY32W me{};
    me.dwSize = sizeof me;
    for (BOOL ok = Module32FirstW(snap, &me); ok; ok = Module32NextW(snap, &me))
        impl_->modules[lower(narrow(me.szModule))] = (uint64_t)(uintptr_t)me.modBaseAddr;
    CloseHandle(snap);

    // Identify the build from the exe's PE header.
    const auto exeBase = impl_->moduleBase(def->process);
    int32_t peOffset = 0;
    uint32_t sig = 0, tds = 0, soi = 0;
    uint16_t machine = 0;
    if (!exeBase || !impl_->read(*exeBase + 0x3C, &peOffset, 4) || peOffset <= 0 || peOffset > 0x1000 ||
        !impl_->read(*exeBase + peOffset, &sig, 4) || sig != 0x00004550 ||
        !impl_->read(*exeBase + peOffset + 4, &machine, 2) || !impl_->read(*exeBase + peOffset + 8, &tds, 4) ||
        !impl_->read(*exeBase + peOffset + 24 + 56, &soi, 4)) {
        impl_->close();
        message_ = "Found " + def->process + " but could not read its exe header; retrying.";
        return;
    }
    impl_->pointers64 = machine != 0x014C;  // IMAGE_FILE_MACHINE_I386

    info_ = {};
    info_.gameName = def->name;
    info_.process = def->process;
    info_.pid = pid;
    info_.timeDateStamp = tds;
    info_.sizeOfImage = soi;
    const VersionMatch m = matchVersion(*def, tds, soi);
    if (!m.version) {
        impl_->close();
        message_ = "Unknown build of " + def->name + " (timeDateStamp " + hex32(tds) + ", sizeOfImage " + hex32(soi) +
                   "). Add a version for it to the game definition.";
        return;
    }
    impl_->def = def;
    impl_->version = m.version;
    info_.versionLabel = m.version->label;
    info_.exactBuild = m.exact;
    message_.clear();
#endif
}

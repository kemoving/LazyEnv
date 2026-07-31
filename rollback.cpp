// LazyEnv - Cross-platform, recoverable, zero-pollution dev environment configurator
// Copyright (C) 2026 Rein
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

// ============================================================================
// LazyEnv - rollback.cpp
// Environment variable snapshot & rollback implementation
// ============================================================================

#include "rollback.h"

#include <Windows.h>
#include <ShlObj.h>

#include <fstream>
#include <sstream>
#include <chrono>
#include <random>
#include <algorithm>
#include <stdexcept>
#include <format>

// ---------------------------------------------------------------------------
// Minimal JSON helpers (no external dependency)
// ---------------------------------------------------------------------------
namespace {

std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 16);
    for (char c : s) {
        switch (c) {
            case '\"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

std::string jsonUnescape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            switch (s[i + 1]) {
                case '\"': out += '\"'; ++i; break;
                case '\\': out += '\\'; ++i; break;
                case 'b':  out += '\b'; ++i; break;
                case 'f':  out += '\f'; ++i; break;
                case 'n':  out += '\n'; ++i; break;
                case 'r':  out += '\r'; ++i; break;
                case 't':  out += '\t'; ++i; break;
                default:   out += s[i]; break;
            }
        } else {
            out += s[i];
        }
    }
    return out;
}

// Extract a quoted string value after "key": "..."
std::string extractJsonString(const std::string& json,
                              const std::string& key) {
    std::string needle = "\"" + key + "\"";
    auto pos = json.find(needle);
    if (pos == std::string::npos) return "";
    pos = json.find('\"', pos + needle.size() + 1); // skip colon, find opening quote
    if (pos == std::string::npos) return "";
    ++pos;
    std::string result;
    for (; pos < json.size(); ++pos) {
        if (json[pos] == '\\' && pos + 1 < json.size()) {
            result += json[pos];
            result += json[pos + 1];
            ++pos;
        } else if (json[pos] == '\"') {
            break;
        } else {
            result += json[pos];
        }
    }
    return jsonUnescape(result);
}

uint32_t extractJsonUint(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\"";
    auto pos = json.find(needle);
    if (pos == std::string::npos) return 0;
    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos) return 0;
    ++pos;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) ++pos;
    return static_cast<uint32_t>(std::stoul(json.substr(pos)));
}

// Parse a JSON array of SnapshotEntry objects.
std::vector<lazyenv::SnapshotEntry> parseEntryArray(const std::string& json,
                                                     const std::string& key) {
    std::vector<lazyenv::SnapshotEntry> entries;
    std::string needle = "\"" + key + "\"";
    auto arrStart = json.find(needle);
    if (arrStart == std::string::npos) return entries;
    arrStart = json.find('[', arrStart);
    if (arrStart == std::string::npos) return entries;

    // Find matching ']'
    int depth = 0;
    size_t arrEnd = arrStart;
    for (size_t i = arrStart; i < json.size(); ++i) {
        if (json[i] == '[') ++depth;
        else if (json[i] == ']') { --depth; if (depth == 0) { arrEnd = i; break; } }
    }

    // Split by '{' ... '}'
    size_t pos = arrStart;
    while (true) {
        auto objStart = json.find('{', pos);
        if (objStart == std::string::npos || objStart > arrEnd) break;
        auto objEnd = json.find('}', objStart);
        if (objEnd == std::string::npos || objEnd > arrEnd) break;
        std::string obj = json.substr(objStart, objEnd - objStart + 1);
        lazyenv::SnapshotEntry e;
        e.name  = extractJsonString(obj, "name");
        e.value = extractJsonString(obj, "value");
        e.type  = extractJsonUint(obj, "type");
        entries.push_back(std::move(e));
        pos = objEnd + 1;
    }
    return entries;
}

std::wstring utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.data(),
                                  static_cast<int>(s.size()), nullptr, 0);
    std::wstring ws(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(),
                        static_cast<int>(s.size()), ws.data(), len);
    return ws;
}

std::string wideToUtf8(const std::wstring& ws) {
    if (ws.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, ws.data(),
                                  static_cast<int>(ws.size()),
                                  nullptr, 0, nullptr, nullptr);
    std::string s(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws.data(),
                        static_cast<int>(ws.size()),
                        s.data(), len, nullptr, nullptr);
    return s;
}

} // anonymous namespace

// ===========================================================================
// RollbackManager implementation
// ===========================================================================
namespace lazyenv {

RollbackManager::RollbackManager() {
    // Determine storage directory: %LOCALAPPDATA%/LazyEnv/snapshots
    wchar_t* appdata = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &appdata))) {
        storageDir_ = std::filesystem::path(appdata) / L"LazyEnv" / L"snapshots";
        CoTaskMemFree(appdata);
    } else {
        // Fallback
        storageDir_ = std::filesystem::path("C:/LazyEnv/snapshots");
    }
    std::filesystem::create_directories(storageDir_);
}

// ---------------------------------------------------------------------------
// Registry capture
// ---------------------------------------------------------------------------
std::vector<SnapshotEntry> RollbackManager::captureRegistryKey(bool system) {
    std::vector<SnapshotEntry> entries;

    HKEY root = system ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    const wchar_t* subkey = system
        ? L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment"
        : L"Environment";

    HKEY hKey = nullptr;
    if (RegOpenKeyExW(root, subkey, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return entries;

    DWORD index = 0;
    wchar_t nameBuf[32767];
    BYTE   dataBuf[32767];
    while (true) {
        DWORD nameLen = sizeof(nameBuf) / sizeof(wchar_t);
        DWORD dataLen = sizeof(dataBuf);
        DWORD type    = 0;
        LONG  rc = RegEnumValueW(hKey, index, nameBuf, &nameLen,
                                 nullptr, &type, dataBuf, &dataLen);
        if (rc != ERROR_SUCCESS) break;

        SnapshotEntry e;
        e.name  = wideToUtf8(std::wstring(nameBuf, nameLen));
        e.type  = type;
        if (type == REG_SZ || type == REG_EXPAND_SZ) {
            // dataLen includes null terminator (in bytes)
            size_t wcharCount = dataLen / sizeof(wchar_t);
            if (wcharCount > 0 && reinterpret_cast<wchar_t*>(dataBuf)[wcharCount - 1] == L'\0')
                --wcharCount;
            e.value = wideToUtf8(std::wstring(reinterpret_cast<wchar_t*>(dataBuf), wcharCount));
        }
        entries.push_back(std::move(e));
        ++index;
    }
    RegCloseKey(hKey);
    return entries;
}

// ---------------------------------------------------------------------------
// Registry restore
// ---------------------------------------------------------------------------
bool RollbackManager::restoreRegistryKey(
    const std::vector<SnapshotEntry>& entries, bool system) {

    HKEY root = system ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    const wchar_t* subkey = system
        ? L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment"
        : L"Environment";

    HKEY hKey = nullptr;
    REGSAM access = KEY_READ | KEY_WRITE;
    if (RegOpenKeyExW(root, subkey, 0, access, &hKey) != ERROR_SUCCESS)
        return false;

    // Step 1: delete all existing values
    std::vector<std::wstring> existing;
    {
        DWORD idx = 0;
        wchar_t buf[32767];
        while (true) {
            DWORD len = sizeof(buf) / sizeof(wchar_t);
            if (RegEnumValueW(hKey, idx, buf, &len,
                              nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
                break;
            existing.emplace_back(buf, len);
            ++idx;
        }
    }
    for (auto& name : existing) {
        RegDeleteValueW(hKey, name.c_str());
    }

    // Step 2: write snapshot values
    for (auto& e : entries) {
        std::wstring wname = utf8ToWide(e.name);
        std::wstring wval  = utf8ToWide(e.value);
        DWORD dataSize = static_cast<DWORD>((wval.size() + 1) * sizeof(wchar_t));
        RegSetValueExW(hKey, wname.c_str(), 0, e.type,
                       reinterpret_cast<const BYTE*>(wval.c_str()), dataSize);
    }

    RegCloseKey(hKey);
    return true;
}

// ---------------------------------------------------------------------------
// Single variable read / write
// ---------------------------------------------------------------------------
std::string RollbackManager::readEnvVariable(const std::string& name, bool system) {
    HKEY root = system ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    const wchar_t* subkey = system
        ? L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment"
        : L"Environment";

    HKEY hKey = nullptr;
    if (RegOpenKeyExW(root, subkey, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return "";

    wchar_t buf[32767];
    DWORD   bufSize = sizeof(buf);
    DWORD   type    = 0;
    std::wstring wname = utf8ToWide(name);
    LONG rc = RegQueryValueExW(hKey, wname.c_str(), nullptr, &type,
                               reinterpret_cast<BYTE*>(buf), &bufSize);
    RegCloseKey(hKey);
    if (rc != ERROR_SUCCESS) return "";

    size_t wcharCount = bufSize / sizeof(wchar_t);
    if (wcharCount > 0 && buf[wcharCount - 1] == L'\0') --wcharCount;
    return wideToUtf8(std::wstring(buf, wcharCount));
}

bool RollbackManager::writeEnvVariable(const std::string& name,
                                        const std::string& value,
                                        uint32_t type,
                                        bool system) {
    HKEY root = system ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    const wchar_t* subkey = system
        ? L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment"
        : L"Environment";

    HKEY hKey = nullptr;
    if (RegOpenKeyExW(root, subkey, 0, KEY_WRITE, &hKey) != ERROR_SUCCESS)
        return false;

    std::wstring wname = utf8ToWide(name);
    std::wstring wval  = utf8ToWide(value);
    DWORD dataSize = static_cast<DWORD>((wval.size() + 1) * sizeof(wchar_t));
    LONG rc = RegSetValueExW(hKey, wname.c_str(), 0, type,
                             reinterpret_cast<const BYTE*>(wval.c_str()), dataSize);
    RegCloseKey(hKey);
    return rc == ERROR_SUCCESS;
}

bool RollbackManager::deleteEnvVariable(const std::string& name, bool system) {
    HKEY root = system ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    const wchar_t* subkey = system
        ? L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment"
        : L"Environment";

    HKEY hKey = nullptr;
    LONG rc = RegOpenKeyExW(root, subkey, 0, KEY_SET_VALUE, &hKey);
    if (rc != ERROR_SUCCESS) return false;

    std::wstring wname = utf8ToWide(name);
    rc = RegDeleteValueW(hKey, wname.c_str());
    RegCloseKey(hKey);
    return rc == ERROR_SUCCESS;
}

void RollbackManager::broadcastEnvironmentChange() {
    DWORD_PTR result = 0;
    SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0,
                        reinterpret_cast<LPARAM>(L"Environment"),
                        SMTO_ABORTIFHUNG, 5000, &result);
}

// ---------------------------------------------------------------------------
// Snapshot CRUD
// ---------------------------------------------------------------------------
std::string RollbackManager::createSnapshot(const std::string& description) {
    Snapshot snap;
    snap.id          = generateId();
    snap.timestamp   = currentTimestamp();
    snap.description = description.empty() ? "Manual snapshot" : description;
    snap.user_env    = captureRegistryKey(false);
    snap.system_env  = captureRegistryKey(true);

    if (!saveSnapshot(snap))
        return "";
    return snap.id;
}

std::vector<Snapshot> RollbackManager::listSnapshots() const {
    std::vector<Snapshot> result;
    if (!std::filesystem::exists(storageDir_)) return result;

    for (auto& entry : std::filesystem::directory_iterator(storageDir_)) {
        if (entry.path().extension() == ".json") {
            try {
                result.push_back(loadSnapshot(entry.path()));
            } catch (...) {
                // Skip corrupted files
            }
        }
    }
    // Sort newest first
    std::sort(result.begin(), result.end(),
              [](const Snapshot& a, const Snapshot& b) {
                  return a.timestamp > b.timestamp;
              });
    return result;
}

bool RollbackManager::restoreSnapshot(const std::string& snapshotId) {
    auto path = storageDir_ / (snapshotId + ".json");
    if (!std::filesystem::exists(path)) return false;

    Snapshot snap = loadSnapshot(path);

    // Restore user environment
    if (!restoreRegistryKey(snap.user_env, false))
        return false;

    // Attempt system environment (may fail without admin privileges)
    restoreRegistryKey(snap.system_env, true);

    broadcastEnvironmentChange();
    return true;
}

bool RollbackManager::deleteSnapshot(const std::string& snapshotId) {
    auto path = storageDir_ / (snapshotId + ".json");
    return std::filesystem::remove(path);
}

bool RollbackManager::exportSnapshot(const std::string& snapshotId,
                                     const std::string& destPath) const {
    auto src = storageDir_ / (snapshotId + ".json");
    if (!std::filesystem::exists(src)) return false;
    try {
        std::filesystem::copy_file(src, std::filesystem::path(utf8ToWide(destPath)),
                                   std::filesystem::copy_options::overwrite_existing);
        return true;
    } catch (...) {
        return false;
    }
}

std::string RollbackManager::importSnapshot(const std::string& srcPath) {
    try {
        auto src = std::filesystem::path(utf8ToWide(srcPath));
        if (!std::filesystem::exists(src)) return "";

        // Load and validate the external file
        Snapshot snap = loadSnapshot(src);
        if (snap.id.empty() || snap.timestamp.empty()) return "";

        // Assign a new ID and timestamp to avoid collisions
        std::string originalId = snap.id;
        snap.id = generateId();
        std::string importNote = snap.description.empty()
            ? "Imported from external file"
            : snap.description + " (imported)";
        snap.description = importNote;

        if (!saveSnapshot(snap)) return "";
        return snap.id;
    } catch (...) {
        return "";
    }
}

// ---------------------------------------------------------------------------
// JSON serialization
// ---------------------------------------------------------------------------
bool RollbackManager::saveSnapshot(const Snapshot& snap) const {
    std::ostringstream os;
    os << "{\n";
    os << "  \"id\": \"" << jsonEscape(snap.id) << "\",\n";
    os << "  \"timestamp\": \"" << jsonEscape(snap.timestamp) << "\",\n";
    os << "  \"description\": \"" << jsonEscape(snap.description) << "\",\n";

    auto writeEntries = [&](const std::string& key,
                            const std::vector<SnapshotEntry>& entries) {
        os << "  \"" << key << "\": [\n";
        for (size_t i = 0; i < entries.size(); ++i) {
            os << "    {\"name\": \"" << jsonEscape(entries[i].name)
               << "\", \"value\": \"" << jsonEscape(entries[i].value)
               << "\", \"type\": " << entries[i].type << "}";
            if (i + 1 < entries.size()) os << ",";
            os << "\n";
        }
        os << "  ]";
    };

    writeEntries("user_env", snap.user_env);
    os << ",\n";
    writeEntries("system_env", snap.system_env);
    os << "\n}\n";

    auto path = storageDir_ / (snap.id + ".json");
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f << os.str();
    return f.good();
}

Snapshot RollbackManager::loadSnapshot(const std::filesystem::path& path) const {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("Cannot open snapshot file");
    std::string json((std::istreambuf_iterator<char>(f)),
                      std::istreambuf_iterator<char>());

    Snapshot snap;
    snap.id          = extractJsonString(json, "id");
    snap.timestamp   = extractJsonString(json, "timestamp");
    snap.description = extractJsonString(json, "description");
    snap.user_env    = parseEntryArray(json, "user_env");
    snap.system_env  = parseEntryArray(json, "system_env");
    return snap;
}

// ---------------------------------------------------------------------------
// Utilities
// ---------------------------------------------------------------------------
std::string RollbackManager::generateId() {
    static std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<uint32_t> dist(0, 0xFFFFFFFF);
    return std::format("{:08x}-{:04x}-{:04x}-{:04x}-{:012x}",
                       dist(rng),
                       dist(rng) & 0xFFFF,
                       (dist(rng) & 0x0FFF) | 0x4000,
                       (dist(rng) & 0x3FFF) | 0x8000,
                       static_cast<uint64_t>(dist(rng)) << 16 | (dist(rng) & 0xFFFF));
}

std::string RollbackManager::currentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_s(&tm, &time);
    return std::format("{:04d}-{:02d}-{:02d}T{:02d}:{:02d}:{:02d}",
                       tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                       tm.tm_hour, tm.tm_min, tm.tm_sec);
}

} // namespace lazyenv

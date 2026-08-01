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
// LazyEnv - main.cpp
// WinMain entry: borderless window, DPI-aware, native<->web bridge
// ============================================================================

#include "webview_host.h"
#include "installer.h"
#include "rollback.h"

#include <Windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <shlobj.h>
#include <dwmapi.h>
#include <shellscalingapi.h>
#include <ShObjIdl.h>
#include <VersionHelpers.h>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "shcore.lib")

#ifndef DWMWA_TRANSITIONS_FORCEDISABLED
#define DWMWA_TRANSITIONS_FORCEDISABLED 3
#endif

#include <string>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <thread>
#include <format>
#include <vector>
#include <stdexcept>
#include <algorithm>
#include <functional>
#include <memory>

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------
static constexpr wchar_t kWindowClass[] = L"LazyEnvMainWindow";
static constexpr wchar_t kWindowTitle[] = L"LazyEnv";

static lazyenv::WebViewHost     g_webview;
static lazyenv::Installer       g_installer;
static lazyenv::RollbackManager g_rollback;
// g_msgMutex is no longer needed; postMessage is now thread-safe via message queue
static HWND                     g_mainWindow = nullptr;
static bool                     g_isMaximized  = false;
static bool                     g_wasMinimized = false;
static RECT                     g_normalRect = {};   // saved before "fake maximize"

// ---------------------------------------------------------------------------
// JSON helpers
// ---------------------------------------------------------------------------
namespace {

std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 32);
    for (char c : s) {
        switch (c) {
            case '\"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                    out += buf;
                } else {
                    out += c;
                }
                break;
        }
    }
    return out;
}

std::string extractJsonValue(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\"";
    auto pos = json.find(needle);
    if (pos == std::string::npos) return "";
    pos = json.find('\"', pos + needle.size() + 1);
    if (pos == std::string::npos) return "";
    ++pos;
    std::string result;
    for (; pos < json.size(); ++pos) {
        if (json[pos] == '\\' && pos + 1 < json.size()) {
            ++pos;
            switch (json[pos]) {
                case '\"': result += '\"'; break;
                case '\\': result += '\\'; break;
                case 'n':  result += '\n'; break;
                case 'r':  result += '\r'; break;
                case 't':  result += '\t'; break;
                default:   result += json[pos]; break;
            }
        } else if (json[pos] == '\"') {
            break;
        } else {
            result += json[pos];
        }
    }
    return result;
}

std::vector<std::string> extractJsonArray(const std::string& json,
                                          const std::string& key) {
    std::vector<std::string> result;
    std::string needle = "\"" + key + "\"";
    auto pos = json.find(needle);
    if (pos == std::string::npos) return result;
    pos = json.find('[', pos);
    if (pos == std::string::npos) return result;
    auto end = json.find(']', pos);
    if (end == std::string::npos) return result;
    std::string arr = json.substr(pos + 1, end - pos - 1);
    size_t i = 0;
    while (i < arr.size()) {
        auto q1 = arr.find('\"', i);
        if (q1 == std::string::npos) break;
        auto q2 = arr.find('\"', q1 + 1);
        if (q2 == std::string::npos) break;
        result.push_back(arr.substr(q1 + 1, q2 - q1 - 1));
        i = q2 + 1;
    }
    return result;
}

std::wstring getHtmlUri() {
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::filesystem::path base = std::filesystem::path(exePath).parent_path();
    std::filesystem::path htmlPath = base / "resources" / "index.html";

    // Convert to file:// URI
    // Windows path: C:\foo\bar -> file:///C:/foo/bar
    std::wstring wpath = htmlPath.wstring();
    // Replace backslashes with forward slashes
    for (auto& ch : wpath) {
        if (ch == L'\\') ch = L'/';
    }
    return L"file:///" + wpath;
}

// Detect installed development environments on the system
std::string detectInstalledEnvironments() {
    struct EnvCheck {
        std::string name;
        std::string command;
        std::string versionFlag;
        std::string category;
    };

    std::vector<EnvCheck> checks = {
        {"Python",       "python",    "--version", "language"},
        {"Python 3",     "python3",   "--version", "language"},
        {"Node.js",      "node",      "--version", "language"},
        {"Go",           "go",        "version",   "language"},
        {"Rust (rustc)", "rustc",     "--version", "language"},
        {"Rust (cargo)", "cargo",     "--version", "language"},
        {"Java",         "java",      "-version",  "language"},
        {"Ruby",         "ruby",      "--version", "language"},
        {"PHP",          "php",       "--version", "language"},
        {"Perl",         "perl",      "--version", "language"},
        {"Lua",          "lua",       "-v",        "language"},
        {"Dart",         "dart",      "--version", "language"},
        {"Kotlin",       "kotlin",    "-version",  "language"},
        {"Swift",        "swift",     "--version", "language"},
        {"Zig",          "zig",       "version",   "language"},
        {"Deno",         "deno",      "--version", "language"},
        {"Bun",          "bun",       "--version", "language"},
        {"Git",          "git",       "--version", "tool"},
        {"CMake",        "cmake",     "--version", "tool"},
        {"Ninja",        "ninja",     "--version", "tool"},
        {"Make",         "make",      "--version", "tool"},
        {"GCC",          "gcc",       "--version", "tool"},
        {"G++",          "g++",       "--version", "tool"},
        {"Clang",        "clang",     "--version", "tool"},
        {"MSVC (cl)",    "cl",        "",          "tool"},
        {"Docker",       "docker",    "--version", "runtime"},
        {"npm",          "npm",       "--version", "tool"},
        {"yarn",         "yarn",      "--version", "tool"},
        {"pnpm",         "pnpm",      "--version", "tool"},
        {"pip",          "pip",       "--version", "tool"},
        {"conda",        "conda",     "--version", "tool"},
        {"dotnet",       "dotnet",    "--version", "language"},
        {"PowerShell",   "pwsh",      "--version", "tool"},
        {"GitHub CLI",   "gh",        "--version", "tool"},
        {"FFmpeg",       "ffmpeg",    "-version",  "utility"},
        {"curl",         "curl",      "--version", "utility"},
        {"wget",         "wget",      "--version", "utility"},
        {"jq",           "jq",        "--version", "utility"},
        {"ripgrep",      "rg",        "--version", "utility"},
        {"fd",           "fd",        "--version", "utility"},
        {"fzf",          "fzf",       "--version", "utility"},
    };

    std::ostringstream os;
    os << "[";
    bool first = true;

    for (auto& chk : checks) {
        std::string output;
        std::string cmd = chk.command;
        if (!chk.versionFlag.empty()) {
            cmd += " " + chk.versionFlag;
        }
        cmd += " 2>&1";

        int rc = lazyenv::Installer::runCommand(cmd, output, 5000);

        // Extract first line of output as version
        std::string version;
        if (rc == 0 && !output.empty()) {
            auto nl = output.find('\n');
            version = output.substr(0, nl == std::string::npos ? output.size() : nl);
            while (!version.empty() && (version.back() == '\r' || version.back() == '\n' || version.back() == ' '))
                version.pop_back();
            if (version.size() > 120) version = version.substr(0, 120);
        }

        if (rc == 0 && !version.empty()) {
            if (!first) os << ",";
            first = false;
            os << "{\"name\":\"" << jsonEscape(chk.name)
               << "\",\"command\":\"" << jsonEscape(chk.command)
               << "\",\"version\":\"" << jsonEscape(version)
               << "\",\"category\":\"" << jsonEscape(chk.category)
               << "\"}";
        }
    }
    os << "]";
    return os.str();
}

// Probe a single command: run "command --version" (and fallbacks) to detect it
std::string probeCommand(const std::string& command, const std::string& category) {
    // Try common version flags
    std::vector<std::string> flags = {"--version", "-version", "-v", "version", ""};

    for (auto& flag : flags) {
        std::string cmd = command;
        if (!flag.empty()) cmd += " " + flag;
        cmd += " 2>&1";

        std::string output;
        int rc = lazyenv::Installer::runCommand(cmd, output, 5000);

        if (rc == 0 && !output.empty()) {
            auto nl = output.find('\n');
            std::string version = output.substr(0, nl == std::string::npos ? output.size() : nl);
            while (!version.empty() && (version.back() == '\r' || version.back() == '\n' || version.back() == ' '))
                version.pop_back();
            if (version.size() > 120) version = version.substr(0, 120);

            if (!version.empty()) {
                return std::format(
                    "{{\"action\":\"probeResult\",\"found\":true,"
                    "\"name\":\"{}\",\"command\":\"{}\","
                    "\"version\":\"{}\",\"category\":\"{}\"}}",
                    jsonEscape(command), jsonEscape(command),
                    jsonEscape(version), jsonEscape(category));
            }
        }
    }

    return std::format(
        "{{\"action\":\"probeResult\",\"found\":false,"
        "\"command\":\"{}\",\"message\":\"Command not found or not recognized.\"}}",
        jsonEscape(command));
}

// ---------------------------------------------------------------------------
// Generic SEH-safe background thread launcher
//
// C++ try-catch cannot catch ACCESS_VIOLATION (C0000005). We use Win32
// CreateThread + __try/__except as the entry point for all background work.
// The SEH thread proc must NOT contain any C++ objects with destructors
// (MSVC C2712 restriction), so the work/error functors are stored on the
// heap and accessed via raw pointers.
// ---------------------------------------------------------------------------
struct ThreadPayload {
    std::function<void()> work;
    std::function<void()> onError;
};

static void callOnErrorSafe(const std::function<void()>& onError) {
    try {
        onError();
    } catch (...) {
        // onError threw — thread is already in an error state, nothing to do
    }
}

static DWORD WINAPI sehThreadProc(LPVOID param) {
    ThreadPayload* payload = static_cast<ThreadPayload*>(param);
    bool sehException = false;
    __try {
        payload->work();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        sehException = true;
    }
    if (sehException && payload->onError) {
        callOnErrorSafe(payload->onError);
    }
    delete payload;
    return 0;
}

static void launchThreadSafe(std::function<void()> work,
                             std::function<void()> onError = nullptr) {
    auto* payload = new ThreadPayload{std::move(work), std::move(onError)};
    HANDLE h = CreateThread(nullptr, 0, sehThreadProc, payload, 0, nullptr);
    if (h) CloseHandle(h);  // detached — thread owns its own lifecycle
}

// SEH wrapper for environment detection (kept inline for clarity)
static void doDetectEnvironments() {
    try {
        std::string envJson = detectInstalledEnvironments();
        std::string respMsg = "{\"action\":\"environmentsDetected\",\"environments\":" + envJson + "}";
        g_webview.postMessage(respMsg);
    } catch (const std::exception& e) {
        std::string errMsg = "{\"action\":\"environmentsDetected\",\"environments\":[],\"error\":\"" + jsonEscape(e.what()) + "\"}";
        g_webview.postMessage(errMsg);
    } catch (...) {
        g_webview.postMessage("{\"action\":\"environmentsDetected\",\"environments\":[],\"error\":\"Unknown error during environment detection\"}");
    }
}

// Post an error message — must be a separate function free of __try
static void reportEnvDetectionSEHError() {
    g_webview.postMessage("{\"action\":\"environmentsDetected\",\"environments\":[],\"error\":\"Fatal system exception (SEH)\"}");
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Message handler
// ---------------------------------------------------------------------------
std::string handleWebMessage(const std::string& message) {
    std::string action = extractJsonValue(message, "action");

    // Top-level safety net: catch any exception that escapes from
    // snapshot/registry/filesystem operations, return a structured
    // error so the UI can report the problem instead of crashing.
    try {
        // ------ Admin check ------
        if (action == "adminCheck") {
        bool isAdmin = IsUserAnAdmin();
        return std::format("{{\"action\":\"adminStatus\",\"isAdmin\":{}}}", isAdmin ? "true" : "false");
    }

    // ------ Window controls ------
    if (action == "windowMinimize") {
        ShowWindow(g_mainWindow, SW_MINIMIZE);
        return "";
    }
    if (action == "windowMaximize") {
        // Avoid ShowWindow(SW_MAXIMIZE/SW_RESTORE) entirely.
        // Those window-state transitions cause WebView2 to keep
        // its old GPU texture on shrink (maximized→normal), which
        // leads to clipped rendering.
        //
        // Instead we stay in "normal" window state and use
        // SetWindowPos to manually fill / restore to the work area.
        // This is a plain resize — WebView2 handles it correctly
        // in both directions.
        if (g_isMaximized) {
            // Restore to saved normal rect
            g_isMaximized = false;
            SetWindowPos(g_mainWindow, nullptr,
                         g_normalRect.left, g_normalRect.top,
                         g_normalRect.right  - g_normalRect.left,
                         g_normalRect.bottom - g_normalRect.top,
                         SWP_NOZORDER | SWP_FRAMECHANGED);
        } else {
            // Save current rect, then fill the monitor work area
            GetWindowRect(g_mainWindow, &g_normalRect);
            g_isMaximized = true;
            HMONITOR mon = MonitorFromWindow(g_mainWindow,
                                             MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi{};
            mi.cbSize = sizeof(mi);
            GetMonitorInfoW(mon, &mi);
            SetWindowPos(g_mainWindow, nullptr,
                         mi.rcWork.left, mi.rcWork.top,
                         mi.rcWork.right  - mi.rcWork.left,
                         mi.rcWork.bottom - mi.rcWork.top,
                         SWP_NOZORDER | SWP_FRAMECHANGED);
        }
        return "";
    }
    if (action == "windowClose") {
        PostMessage(g_mainWindow, WM_CLOSE, 0, 0);
        return "";
    }

    // ------ Window drag (triggered from JS on titlebar mousedown) ------
    if (action == "windowDragStart") {
        // Post to the main window's message queue so it executes outside
        // the WebView2 callback context. This allows the system drag loop
        // to work correctly.
        PostMessageW(g_mainWindow, lazyenv::WM_WEBVIEW_DRAG_START, 0, 0);
        return "";
    }

    // ------ Detect installed environments ------
    if (action == "detectEnvironments") {
        launchThreadSafe(doDetectEnvironments, reportEnvDetectionSEHError);
        return "{\"action\":\"detectStarted\"}";
    }

    // ------ Probe a single command (manual add) ------
    if (action == "probeCommand") {
        std::string cmd = extractJsonValue(message, "command");
        std::string cat = extractJsonValue(message, "category");
        if (cat.empty()) cat = "other";

        launchThreadSafe([cmd, cat]() {
            try {
                std::string result = probeCommand(cmd, cat);
                g_webview.postMessage(result);
            } catch (...) {
                g_webview.postMessage("{\"action\":\"probeResult\",\"found\":false,\"command\":\"" + jsonEscape(cmd) + "\",\"error\":true}");
            }
        }, [cmd]() {
            g_webview.postMessage("{\"action\":\"probeResult\",\"found\":false,\"command\":\"" + jsonEscape(cmd) + "\",\"error\":\"Fatal SEH exception\"}");
        });
        return "{\"action\":\"probeStarted\"}";
    }

    // ------ Uninstall package ------
    if (action == "uninstallPackage") {
        std::string pkgName = extractJsonValue(message, "command");
        launchThreadSafe([pkgName]() {
            try {
                std::string output;
                std::string cmd = "winget uninstall --name \"" + pkgName + "\" --silent --accept-source-agreements 2>&1";
                int rc = lazyenv::Installer::runCommand(cmd, output, 300000);
                std::string respMsg = std::format(
                    "{{\"action\":\"uninstallResult\",\"command\":\"{}\",\"success\":{},\"output\":\"{}\"}}",
                    jsonEscape(pkgName), rc == 0 ? "true" : "false", jsonEscape(output.substr(0, 500)));
                g_webview.postMessage(respMsg);
            } catch (...) {
                std::string respMsg = std::format(
                    "{{\"action\":\"uninstallResult\",\"command\":\"{}\",\"success\":false,\"error\":true}}",
                    jsonEscape(pkgName));
                g_webview.postMessage(respMsg);
            }
        }, [pkgName]() {
            std::string respMsg = std::format(
                "{{\"action\":\"uninstallResult\",\"command\":\"{}\",\"success\":false,\"error\":\"Fatal SEH exception\"}}",
                jsonEscape(pkgName));
            g_webview.postMessage(respMsg);
        });
        return "{\"action\":\"uninstallStarted\"}";
    }

    // ------ Get catalog ------
    if (action == "getCatalog") {
        auto catalog = lazyenv::getDefaultCatalog();
        std::ostringstream os;
        os << "{\"action\":\"catalogData\",\"packages\":[";
        for (size_t i = 0; i < catalog.size(); ++i) {
            auto& p = catalog[i];
            os << "{\"id\":\"" << jsonEscape(p.id)
               << "\",\"name\":\"" << jsonEscape(p.displayName)
               << "\",\"category\":\"" << jsonEscape(p.category)
               << "\",\"description\":\"" << jsonEscape(p.description)
               << "\"}";
            if (i + 1 < catalog.size()) os << ",";
        }
        os << "]}";
        return os.str();
    }

    // ------ Check winget ------
    if (action == "checkWinget") {
        launchThreadSafe([]() {
            try {
                bool ok = lazyenv::Installer::isWingetAvailable();
                std::string m = std::format("{{\"action\":\"wingetStatus\",\"available\":{}}}",
                                            ok ? "true" : "false");
                g_webview.postMessage(m);
            } catch (...) {
                g_webview.postMessage("{\"action\":\"wingetStatus\",\"available\":false,\"error\":true}");
            }
        }, []() {
            g_webview.postMessage("{\"action\":\"wingetStatus\",\"available\":false,\"error\":\"Fatal SEH exception\"}");
        });
        return "{\"action\":\"wingetCheckStarted\"}";
    }

    // ------ Check if catalog packages are already installed ------
    if (action == "checkInstalled") {
        auto catalog = lazyenv::getDefaultCatalog();
        launchThreadSafe([catalog]() {
            try {
                std::vector<std::string> installed;
                for (auto& p : catalog) {
                    if (lazyenv::Installer::isPackageInstalled(p.id)) {
                        installed.push_back(p.id);
                    }
                }
                std::string idsJson = "[";
                for (size_t i = 0; i < installed.size(); ++i) {
                    if (i > 0) idsJson += ",";
                    idsJson += "\"" + jsonEscape(installed[i]) + "\"";
                }
                idsJson += "]";
                g_webview.postMessage("{\"action\":\"installedList\",\"packageIds\":" + idsJson + "}");
            } catch (...) {
                g_webview.postMessage("{\"action\":\"installedList\",\"packageIds\":[]}");
            }
        }, []() {
            g_webview.postMessage("{\"action\":\"installedList\",\"packageIds\":[]}");
        });
        return "{}";
    }

    // ------ Install packages (async, with streaming log) ------
    if (action == "install") {
        auto ids = extractJsonArray(message, "packages");
        std::string installLocation = extractJsonValue(message, "installLocation");
        auto catalog = lazyenv::getDefaultCatalog();
        std::string snapId = g_rollback.createSnapshot("Pre-install snapshot");

        launchThreadSafe([ids, installLocation, catalog, snapId]() {
            try {
            int total = static_cast<int>(ids.size());
            int current = 0;

            for (auto& id : ids) {
                lazyenv::PackageInfo pkg;
                bool found = false;
                for (auto& p : catalog) {
                    if (p.id == id) { pkg = p; found = true; break; }
                }
                if (!found) {
                    pkg.id = id;
                    pkg.displayName = id;
                }

                std::string cmdLine = std::format(
                    "winget install --id {} --exact --silent "
                    "--accept-package-agreements --accept-source-agreements",
                    pkg.id);

                // Append custom install location if specified.
                // Each package gets its own subfolder under the base path.
                std::string effectiveLocation;
                if (!installLocation.empty()) {
                    effectiveLocation = lazyenv::makePackageInstallLocation(
                        installLocation, pkg.displayName);
                    cmdLine += std::format(" --location \"{}\"", effectiveLocation);
                }

                // Send "running" status
                {
                    std::string m = std::format(
                        "{{\"action\":\"installProgress\",\"packageId\":\"{}\","
                        "\"status\":\"running\",\"message\":\"Installing {}...\","
                        "\"command\":\"{}\",\"current\":{},\"total\":{}}}",
                        jsonEscape(id), jsonEscape(pkg.displayName),
                        jsonEscape(cmdLine), current, total);
                    g_webview.postMessage(m);
                }

                // Run with streaming output
                std::string fullOutput;
                std::string pkgId = id;  // copy for lambda capture
                int exitCode = lazyenv::Installer::runCommandStreaming(
                    cmdLine, fullOutput,
                    [pkgId](const std::string& line) {
                        std::string m = std::format(
                            "{{\"action\":\"installLog\",\"packageId\":\"{}\","
                            "\"line\":\"{}\"}}",
                            jsonEscape(pkgId), jsonEscape(line));
                        g_webview.postMessage(m);
                    },
                    600000);

                ++current;

                // Determine final status
                std::string statusStr;
                if (exitCode == 0) {
                    statusStr = "success";
                    if (pkg.addToPath) {
                        if (!effectiveLocation.empty())
                            lazyenv::Installer::addToUserPath(effectiveLocation);
                        else if (!pkg.defaultPath.empty())
                            lazyenv::Installer::addToUserPath(pkg.defaultPath);
                    }
                } else if (fullOutput.find("already installed") != std::string::npos ||
                           fullOutput.find("No available upgrade") != std::string::npos) {
                    statusStr = "skipped";
                } else {
                    statusStr = "failed";
                }

                {
                    std::string m = std::format(
                        "{{\"action\":\"installProgress\",\"packageId\":\"{}\","
                        "\"status\":\"{}\",\"message\":\"{}\","
                        "\"command\":\"{}\",\"output\":\"{}\","
                        "\"exitCode\":{},\"current\":{},\"total\":{}}}",
                        jsonEscape(id), statusStr,
                        jsonEscape(pkg.displayName + ": " + statusStr),
                        jsonEscape(cmdLine),
                        jsonEscape(fullOutput.substr(0, 2000)),
                        exitCode, current, total);
                    g_webview.postMessage(m);
                }
            }

            {
                std::string m = std::format(
                    "{{\"action\":\"installComplete\",\"snapshotId\":\"{}\"}}",
                    jsonEscape(snapId));
                g_webview.postMessage(m);
            }
            } catch (const std::exception& e) {
                std::string m = std::format(
                    "{{\"action\":\"installComplete\",\"snapshotId\":\"{}\",\"error\":\"{}\"}}",
                    jsonEscape(snapId), jsonEscape(e.what()));
                g_webview.postMessage(m);
            } catch (...) {
                std::string m = std::format(
                    "{{\"action\":\"installComplete\",\"snapshotId\":\"{}\",\"error\":\"Unknown error during installation\"}}",
                    jsonEscape(snapId));
                g_webview.postMessage(m);
            }
        }, [snapId]() {
            std::string m = std::format(
                "{{\"action\":\"installComplete\",\"snapshotId\":\"{}\",\"error\":\"Fatal SEH exception during installation\"}}",
                jsonEscape(snapId));
            g_webview.postMessage(m);
        });

        return std::format("{{\"action\":\"installStarted\",\"snapshotId\":\"{}\"}}", jsonEscape(snapId));
    }

    // ------ Retry single package (also streaming) ------
    if (action == "retryInstall") {
        std::string id = extractJsonValue(message, "packageId");
        std::string installLocation = extractJsonValue(message, "installLocation");
        auto catalog = lazyenv::getDefaultCatalog();

        launchThreadSafe([id, installLocation, catalog]() {
            try {
            lazyenv::PackageInfo pkg;
            for (auto& p : catalog) {
                if (p.id == id) { pkg = p; break; }
            }
            if (pkg.id.empty()) {
                pkg.id = id;
                pkg.displayName = id;
            }

            std::string cmdLine = std::format(
                "winget install --id {} --exact --silent "
                "--accept-package-agreements --accept-source-agreements",
                pkg.id);

            // Append custom install location if specified.
            // Each package gets its own subfolder under the base path.
            std::string effectiveLocation;
            if (!installLocation.empty()) {
                effectiveLocation = lazyenv::makePackageInstallLocation(
                    installLocation, pkg.displayName);
                cmdLine += std::format(" --location \"{}\"", effectiveLocation);
            }

            {
                std::string m = std::format(
                    "{{\"action\":\"installProgress\",\"packageId\":\"{}\","
                    "\"status\":\"running\",\"message\":\"Retrying {}...\","
                    "\"command\":\"{}\"}}",
                    jsonEscape(id), jsonEscape(pkg.displayName), jsonEscape(cmdLine));
                g_webview.postMessage(m);
            }

            std::string fullOutput;
            std::string retryId = id;  // copy for lambda capture
            int exitCode = lazyenv::Installer::runCommandStreaming(
                cmdLine, fullOutput,
                [retryId](const std::string& line) {
                    std::string m = std::format(
                        "{{\"action\":\"installLog\",\"packageId\":\"{}\","
                        "\"line\":\"{}\"}}",
                        jsonEscape(retryId), jsonEscape(line));
                    g_webview.postMessage(m);
                },
                600000);

            std::string statusStr;
            if (exitCode == 0) {
                statusStr = "success";
                if (pkg.addToPath) {
                    if (!effectiveLocation.empty())
                        lazyenv::Installer::addToUserPath(effectiveLocation);
                    else if (!pkg.defaultPath.empty())
                        lazyenv::Installer::addToUserPath(pkg.defaultPath);
                }
            } else if (fullOutput.find("already installed") != std::string::npos ||
                       fullOutput.find("No available upgrade") != std::string::npos) {
                statusStr = "skipped";
            } else {
                statusStr = "failed";
            }

            {
                std::string m = std::format(
                    "{{\"action\":\"installProgress\",\"packageId\":\"{}\","
                    "\"status\":\"{}\",\"message\":\"{}\","
                    "\"command\":\"{}\",\"output\":\"{}\",\"exitCode\":{}}}",
                    jsonEscape(id), statusStr,
                    jsonEscape(pkg.displayName + ": " + statusStr),
                    jsonEscape(cmdLine),
                    jsonEscape(fullOutput.substr(0, 2000)),
                    exitCode);
                g_webview.postMessage(m);
            }
            } catch (const std::exception& e) {
                std::string m = std::format(
                    "{{\"action\":\"installProgress\",\"packageId\":\"{}\","
                    "\"status\":\"failed\",\"message\":\"Retry error: {}\"}}",
                    jsonEscape(id), jsonEscape(e.what()));
                g_webview.postMessage(m);
            } catch (...) {
                std::string m = std::format(
                    "{{\"action\":\"installProgress\",\"packageId\":\"{}\","
                    "\"status\":\"failed\",\"message\":\"Unknown error during retry\"}}",
                    jsonEscape(id));
                g_webview.postMessage(m);
            }
        }, [id]() {
            std::string m = std::format(
                "{{\"action\":\"installProgress\",\"packageId\":\"{}\","
                "\"status\":\"failed\",\"message\":\"Fatal SEH exception during retry\"}}",
                jsonEscape(id));
            g_webview.postMessage(m);
        });

        return "{\"action\":\"retryStarted\"}";
    }

    // ------ Create snapshot ------
    if (action == "createSnapshot") {
        std::string desc = extractJsonValue(message, "description");
        std::string id = g_rollback.createSnapshot(desc);
        bool ok = !id.empty();
        return std::format("{{\"action\":\"snapshotCreated\",\"id\":\"{}\",\"success\":{}}}",
                           jsonEscape(id), ok ? "true" : "false");
    }

    // ------ List snapshots ------
    if (action == "listSnapshots") {
        auto snaps = g_rollback.listSnapshots();
        std::ostringstream os;
        os << "{\"action\":\"snapshotList\",\"snapshots\":[";
        for (size_t i = 0; i < snaps.size(); ++i) {
            os << "{\"id\":\"" << jsonEscape(snaps[i].id)
               << "\",\"timestamp\":\"" << jsonEscape(snaps[i].timestamp)
               << "\",\"description\":\"" << jsonEscape(snaps[i].description)
               << "\",\"userVarCount\":" << snaps[i].user_env.size()
               << ",\"systemVarCount\":" << snaps[i].system_env.size()
               << "}";
            if (i + 1 < snaps.size()) os << ",";
        }
        os << "]}";
        return os.str();
    }

    // ------ Diff snapshot (compare snapshot with current registry) ------
    if (action == "diffSnapshot") {
        std::string id = extractJsonValue(message, "snapshotId");
        auto diffs = g_rollback.diffSnapshot(id);
        std::string json;
        json += "{\"action\":\"snapshotDiff\",\"snapshotId\":\"" + jsonEscape(id) + "\",\"diffs\":[";
        for (size_t i = 0; i < diffs.size(); ++i) {
            if (i > 0) json += ",";
            auto& d = diffs[i];
            json += std::format(
                "{{\"name\":\"{}\",\"currentValue\":\"{}\",\"snapshotValue\":\"{}\","
                "\"currentType\":{},\"snapshotType\":{},\"changeType\":\"{}\",\"system\":{}}}",
                jsonEscape(d.name), jsonEscape(d.currentValue),
                jsonEscape(d.snapshotValue),
                d.currentType, d.snapshotType,
                jsonEscape(d.changeType),
                d.system ? "true" : "false");
        }
        json += "]}";
        return json;
    }

    // ------ Restore snapshot (full or incremental) ------
    if (action == "restoreSnapshot") {
        std::string id   = extractJsonValue(message, "snapshotId");
        std::string mode = extractJsonValue(message, "mode");
        bool ok = false;
        if (mode == "incremental") {
            std::vector<std::string> names = extractJsonArray(message, "names");
            ok = g_rollback.restoreSnapshotIncremental(id, names);
        } else {
            ok = g_rollback.restoreSnapshot(id);
        }
        return std::format("{{\"action\":\"restoreResult\",\"success\":{},\"snapshotId\":\"{}\"}}",
                           ok ? "true" : "false", jsonEscape(id));
    }

    // ------ Delete snapshot ------
    if (action == "deleteSnapshot") {
        std::string id = extractJsonValue(message, "snapshotId");
        bool ok = g_rollback.deleteSnapshot(id);
        return std::format("{{\"action\":\"deleteResult\",\"success\":{},\"snapshotId\":\"{}\"}}",
                           ok ? "true" : "false", jsonEscape(id));
    }

    // ------ Export snapshot to file ------
    if (action == "exportSnapshot") {
        std::string id = extractJsonValue(message, "snapshotId");

        // Open a Save File dialog on the UI thread via PostMessage would be
        // complex; instead we use the COM file dialog directly here since
        // handleWebMessage is called from the UI thread context.
        HWND owner = g_mainWindow;
        launchThreadSafe([id, owner]() {
            // Must CoInitialize for file dialog on this thread
            CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

            IFileSaveDialog* pDialog = nullptr;
            HRESULT hr = CoCreateInstance(CLSID_FileSaveDialog, nullptr,
                                          CLSCTX_INPROC_SERVER,
                                          IID_PPV_ARGS(&pDialog));
            if (SUCCEEDED(hr)) {
                COMDLG_FILTERSPEC filter[] = {
                    { L"JSON Files", L"*.json" },
                    { L"All Files", L"*.*" }
                };
                pDialog->SetFileTypes(2, filter);
                pDialog->SetDefaultExtension(L"json");

                std::wstring defaultName = L"snapshot_" +
                    std::wstring(id.begin(), id.end()).substr(0, 8) + L".json";
                pDialog->SetFileName(defaultName.c_str());

                hr = pDialog->Show(owner);
                if (SUCCEEDED(hr)) {
                    IShellItem* pItem = nullptr;
                    hr = pDialog->GetResult(&pItem);
                    if (SUCCEEDED(hr)) {
                        PWSTR filePath = nullptr;
                        pItem->GetDisplayName(SIGDN_FILESYSPATH, &filePath);
                        if (filePath) {
                            // Convert wide path to UTF-8
                            int len = WideCharToMultiByte(CP_UTF8, 0, filePath, -1,
                                                         nullptr, 0, nullptr, nullptr);
                            std::string destPath(len - 1, '\0');
                            WideCharToMultiByte(CP_UTF8, 0, filePath, -1,
                                               destPath.data(), len, nullptr, nullptr);
                            CoTaskMemFree(filePath);

                            bool ok = g_rollback.exportSnapshot(id, destPath);
                            std::string m = std::format(
                                "{{\"action\":\"exportResult\",\"success\":{},"
                                "\"snapshotId\":\"{}\"}}",
                                ok ? "true" : "false", jsonEscape(id));
                            g_webview.postMessage(m);
                        }
                        pItem->Release();
                    }
                }
                pDialog->Release();
            }
            CoUninitialize();
        }, [id]() {
            std::string m = std::format(
                "{{\"action\":\"exportResult\",\"success\":false,\"snapshotId\":\"{}\",\"error\":\"Fatal SEH exception\"}}",
                jsonEscape(id));
            g_webview.postMessage(m);
        });

        return "";
    }

    // ------ Import snapshot from file ------
    if (action == "importSnapshot") {
        HWND owner = g_mainWindow;
        launchThreadSafe([owner]() {
            CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

            IFileOpenDialog* pDialog = nullptr;
            HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr,
                                          CLSCTX_INPROC_SERVER,
                                          IID_PPV_ARGS(&pDialog));
            if (SUCCEEDED(hr)) {
                COMDLG_FILTERSPEC filter[] = {
                    { L"JSON Files", L"*.json" },
                    { L"All Files", L"*.*" }
                };
                pDialog->SetFileTypes(2, filter);

                hr = pDialog->Show(owner);
                if (SUCCEEDED(hr)) {
                    IShellItem* pItem = nullptr;
                    hr = pDialog->GetResult(&pItem);
                    if (SUCCEEDED(hr)) {
                        PWSTR filePath = nullptr;
                        pItem->GetDisplayName(SIGDN_FILESYSPATH, &filePath);
                        if (filePath) {
                            int len = WideCharToMultiByte(CP_UTF8, 0, filePath, -1,
                                                         nullptr, 0, nullptr, nullptr);
                            std::string srcPath(len - 1, '\0');
                            WideCharToMultiByte(CP_UTF8, 0, filePath, -1,
                                               srcPath.data(), len, nullptr, nullptr);
                            CoTaskMemFree(filePath);

                            std::string newId = g_rollback.importSnapshot(srcPath);
                            bool ok = !newId.empty();
                            std::string m = std::format(
                                "{{\"action\":\"importResult\",\"success\":{},"
                                "\"snapshotId\":\"{}\"}}",
                                ok ? "true" : "false", jsonEscape(newId));
                            g_webview.postMessage(m);
                        }
                        pItem->Release();
                    }
                }
                pDialog->Release();
            }
            CoUninitialize();
        }, []() {
            g_webview.postMessage("{\"action\":\"importResult\",\"success\":false,\"error\":\"Fatal SEH exception\"}");
        });

        return "";
    }

    // ------ Check command ------
    if (action == "checkCommand") {
        std::string cmd = extractJsonValue(message, "command");
        launchThreadSafe([cmd]() {
            try {
                bool ok = lazyenv::Installer::isCommandAvailable(cmd);
                std::string m = std::format("{{\"action\":\"commandCheck\",\"command\":\"{}\",\"available\":{}}}",
                                           jsonEscape(cmd), ok ? "true" : "false");
                g_webview.postMessage(m);
            } catch (...) {
                g_webview.postMessage("{\"action\":\"commandCheck\",\"available\":false,\"error\":true}");
            }
        }, [cmd]() {
            std::string m = std::format("{{\"action\":\"commandCheck\",\"command\":\"{}\",\"available\":false,\"error\":\"Fatal SEH exception\"}}",
                                       jsonEscape(cmd));
            g_webview.postMessage(m);
        });
        return "{\"action\":\"commandCheckStarted\"}";
    }

    // ------ List environment variables (for Settings page) ------
    if (action == "listEnvVars") {
        std::string scope = extractJsonValue(message, "scope");
        bool sys = (scope == "system");

        // Use captureRegistryKey indirectly via a temporary snapshot approach,
        // or directly enumerate the registry. We'll enumerate directly.
        HKEY root = sys ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
        const wchar_t* subKey = sys
            ? L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment"
            : L"Environment";

        HKEY hKey = nullptr;
        LONG rc = RegOpenKeyExW(root, subKey, 0, KEY_READ, &hKey);

        std::ostringstream os;
        os << "{\"action\":\"envVarList\",\"variables\":[";
        bool first = true;

        if (rc == ERROR_SUCCESS) {
            DWORD index = 0;
            wchar_t nameBuf[1024];
            BYTE valueBuf[32768];
            DWORD nameLen, valueLen, type;

            while (true) {
                nameLen = 1024;
                valueLen = 32768;
                rc = RegEnumValueW(hKey, index, nameBuf, &nameLen,
                                   nullptr, &type, valueBuf, &valueLen);
                if (rc != ERROR_SUCCESS) break;

                // Convert name to UTF-8
                int nl = WideCharToMultiByte(CP_UTF8, 0, nameBuf, nameLen,
                                            nullptr, 0, nullptr, nullptr);
                std::string nameUtf8(nl, '\0');
                WideCharToMultiByte(CP_UTF8, 0, nameBuf, nameLen,
                                   nameUtf8.data(), nl, nullptr, nullptr);

                // Convert value to UTF-8 (handle REG_SZ and REG_EXPAND_SZ)
                std::string valueUtf8;
                if (type == REG_SZ || type == REG_EXPAND_SZ) {
                    int wchars = static_cast<int>(valueLen / sizeof(wchar_t));
                    if (wchars > 0 && reinterpret_cast<wchar_t*>(valueBuf)[wchars - 1] == L'\0')
                        wchars--;
                    int vl = WideCharToMultiByte(CP_UTF8, 0,
                                                reinterpret_cast<wchar_t*>(valueBuf), wchars,
                                                nullptr, 0, nullptr, nullptr);
                    valueUtf8.resize(vl);
                    WideCharToMultiByte(CP_UTF8, 0,
                                       reinterpret_cast<wchar_t*>(valueBuf), wchars,
                                       valueUtf8.data(), vl, nullptr, nullptr);
                } else if (type == REG_DWORD && valueLen >= 4) {
                    DWORD dval = *reinterpret_cast<DWORD*>(valueBuf);
                    valueUtf8 = std::to_string(dval);
                } else {
                    valueUtf8 = "(binary data)";
                }

                std::string typeStr;
                switch (type) {
                    case REG_SZ:        typeStr = "REG_SZ"; break;
                    case REG_EXPAND_SZ: typeStr = "REG_EXPAND_SZ"; break;
                    case REG_DWORD:     typeStr = "REG_DWORD"; break;
                    default:            typeStr = "REG_BINARY"; break;
                }

                if (!first) os << ",";
                first = false;
                os << "{\"name\":\"" << jsonEscape(nameUtf8)
                   << "\",\"value\":\"" << jsonEscape(valueUtf8)
                   << "\",\"type\":\"" << typeStr << "\"}";

                index++;
            }
            RegCloseKey(hKey);
        }

        os << "]}";
        return os.str();
    }

    // ------ Write environment variable (for Settings page) ------
    if (action == "writeEnvVar") {
        std::string name  = extractJsonValue(message, "name");
        std::string value = extractJsonValue(message, "value");
        std::string typeStr = extractJsonValue(message, "type");
        std::string scope = extractJsonValue(message, "scope");
        bool sys = (scope == "system");

        // Defensive: reject empty variable name
        if (name.empty()) {
            return "{\"action\":\"envVarWriteResult\",\"success\":false,"
                   "\"name\":\"\",\"message\":\"Variable name cannot be empty\"}";
        }

        uint32_t regType = REG_SZ;
        if (typeStr == "REG_EXPAND_SZ") regType = REG_EXPAND_SZ;

        // Auto-snapshot before modifying — safe rollback
        // Skip if non-admin touching system scope (write will fail anyway)
        if (!sys || IsUserAnAdmin()) {
            g_rollback.createSnapshot(std::format("Auto-save before modifying {} ({})",
                name, sys ? "system" : "user"));
        }

        bool ok = lazyenv::RollbackManager::writeEnvVariable(name, value, regType, sys);
        if (ok) {
            lazyenv::RollbackManager::broadcastEnvironmentChange();
        }

        return std::format(
            "{{\"action\":\"envVarWriteResult\",\"success\":{},\"name\":\"{}\","
            "\"message\":\"{}\"}}",
            ok ? "true" : "false", jsonEscape(name),
            ok ? "" : jsonEscape("Failed to write environment variable. "
                                 "Ensure you have sufficient privileges."));
    }

    // ------ Delete environment variable (for Settings page) ------
    if (action == "deleteEnvVar") {
        std::string name  = extractJsonValue(message, "name");
        std::string scope = extractJsonValue(message, "scope");
        bool sys = (scope == "system");

        // Defensive: reject empty variable name
        if (name.empty()) {
            return "{\"action\":\"envVarDeleteResult\",\"success\":false,"
                   "\"name\":\"\",\"message\":\"Variable name cannot be empty\"}";
        }

        // Auto-snapshot before deleting — safe rollback
        // Skip if non-admin touching system scope (delete will fail anyway)
        if (!sys || IsUserAnAdmin()) {
            g_rollback.createSnapshot(std::format("Auto-save before deleting {} ({})",
                name, sys ? "system" : "user"));
        }

        bool ok = lazyenv::RollbackManager::deleteEnvVariable(name, sys);
        if (ok) {
            lazyenv::RollbackManager::broadcastEnvironmentChange();
        }

        return std::format(
            "{{\"action\":\"envVarDeleteResult\",\"success\":{},\"name\":\"{}\","
            "\"message\":\"{}\"}}",
            ok ? "true" : "false", jsonEscape(name),
            ok ? "" : jsonEscape("Failed to delete environment variable. "
                                 "Ensure you have sufficient privileges."));
    }

    // ------ Read env var ------
    if (action == "readEnv") {
        std::string name = extractJsonValue(message, "name");
        std::string systemStr = extractJsonValue(message, "system");
        bool sys = (systemStr == "true");
        std::string val = lazyenv::RollbackManager::readEnvVariable(name, sys);
        return std::format("{{\"action\":\"envValue\",\"name\":\"{}\",\"value\":\"{}\"}}",
                           jsonEscape(name), jsonEscape(val));
    }

    // ------ Select folder (browse) ------
    if (action == "selectFolder") {
        IFileOpenDialog* pfd = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr,
                                       CLSCTX_INPROC_SERVER,
                                       IID_PPV_ARGS(&pfd));
        if (SUCCEEDED(hr)) {
            DWORD opt = 0;
            pfd->GetOptions(&opt);
            pfd->SetOptions(opt | FOS_PICKFOLDERS);
            if (SUCCEEDED(pfd->Show(g_mainWindow))) {
                IShellItem* psi = nullptr;
                if (SUCCEEDED(pfd->GetResult(&psi))) {
                    PWSTR wpath = nullptr;
                    psi->GetDisplayName(SIGDN_FILESYSPATH, &wpath);
                    if (wpath) {
                        int len = WideCharToMultiByte(CP_UTF8, 0, wpath, -1,
                                                      nullptr, 0, nullptr, nullptr);
                        std::string path(len - 1, '\0');
                        WideCharToMultiByte(CP_UTF8, 0, wpath, -1,
                                           path.data(), len, nullptr, nullptr);
                        CoTaskMemFree(wpath);
                        psi->Release();
                        pfd->Release();
                        return std::format("{{\"action\":\"folderSelected\",\"path\":\"{}\"}}",
                                           jsonEscape(path));
                    }
                    psi->Release();
                }
            }
            pfd->Release();
        }
        return ""; // User cancelled — silent, no notification needed
    }

    return "{\"action\":\"error\",\"message\":\"Unknown action\"}";

    } catch (const std::exception& e) {
        return std::format("{{\"action\":\"error\",\"message\":\"{}\"}}",
                           jsonEscape(e.what()));
    } catch (...) {
        return "{\"action\":\"error\",\"message\":\"Unexpected internal error\"}";
    }
}

// ---------------------------------------------------------------------------
// Vectored Exception Handler — fires BEFORE any SEH __try/__except.
//
// Logs ACCESS_VIOLATION details for diagnostic purposes, then lets the
// exception propagate to the inner SEH __try/__except handler.
//
// IMPORTANT: We do NOT use EXCEPTION_CONTINUE_EXECUTION because it resumes
// execution at the faulting instruction with the same corrupted register
// state. This is extremely dangerous:
//   - Stack-local variables (including std::mutex internals) may have been
//     zeroed/corrupted by a prior AV's stack smash.
//   - Resuming leads to cascading failures like mtx_do_lock(0x00000000).
//
// Instead we return EXCEPTION_CONTINUE_SEARCH to let the nearest
// __try/__except catch it cleanly, which unwinds to a known-safe point.
// ---------------------------------------------------------------------------
static LONG CALLBACK vectoredExceptionHandler(_In_ EXCEPTION_POINTERS* ep) {
    DWORD code = ep->ExceptionRecord->ExceptionCode;
    if (code != EXCEPTION_ACCESS_VIOLATION)
        return EXCEPTION_CONTINUE_SEARCH;

    char buf[256];
    snprintf(buf, sizeof(buf),
             "LazyEnv VeH: ACCESS_VIOLATION at 0x%p, thread=%lu\n",
             ep->ExceptionRecord->ExceptionAddress,
             GetCurrentThreadId());
    OutputDebugStringA(buf);

    // Let inner SEH handlers deal with it properly via stack unwind
    return EXCEPTION_CONTINUE_SEARCH;
}

// ---------------------------------------------------------------------------
// Window procedure (inner) — all real WndProc logic.
// Does NOT use __try to avoid C2712 conflict with C++ local objects.
// ---------------------------------------------------------------------------
static LRESULT WndProcImpl(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        // Extend frame into client area for shadow effect
        MARGINS margins = { 0, 0, 0, 1 };
        DwmExtendFrameIntoClientArea(hwnd, &margins);

        // Enable dark mode title bar (for shadow color)
        BOOL darkMode = TRUE;
        DwmSetWindowAttribute(hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */,
                              &darkMode, sizeof(darkMode));
        return 0;
    }

    case WM_NCCALCSIZE: {
        if (wParam == TRUE) {
            auto* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
            if (g_isMaximized) {
                HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
                MONITORINFO mi{};
                mi.cbSize = sizeof(mi);
                GetMonitorInfoW(mon, &mi);
                params->rgrc[0] = mi.rcWork;
            }
            return 0;
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    case WM_NCHITTEST: {
        // Only handle resize edges at the native level.
        // Title-bar dragging is handled via JS -> windowDragStart message.
        POINT pt = { static_cast<int>(static_cast<short>(LOWORD(lParam))),
                     static_cast<int>(static_cast<short>(HIWORD(lParam))) };
        ScreenToClient(hwnd, &pt);
        RECT rc;
        GetClientRect(hwnd, &rc);

        constexpr int border = 6;
        bool onLeft   = pt.x < border;
        bool onRight  = pt.x >= rc.right - border;
        bool onTop    = pt.y < border;
        bool onBottom = pt.y >= rc.bottom - border;

        if (onTop && onLeft)     return HTTOPLEFT;
        if (onTop && onRight)    return HTTOPRIGHT;
        if (onBottom && onLeft)  return HTBOTTOMLEFT;
        if (onBottom && onRight) return HTBOTTOMRIGHT;
        if (onLeft)              return HTLEFT;
        if (onRight)             return HTRIGHT;
        if (onTop)               return HTTOP;
        if (onBottom)            return HTBOTTOM;

        return HTCLIENT;
    }

    case WM_SIZE: {
        if (wParam == SIZE_MINIMIZED) {
            g_wasMinimized = true;
        } else if (wParam == SIZE_RESTORED && g_wasMinimized) {
            g_wasMinimized = false;
            // After restoring from minimized, re-apply fake-maximize position
            // so the bottom of the window isn't hidden behind the taskbar.
            // Deferred via PostMessage to let the current restore sequence finish.
            if (g_isMaximized) {
                PostMessageW(hwnd, lazyenv::WM_REAPPLY_MAXIMIZE, 0, 0);
            }
        }
        g_webview.resize();
        if (g_webview.getController()) {
            int cw = LOWORD(lParam);  // client area width
            int ch = HIWORD(lParam);  // client area height
            std::string stateMsg = std::format(
                "{{\"action\":\"windowState\",\"maximized\":{},\"width\":{},\"height\":{}}}",
                g_isMaximized ? "true" : "false", cw, ch);
            g_webview.postMessage(stateMsg);
        }
        return 0;
    }

    case WM_DPICHANGED: {
        // lParam points to a RECT with the suggested new window size/position
        // for the new DPI. Apply it, then resize WebView2 so its scale updates.
        auto* const suggestedRect = reinterpret_cast<RECT*>(lParam);
        SetWindowPos(hwnd, nullptr,
                     suggestedRect->left,
                     suggestedRect->top,
                     suggestedRect->right - suggestedRect->left,
                     suggestedRect->bottom - suggestedRect->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        g_webview.resize();
        return 0;
    }

    case WM_GETMINMAXINFO: {
        auto* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
        mmi->ptMinTrackSize.x = 800;
        mmi->ptMinTrackSize.y = 600;

        // Constrain maximized size to the work area (excludes taskbar)
        HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi{};
        mi.cbSize = sizeof(mi);
        if (GetMonitorInfoW(mon, &mi)) {
            mmi->ptMaxPosition.x = mi.rcWork.left - mi.rcMonitor.left;
            mmi->ptMaxPosition.y = mi.rcWork.top  - mi.rcMonitor.top;
            mmi->ptMaxSize.x     = mi.rcWork.right  - mi.rcWork.left;
            mmi->ptMaxSize.y     = mi.rcWork.bottom - mi.rcWork.top;
        }
        return 0;
    }

    case WM_ACTIVATE: {
        MARGINS margins = { 0, 0, 0, 1 };
        DwmExtendFrameIntoClientArea(hwnd, &margins);
        return 0;
    }

    // WebView2 thread-safe message delivery
    // postMessage() allocates a std::string* and posts it via lParam.
    case lazyenv::WM_WEBVIEW_POST_MESSAGE: {
        try {
            auto* msg = reinterpret_cast<std::string*>(lParam);
            if (msg) {
                g_webview.deliverMessage(*msg);
            }
            delete msg;
        } catch (const std::exception& e) {
            // PostWebMessageAsString or JSON conversion threw — log and swallow
            OutputDebugStringA(("WM_WEBVIEW_POST_MESSAGE error: " + std::string(e.what()) + "\n").c_str());
        } catch (...) {
            OutputDebugStringA("WM_WEBVIEW_POST_MESSAGE: unknown error\n");
        }
        return 0;
    }

    // Deferred drag start (posted from WebView2 callback context)
    case lazyenv::WM_WEBVIEW_DRAG_START: {
        ReleaseCapture();
        SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        return 0;
    }

    case lazyenv::WM_REAPPLY_MAXIMIZE: {
        // Deferred re-apply after restore from minimized.
        // DwmFlush waits for the compositor to finish any in-flight
        // animation frames before we reposition.  Combined with
        // DWMWA_TRANSITIONS_FORCEDISABLED, this is a belt-and-suspenders
        // guarantee that the window ends up exactly at rcWork.
        if (g_isMaximized) {
            DwmFlush();
            HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi{};
            mi.cbSize = sizeof(mi);
            if (GetMonitorInfoW(mon, &mi)) {
                SetWindowPos(hwnd, nullptr,
                             mi.rcWork.left, mi.rcWork.top,
                             mi.rcWork.right  - mi.rcWork.left,
                             mi.rcWork.bottom - mi.rcWork.top,
                             SWP_NOZORDER | SWP_NOACTIVATE);
            }
        }
        return 0;
    }

    case WM_DESTROY: {
        // Drain any pending WM_WEBVIEW_POST_MESSAGE messages to avoid
        // leaking the heap-allocated std::string* objects.
        MSG msg;
        while (PeekMessageW(&msg, g_mainWindow, lazyenv::WM_WEBVIEW_POST_MESSAGE,
                            lazyenv::WM_WEBVIEW_POST_MESSAGE, PM_REMOVE)) {
            delete reinterpret_cast<std::string*>(msg.lParam);
        }
        PostQuitMessage(0);
        return 0;
    }

    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

// ---------------------------------------------------------------------------
// Window procedure (outer) — SEH wrapper with no C++ local objects.
//
// ACCESS_VIOLATION (C0000005) inside WndProcImpl is an SEH structured
// exception that C++ try-catch cannot see. This __try/__except wrapper
// catches it before it can escape into DispatchMessageW and become a
// fatal C000041D. C2712 (no C++ objects in __try functions) is satisfied
// because this function has none — all C++ code lives in WndProcImpl.
// ---------------------------------------------------------------------------
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    __try {
        return WndProcImpl(hwnd, msg, wParam, lParam);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        OutputDebugStringA("WndProc: SEH exception caught\n");
        return 0;
    }
}

// ---------------------------------------------------------------------------
// Process-wide unhandled exception filter — absolute last resort.
//
// If any exception escapes ALL __try/__except blocks and try/catch blocks,
// this filter fires before the process terminates. We log the exception
// code for diagnostics but let the default handler run (dump creation).
// This does NOT prevent the crash — it only aids debugging.
// ---------------------------------------------------------------------------
static LONG WINAPI unhandledExceptionFilter(_In_ EXCEPTION_POINTERS* ep) {
    char buf[128];
    snprintf(buf, sizeof(buf),
             "LazyEnv: Unhandled exception 0x%08X at 0x%p\n",
             ep->ExceptionRecord->ExceptionCode,
             ep->ExceptionRecord->ExceptionAddress);
    OutputDebugStringA(buf);
    return EXCEPTION_EXECUTE_HANDLER;  // let WER create dump + terminate
}






// Message loop helper — pure C signature, safe inside __try (avoids C2712).
// DispatchMessageW calls WndProc which may use C++ objects, but C2712 only
// inspects the function that directly contains __try.
static void dispatchOneMessage(_Inout_ MSG* msg) {
    TranslateMessage(msg);
    DispatchMessageW(msg);
}

// Message loop in its own function — no C++ local objects, safe for __try.
static void runMessageLoop() {
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        __try {
            dispatchOneMessage(&msg);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            OutputDebugStringA("Message loop: SEH exception caught\n");
        }
    }
}

// WinMain
// ---------------------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow) {
    // Install vectored exception handler — fires BEFORE any SEH or
    // unhandled exception filter. Catches ACCESS_VIOLATION at the
    // earliest possible point to prevent double-fault process death.
    PVOID vehHandle = AddVectoredExceptionHandler(1, vectoredExceptionHandler);

    // Install process-wide unhandled exception filter as the absolute
    // last resort. Any exception that escapes all __try/__except and
    // try/catch blocks will be logged here before WER creates a dump.
    SetUnhandledExceptionFilter(unhandledExceptionFilter);

    // Enable Per-Monitor DPI awareness (v2)
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // Initialize COM
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    // Check WebView2 runtime
    if (!lazyenv::WebViewHost::isRuntimeAvailable()) {
        MessageBoxW(nullptr,
            L"Microsoft Edge WebView2 Runtime is required but not found.\n"
            L"Please install it from:\n"
            L"https://developer.microsoft.com/en-us/microsoft-edge/webview2/",
            L"LazyEnv - Missing Dependency",
            MB_ICONERROR | MB_OK);
        return 1;
    }

    // Register window class
    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance      = hInstance;
    wc.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground  = CreateSolidBrush(RGB(25, 25, 25));
    wc.lpszClassName  = kWindowClass;
    wc.hIcon          = LoadIconW(hInstance, L"IDI_APPICON");
    wc.hIconSm        = LoadIconW(hInstance, L"IDI_APPICON");
    RegisterClassExW(&wc);

    // Create window (centered, 2/3 of work area)
    RECT workArea;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    int workW = workArea.right - workArea.left;
    int workH = workArea.bottom - workArea.top;
    int winW  = workW * 3 / 5;
    int winH  = workH * 4 / 5;
    int x = workArea.left + (workW - winW) / 2;
    int y = workArea.top  + (workH - winH) / 2;

    g_mainWindow = CreateWindowExW(
        0,
        kWindowClass,
        kWindowTitle,
        WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU,
        x, y, winW, winH,
        nullptr, nullptr, hInstance, nullptr);

    if (!g_mainWindow) {
        MessageBoxW(nullptr, L"Failed to create main window.", L"LazyEnv", MB_ICONERROR);
        return 1;
    }

    // Disable DWM window transition animations.  Without this, DWM's
    // restore-from-minimized animation runs asynchronously in the compositor
    // and can move the window so its bottom is hidden behind the taskbar.
    // With transitions disabled, the window instantly appears at its stored
    // (rcWork) position, which is always correct.
    BOOL disableTransitions = TRUE;
    DwmSetWindowAttribute(g_mainWindow, DWMWA_TRANSITIONS_FORCEDISABLED,
                          &disableTransitions, sizeof(disableTransitions));

    // Get file:// URI for the HTML page
    std::wstring htmlUri = getHtmlUri();

    // Initialize WebView2 with file:// navigation
    g_webview.setMessageHandler(handleWebMessage);
    if (!g_webview.initialize(g_mainWindow, htmlUri)) {
        MessageBoxW(nullptr, L"Failed to initialize WebView2.", L"LazyEnv", MB_ICONERROR);
        return 1;
    }

    ShowWindow(g_mainWindow, nCmdShow);
    UpdateWindow(g_mainWindow);

    // Run message loop with SEH protection.
    // ACCESS_VIOLATION (C0000005) on the UI thread cannot be caught by
    // C++ try-catch. __try/__except in runMessageLoop captures it so the
    // process keeps running instead of dying with C000041D.
    runMessageLoop();

    CoUninitialize();
    return 0;
}

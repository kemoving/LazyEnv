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
// LazyEnv - installer.cpp
// Package installation engine implementation
// ============================================================================

#include "installer.h"
#include "rollback.h"   // For environment variable helpers

#include <Windows.h>
#include <eh.h>         // __try/__except SEH

#include <sstream>
#include <algorithm>
#include <format>
#include <cstdio>

namespace lazyenv {

// ===========================================================================
// Installer
// ===========================================================================
Installer::Installer() = default;

void Installer::setProgressCallback(ProgressCallback cb) {
    progressCb_ = std::move(cb);
}

// ---------------------------------------------------------------------------
// winget availability check
// ---------------------------------------------------------------------------
bool Installer::isWingetAvailable() {
    std::string output;
    int rc = runCommand("winget --version", output, 10000);
    return rc == 0 && !output.empty();
}

// ---------------------------------------------------------------------------
// Check whether a specific package ID is already installed
// ---------------------------------------------------------------------------
bool Installer::isPackageInstalled(const std::string& packageId) {
    std::string output;
    std::string cmd = "winget list --id \"" + packageId + "\" --exact --accept-source-agreements";
    int rc = runCommand(cmd, output, 15000);
    // winget returns 0 and lists the package if installed
    return rc == 0 && output.find(packageId) != std::string::npos;
}

// ---------------------------------------------------------------------------
// Single package install
// ---------------------------------------------------------------------------
InstallResult Installer::installPackage(const PackageInfo& pkg,
                                       const std::string& installLocation) {
    InstallResult result;
    result.packageId = pkg.id;
    result.status    = InstallStatus::Running;

    if (progressCb_)
        progressCb_(pkg.id, InstallStatus::Running,
                    std::string("Installing ") + pkg.displayName + "...");

    // Build winget command
    std::string cmd = std::format(
        "winget install --id {} --exact --silent "
        "--accept-package-agreements --accept-source-agreements",
        pkg.id);

    // Append custom install location if specified.
    // Each package gets its own subfolder under the user-provided base path.
    std::string effectiveLocation;
    if (!installLocation.empty()) {
        effectiveLocation = makePackageInstallLocation(installLocation,
                                                       pkg.displayName);
        cmd += std::format(" --location \"{}\"", effectiveLocation);
    }

    result.exitCode = runCommand(cmd, result.output, 600000); // 10 min timeout

    if (result.exitCode == 0) {
        result.status = InstallStatus::Success;

        // Add to PATH if requested.
        // When a custom installLocation is provided, use the package-specific
        // subfolder as the PATH entry (executables live inside that folder).
        if (pkg.addToPath) {
            if (!effectiveLocation.empty()) {
                addToUserPath(effectiveLocation);
            } else if (!pkg.defaultPath.empty()) {
                addToUserPath(pkg.defaultPath);
            }
        }

        if (progressCb_)
            progressCb_(pkg.id, InstallStatus::Success,
                        pkg.displayName + " installed successfully.");
    } else {
        // Check if already installed (winget returns specific codes)
        if (result.output.find("already installed") != std::string::npos ||
            result.output.find("No available upgrade") != std::string::npos) {
            result.status = InstallStatus::Skipped;
            if (progressCb_)
                progressCb_(pkg.id, InstallStatus::Skipped,
                            pkg.displayName + " is already installed.");
        } else {
            result.status = InstallStatus::Failed;
            if (progressCb_)
                progressCb_(pkg.id, InstallStatus::Failed,
                            std::format("{} installation failed (exit code {}).",
                                        pkg.displayName, result.exitCode));
        }
    }
    return result;
}

// ---------------------------------------------------------------------------
// Batch install
// ---------------------------------------------------------------------------
std::vector<InstallResult> Installer::installBatch(
    const std::vector<PackageInfo>& packages,
    const std::string& installLocation) {

    std::vector<InstallResult> results;
    results.reserve(packages.size());
    for (auto& pkg : packages) {
        results.push_back(installPackage(pkg, installLocation));
    }
    return results;
}

// ---------------------------------------------------------------------------
// PATH management
// ---------------------------------------------------------------------------
bool Installer::addToUserPath(const std::string& directory) {
    std::string currentPath = RollbackManager::readEnvVariable("Path", false);

    // Normalize: ensure no trailing backslash for comparison
    std::string normalized = directory;
    while (!normalized.empty() && (normalized.back() == '\\' || normalized.back() == '/'))
        normalized.pop_back();

    // Check if already present (case-insensitive)
    std::string lowerPath = currentPath;
    std::string lowerDir  = normalized;
    std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::transform(lowerDir.begin(), lowerDir.end(), lowerDir.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (lowerPath.find(lowerDir) != std::string::npos)
        return true; // Already present

    // Append
    std::string newPath = currentPath;
    if (!newPath.empty() && newPath.back() != ';')
        newPath += ';';
    newPath += normalized;

    bool ok = RollbackManager::writeEnvVariable("Path", newPath, REG_EXPAND_SZ, false);
    if (ok)
        RollbackManager::broadcastEnvironmentChange();
    return ok;
}

bool Installer::removeFromUserPath(const std::string& directory) {
    std::string currentPath = RollbackManager::readEnvVariable("Path", false);

    std::string normalized = directory;
    while (!normalized.empty() && (normalized.back() == '\\' || normalized.back() == '/'))
        normalized.pop_back();

    // Case-insensitive removal
    std::string lowerPath = currentPath;
    std::string lowerDir  = normalized;
    std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::transform(lowerDir.begin(), lowerDir.end(), lowerDir.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    auto pos = lowerPath.find(lowerDir);
    if (pos == std::string::npos)
        return true; // Not present, nothing to do

    // Remove the entry and surrounding semicolons
    size_t end = pos + normalized.size();
    if (end < currentPath.size() && currentPath[end] == ';') ++end;
    else if (pos > 0 && currentPath[pos - 1] == ';') --pos;

    std::string newPath = currentPath.substr(0, pos) + currentPath.substr(end);

    bool ok = RollbackManager::writeEnvVariable("Path", newPath, REG_EXPAND_SZ, false);
    if (ok)
        RollbackManager::broadcastEnvironmentChange();
    return ok;
}

bool Installer::isCommandAvailable(const std::string& command) {
    std::string output;
    std::string cmd = "where " + command;
    int rc = runCommand(cmd, output, 5000);
    return rc == 0 && !output.empty();
}

// ---------------------------------------------------------------------------
// Install location resolution
// ---------------------------------------------------------------------------
std::string makePackageInstallLocation(const std::string& baseLocation,
                                       const std::string& packageDisplayName) {
    if (baseLocation.empty())
        return "";

    // Sanitize display name for use as a Windows directory name.
    std::string folder = packageDisplayName.empty() ? "package" : packageDisplayName;
    for (auto& c : folder) {
        if (c == '\\' || c == '/' || c == ':' || c == '*' ||
            c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            c = '_';
        }
    }
    // Windows forbids trailing spaces and periods in directory names.
    while (!folder.empty() && (folder.back() == ' ' || folder.back() == '.'))
        folder.pop_back();
    if (folder.empty())
        folder = "package";

    // Normalize base: remove trailing path separators.
    std::string normalized = baseLocation;
    while (!normalized.empty() &&
           (normalized.back() == '\\' || normalized.back() == '/')) {
        normalized.pop_back();
    }

    return normalized + "\\" + folder;
}

// ---------------------------------------------------------------------------
// Command execution helpers (shared pipe setup)
// ---------------------------------------------------------------------------
namespace {

struct PipeHandles {
    HANDLE hRead  = nullptr;
    HANDLE hWrite = nullptr;
};

bool createOutputPipe(PipeHandles& ph) {
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    if (!CreatePipe(&ph.hRead, &ph.hWrite, &sa, 0))
        return false;
    SetHandleInformation(ph.hRead, HANDLE_FLAG_INHERIT, 0);
    return true;
}

bool launchProcess(const std::string& cmdLine, HANDLE hWritePipe,
                   PROCESS_INFORMATION& pi) {
    STARTUPINFOW si{};
    si.cb         = sizeof(si);
    si.dwFlags    = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hWritePipe;
    si.hStdError  = hWritePipe;
    si.wShowWindow = SW_HIDE;

    // Launch the command directly without cmd /c wrapper.
    // cmd.exe strips quotes from arguments when launching child processes,
    // which breaks paths containing spaces (e.g. --location "E:\Program Files").
    // winget and where are standalone executables; CreateProcessW searches PATH.
    int wlen = MultiByteToWideChar(CP_UTF8, 0, cmdLine.c_str(),
                                   static_cast<int>(cmdLine.size()), nullptr, 0);
    if (wlen <= 0) return false;
    std::wstring wcmd(wlen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, cmdLine.c_str(),
                        static_cast<int>(cmdLine.size()), wcmd.data(), wlen);

    return CreateProcessW(
        nullptr, wcmd.data(),
        nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW,
        nullptr, nullptr,
        &si, &pi) != 0;
}

// ---------------------------------------------------------------------------
// SEH-safe core: ReadFile loop + WaitForSingleObject.
// This function contains NO C++ objects with destructors — safe for __try.
// ---------------------------------------------------------------------------
struct RawExecResult {
    char  outputData[65536];  // fixed-size buffer, no heap allocation
    DWORD outputLen;
    DWORD exitCode;
    bool  success;
};

// Forward-declare the SEH wrapper (no C++ objects, pure C signatures)
static void sehReadPipeLoop(HANDLE hRead, RawExecResult* result);

// SEH-protected ReadFile loop — pure C, no C++ objects
static void sehReadPipeLoop(HANDLE hRead, RawExecResult* result) {
    __try {
        char buf[4096];
        DWORD bytesRead = 0;
        result->outputLen = 0;
        DWORD bufSize = static_cast<DWORD>(sizeof(buf) - 1);
        DWORD maxOut = static_cast<DWORD>(sizeof(result->outputData) - 1);

        while (ReadFile(hRead, buf, bufSize, &bytesRead, nullptr) && bytesRead > 0) {
            if (bytesRead > bufSize) bytesRead = bufSize;
            // Avoid overflow: clamp to remaining space
            if (result->outputLen + bytesRead > maxOut)
                bytesRead = maxOut - result->outputLen;
            if (bytesRead == 0) break;
            memcpy(result->outputData + result->outputLen, buf, bytesRead);
            result->outputLen += bytesRead;
        }
        result->outputData[result->outputLen] = '\0';
        result->success = true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        // DO NOT touch result->outputData[result->outputLen] — if the
        // original AV corrupted outputLen or the buffer pointer, writing
        // here would trigger a second ACCESS_VIOLATION inside the handler
        // (double fault), which kills the process immediately.
        result->success = false;
    }
}

// Read-after-SEH: wait for process exit (no __try needed, stable after ReadFile)
static DWORD waitForProcessExit(HANDLE hProcess, DWORD timeoutMs) {
    WaitForSingleObject(hProcess, timeoutMs);
    DWORD exitCode = 0;
    GetExitCodeProcess(hProcess, &exitCode);
    return exitCode;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Blocking command execution with full output capture
//
// All Win32 I/O (CreateProcess + ReadFile pipe loop) runs inside __try/__except
// via sehReadPipeLoop. This prevents ACCESS_VIOLATION inside pipe I/O from
// escaping and corrupting the call stack, which would cause a double fault
// in the outer __except handler (sehThreadProc).
// ---------------------------------------------------------------------------
int Installer::runCommand(const std::string& cmdLine,
                          std::string& output,
                          uint32_t timeoutMs) {
    output.clear();

    PipeHandles ph;
    if (!createOutputPipe(ph)) return -1;

    PROCESS_INFORMATION pi{};
    if (!launchProcess(cmdLine, ph.hWrite, pi)) {
        CloseHandle(ph.hRead);
        CloseHandle(ph.hWrite);
        return -1;
    }
    CloseHandle(ph.hWrite);  // child inherits the write end

    // SEH-safe ReadFile loop — uses fixed-size stack buffer, no heap alloc
    RawExecResult raw{};
    sehReadPipeLoop(ph.hRead, &raw);
    CloseHandle(ph.hRead);

    if (!raw.success) {
        // SEH caught an ACCESS_VIOLATION during pipe read.
        // outputLen may have been corrupted by the fault — clamp it to
        // the actual buffer size before using it.
        const DWORD kMaxBuf = static_cast<DWORD>(sizeof(raw.outputData));
        if (raw.outputLen > 0 && raw.outputLen <= kMaxBuf)
            output.assign(raw.outputData, raw.outputLen);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return -2;  // special code: SEH exception during I/O
    }

    // Copy output from fixed buffer to std::string (safe: no I/O, only memory)
    if (raw.outputLen > 0)
        output.assign(raw.outputData, raw.outputLen);

    DWORD exitCode = waitForProcessExit(pi.hProcess, timeoutMs);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return static_cast<int>(exitCode);
}

// ---------------------------------------------------------------------------
// Streaming command execution with line-by-line callback
//
// Uses the same SEH-safe batch-read (sehReadPipeLoop) as runCommand.
// Line splitting and onLine callbacks happen AFTER the pipe is closed,
// in a clean C++ context with no SEH risk.
// ---------------------------------------------------------------------------
int Installer::runCommandStreaming(const std::string& cmdLine,
                                  std::string& fullOutput,
                                  LineCallback onLine,
                                  uint32_t timeoutMs) {
    fullOutput.clear();

    PipeHandles ph;
    if (!createOutputPipe(ph)) return -1;

    PROCESS_INFORMATION pi{};
    if (!launchProcess(cmdLine, ph.hWrite, pi)) {
        CloseHandle(ph.hRead);
        CloseHandle(ph.hWrite);
        return -1;
    }
    CloseHandle(ph.hWrite);

    // SEH-safe read into a fixed-size stack buffer (same approach as runCommand)
    RawExecResult raw{};
    sehReadPipeLoop(ph.hRead, &raw);
    CloseHandle(ph.hRead);

    if (!raw.success) {
        // SEH caught an exception during I/O — save what we got.
        const DWORD kMaxBuf = static_cast<DWORD>(sizeof(raw.outputData));
        if (raw.outputLen > 0 && raw.outputLen <= kMaxBuf) {
            fullOutput.assign(raw.outputData, raw.outputLen);
        }
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return -2;
    }

    fullOutput.assign(raw.outputData, raw.outputLen);

    // Split raw output into lines and invoke callback (no I/O, safe)
    if (onLine) {
        std::string lineBuffer;
        for (DWORD i = 0; i < raw.outputLen; ++i) {
            if (raw.outputData[i] == '\n') {
                if (!lineBuffer.empty() && lineBuffer.back() == '\r')
                    lineBuffer.pop_back();
                if (!lineBuffer.empty()) onLine(lineBuffer);
                lineBuffer.clear();
            } else {
                lineBuffer += raw.outputData[i];
            }
        }
        if (!lineBuffer.empty()) {
            if (lineBuffer.back() == '\r') lineBuffer.pop_back();
            if (!lineBuffer.empty()) onLine(lineBuffer);
        }
    }

    DWORD exitCode = waitForProcessExit(pi.hProcess, timeoutMs);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return static_cast<int>(exitCode);
}

// ===========================================================================
// Default package catalog
// ===========================================================================
std::vector<PackageInfo> getDefaultCatalog() {
    return {
        // --- Languages ---
        {"Python.Python.3.12", "Python 3.12", "language",
         "General-purpose programming language",
         "C:\\Python312;C:\\Python312\\Scripts", true},

        {"OpenJS.NodeJS.LTS", "Node.js (LTS)", "language",
         "JavaScript runtime built on V8",
         "", true},

        {"Rustlang.Rustup", "Rust (rustup)", "language",
         "Systems programming language",
         "%USERPROFILE%\\.cargo\\bin", true},

        {"GoLang.Go", "Go", "language",
         "Statically typed compiled language by Google",
         "C:\\Program Files\\Go\\bin", true},

        {"Oracle.JDK.21", "Java JDK 21", "language",
         "Java Development Kit",
         "", true},

        // --- Build tools ---
        {"Kitware.CMake", "CMake", "tool",
         "Cross-platform build system generator",
         "", true},

        {"Ninja-build.Ninja", "Ninja", "tool",
         "Small, fast build system",
         "", true},

        {"LLVM.LLVM", "LLVM/Clang", "tool",
         "Compiler infrastructure and C/C++ compiler",
         "C:\\Program Files\\LLVM\\bin", true},

        // --- Version control ---
        {"Git.Git", "Git", "tool",
         "Distributed version control system",
         "C:\\Program Files\\Git\\cmd", true},

        {"GitHub.cli", "GitHub CLI", "tool",
         "GitHub command-line tool",
         "", true},

        // --- Editors ---
        {"Microsoft.VisualStudioCode", "VS Code", "editor",
         "Lightweight code editor by Microsoft",
         "", false},

        {"Neovim.Neovim", "Neovim", "editor",
         "Hyperextensible Vim-based text editor",
         "", true},

        // --- Containers & VMs ---
        {"Docker.DockerDesktop", "Docker Desktop", "runtime",
         "Container platform for development",
         "", false},

        // --- Databases ---
        {"PostgreSQL.PostgreSQL.16", "PostgreSQL 16", "database",
         "Advanced open-source relational database",
         "C:\\Program Files\\PostgreSQL\\16\\bin", true},

        {"Redis.Redis", "Redis", "database",
         "In-memory data structure store",
         "", true},

        // --- Utilities ---
        {"jqlang.jq", "jq", "utility",
         "Lightweight command-line JSON processor",
         "", true},

        {"BurntSushi.ripgrep.MSVC", "ripgrep", "utility",
         "Blazingly fast grep alternative",
         "", true},

        {"sharkdp.fd", "fd", "utility",
         "Fast and user-friendly find alternative",
         "", true},

        {"junegunn.fzf", "fzf", "utility",
         "Command-line fuzzy finder",
         "", true},

        {"WinSCP.WinSCP", "WinSCP", "utility",
         "SFTP/SCP/FTP client for Windows",
         "", false},
    };
}

} // namespace lazyenv

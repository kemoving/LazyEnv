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

#include <sstream>
#include <algorithm>
#include <format>

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
// Single package install
// ---------------------------------------------------------------------------
InstallResult Installer::installPackage(const PackageInfo& pkg) {
    InstallResult result;
    result.packageId = pkg.id;
    result.status    = InstallStatus::Running;

    if (progressCb_)
        progressCb_(pkg.id, InstallStatus::Running,
                    "Installing " + pkg.displayName + "...");

    // Build winget command
    std::string cmd = std::format(
        "winget install --id {} --exact --silent "
        "--accept-package-agreements --accept-source-agreements",
        pkg.id);

    result.exitCode = runCommand(cmd, result.output, 600000); // 10 min timeout

    if (result.exitCode == 0) {
        result.status = InstallStatus::Success;

        // Add to PATH if requested
        if (pkg.addToPath && !pkg.defaultPath.empty()) {
            addToUserPath(pkg.defaultPath);
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
    const std::vector<PackageInfo>& packages) {

    std::vector<InstallResult> results;
    results.reserve(packages.size());
    for (auto& pkg : packages) {
        results.push_back(installPackage(pkg));
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

    std::string fullCmd = "cmd /c " + cmdLine;
    int wlen = MultiByteToWideChar(CP_UTF8, 0, fullCmd.c_str(),
                                   static_cast<int>(fullCmd.size()), nullptr, 0);
    std::wstring wcmd(wlen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, fullCmd.c_str(),
                        static_cast<int>(fullCmd.size()), wcmd.data(), wlen);

    return CreateProcessW(
        nullptr, wcmd.data(),
        nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW,
        nullptr, nullptr,
        &si, &pi) != 0;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Blocking command execution with full output capture
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
    CloseHandle(ph.hWrite);

    // Read all output
    char buf[4096];
    DWORD bytesRead = 0;
    DWORD bufSize = static_cast<DWORD>(sizeof(buf) - 1);
    while (ReadFile(ph.hRead, buf, bufSize, &bytesRead, nullptr) && bytesRead > 0) {
        if (bytesRead > bufSize) bytesRead = bufSize;  // defensive: never overflow
        buf[bytesRead] = '\0';
        output += buf;
    }
    CloseHandle(ph.hRead);

    WaitForSingleObject(pi.hProcess, timeoutMs);

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return static_cast<int>(exitCode);
}

// ---------------------------------------------------------------------------
// Streaming command execution with line-by-line callback
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

    // Read output and split into lines, calling onLine for each
    std::string lineBuffer;
    char buf[1024];
    DWORD bytesRead = 0;
    DWORD bufSize = static_cast<DWORD>(sizeof(buf) - 1);

    while (ReadFile(ph.hRead, buf, bufSize, &bytesRead, nullptr) && bytesRead > 0) {
        if (bytesRead > bufSize) bytesRead = bufSize;  // defensive
        buf[bytesRead] = '\0';
        fullOutput += buf;

        // Split into lines
        for (DWORD i = 0; i < bytesRead; ++i) {
            if (buf[i] == '\n') {
                // Trim trailing \r
                if (!lineBuffer.empty() && lineBuffer.back() == '\r')
                    lineBuffer.pop_back();
                if (onLine && !lineBuffer.empty())
                    onLine(lineBuffer);
                lineBuffer.clear();
            } else {
                lineBuffer += buf[i];
            }
        }
    }

    // Flush remaining partial line
    if (!lineBuffer.empty()) {
        if (lineBuffer.back() == '\r') lineBuffer.pop_back();
        if (onLine && !lineBuffer.empty())
            onLine(lineBuffer);
    }

    CloseHandle(ph.hRead);

    WaitForSingleObject(pi.hProcess, timeoutMs);

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
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

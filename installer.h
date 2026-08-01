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

#pragma once
// ============================================================================
// LazyEnv - installer.h
// Package installation engine (winget backend) with PATH management
// ============================================================================

#ifndef LAZYENV_INSTALLER_H
#define LAZYENV_INSTALLER_H

#include <string>
#include <vector>
#include <functional>
#include <cstdint>

namespace lazyenv {

// ----------------------------------------------------------------------------
// Package definition
// ----------------------------------------------------------------------------
struct PackageInfo {
    std::string id;           // winget package ID, e.g. "Python.Python.3.12"
    std::string displayName;  // Human-readable name
    std::string category;     // "language", "tool", "runtime", etc.
    std::string description;
    std::string defaultPath;  // Expected install path (for PATH entry)
    bool        addToPath;    // Whether to add defaultPath to user PATH
};

// ----------------------------------------------------------------------------
// Installation progress / result
// ----------------------------------------------------------------------------
enum class InstallStatus {
    Pending,
    Running,
    Success,
    Failed,
    Skipped
};

struct InstallResult {
    std::string   packageId;
    InstallStatus status;
    int           exitCode;
    std::string   output;     // Combined stdout + stderr
};

// Progress callback: (packageId, status, progressMessage)
using ProgressCallback = std::function<void(const std::string&,
                                            InstallStatus,
                                            const std::string&)>;

// Line callback: called for each line of output during streaming execution.
using LineCallback = std::function<void(const std::string& line)>;

// ----------------------------------------------------------------------------
// Installer
// ----------------------------------------------------------------------------
class Installer {
public:
    Installer();

    // Set callback for real-time progress updates.
    void setProgressCallback(ProgressCallback cb);

    // Check whether winget is available on this system.
    static bool isWingetAvailable();

    // Check whether a specific package is already installed (by winget ID).
    static bool isPackageInstalled(const std::string& packageId);

    // Install a single package via winget.
    // If installLocation is non-empty, winget --location is used to control
    // where the package is installed.
    InstallResult installPackage(const PackageInfo& pkg,
                                 const std::string& installLocation = "");

    // Install a batch of packages sequentially.
    std::vector<InstallResult> installBatch(const std::vector<PackageInfo>& packages,
                                           const std::string& installLocation = "");

    // Append a directory to the user PATH (registry-level, persistent).
    // Returns true if the path was added (or already present).
    static bool addToUserPath(const std::string& directory);

    // Remove a directory from the user PATH.
    static bool removeFromUserPath(const std::string& directory);

    // Check if a command is reachable via current PATH.
    static bool isCommandAvailable(const std::string& command);

    // Run an arbitrary command and capture output (blocking).
    static int runCommand(const std::string& cmdLine,
                          std::string& output,
                          uint32_t timeoutMs = 300000);

    // Run a command with streaming line-by-line output via callback.
    static int runCommandStreaming(const std::string& cmdLine,
                                  std::string& fullOutput,
                                  LineCallback onLine,
                                  uint32_t timeoutMs = 300000);

private:
    ProgressCallback progressCb_;
};

// ----------------------------------------------------------------------------
// Install location helpers
// ----------------------------------------------------------------------------

// Resolve a base install location into a package-specific directory.
// When baseLocation is empty, returns empty. Otherwise returns
// baseLocation\<sanitizedDisplayName> so each package gets its own folder.
std::string makePackageInstallLocation(const std::string& baseLocation,
                                       const std::string& packageDisplayName);

// ----------------------------------------------------------------------------
// Built-in package catalog
// Provides a curated list of common development tools.
// ----------------------------------------------------------------------------
std::vector<PackageInfo> getDefaultCatalog();

} // namespace lazyenv

#endif // LAZYENV_INSTALLER_H

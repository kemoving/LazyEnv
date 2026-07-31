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
// LazyEnv - rollback.h
// Environment variable snapshot & rollback engine
// ============================================================================

#ifndef LAZYENV_ROLLBACK_H
#define LAZYENV_ROLLBACK_H

#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <filesystem>

namespace lazyenv {

// ----------------------------------------------------------------------------
// Snapshot metadata
// ----------------------------------------------------------------------------
struct SnapshotEntry {
    std::string name;
    std::string value;
    uint32_t    type;   // REG_SZ, REG_EXPAND_SZ, etc.
};

struct Snapshot {
    std::string                id;          // UUID
    std::string                timestamp;   // ISO-8601
    std::string                description;
    std::vector<SnapshotEntry> user_env;
    std::vector<SnapshotEntry> system_env;
};

// ----------------------------------------------------------------------------
// RollbackManager
// Manages environment variable snapshots stored as JSON files.
// Storage path: %LOCALAPPDATA%/LazyEnv/snapshots/
// ----------------------------------------------------------------------------
class RollbackManager {
public:
    RollbackManager();

    // Take a snapshot of current environment variables (user + system).
    // Returns the snapshot ID on success.
    std::string createSnapshot(const std::string& description = "");

    // List all available snapshots (newest first).
    std::vector<Snapshot> listSnapshots() const;

    // Restore environment from a specific snapshot.
    // Returns true on success.
    bool restoreSnapshot(const std::string& snapshotId);

    // Delete a snapshot file.
    bool deleteSnapshot(const std::string& snapshotId);

    // Export a snapshot to a user-specified file path.
    // Returns true on success.
    bool exportSnapshot(const std::string& snapshotId,
                        const std::string& destPath) const;

    // Import a snapshot from an external JSON file.
    // Returns the new snapshot ID on success, empty string on failure.
    std::string importSnapshot(const std::string& srcPath);

    // Export a single environment variable's current value (for diff display).
    static std::string readEnvVariable(const std::string& name, bool system);

    // Write a single environment variable to the registry.
    static bool writeEnvVariable(const std::string& name,
                                 const std::string& value,
                                 uint32_t type,
                                 bool system);

    // Delete a single environment variable from the registry.
    static bool deleteEnvVariable(const std::string& name, bool system);

    // Broadcast WM_SETTINGCHANGE so running processes pick up changes.
    static void broadcastEnvironmentChange();

private:
    std::filesystem::path storageDir_;

    // Capture all variables from a registry key.
    static std::vector<SnapshotEntry> captureRegistryKey(bool system);

    // Restore all variables from a snapshot entry list to a registry key.
    static bool restoreRegistryKey(const std::vector<SnapshotEntry>& entries,
                                   bool system);

    // Serialize / deserialize snapshot to/from JSON file.
    bool saveSnapshot(const Snapshot& snap) const;
    Snapshot loadSnapshot(const std::filesystem::path& path) const;

    // Generate a UUID-style identifier.
    static std::string generateId();

    // Get current ISO-8601 timestamp.
    static std::string currentTimestamp();
};

} // namespace lazyenv

#endif // LAZYENV_ROLLBACK_H

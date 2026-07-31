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
#include <unordered_set>
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

// ----------------------------------------------------------------------------
// Diff entry — 快照与当前注册表的差异项
// ----------------------------------------------------------------------------
struct DiffEntry {
    std::string name;
    std::string currentValue;   // 当前注册表中的值（空 = 注册表中不存在）
    std::string snapshotValue;  // 快照中的值（空 = 快照中不存在，将被删除）
    uint32_t    currentType = 0;
    uint32_t    snapshotType = 0;
    std::string changeType;     // "added" / "modified" / "removed"
    bool        system = false; // true = 系统作用域
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

    // Incremental restore: only update variables that differ from the
    // snapshot.  Variables added after snapshot are removed; deleted
    // ones are restored; unchanged ones are left untouched.
    // If `names` is non-empty, only those specific variables are restored.
    bool restoreSnapshotIncremental(const std::string& snapshotId,
        const std::vector<std::string>& names = {});

    // Compare a snapshot with the current registry and return all
    // differences (added / modified / removed variables).
    // Used by the UI to let users pick what to restore.
    std::vector<DiffEntry> diffSnapshot(const std::string& snapshotId) const;

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

    // Incremental restore: compare snapshot entries with current registry,
    // only update variables that differ.
    // If `filter` is non-null and non-empty, only those entries are processed;
    // deletions of non-snapshot entries are also skipped when filtered.
    static bool restoreRegistryKeyIncremental(
        const std::vector<SnapshotEntry>& entries, bool system,
        const std::unordered_set<std::string>* filter = nullptr);

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

/*
 * LazyEnv - Cross-platform, recoverable, zero-pollution dev environment configurator
 * Copyright (C) 2026 Rein
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

// ============================================================================
// LazyEnv - i18n.js
// Internationalization module with locale dictionaries and translation API.
// Supports: en, zh-CN
// ============================================================================

(function () {
    "use strict";

    // -----------------------------------------------------------------------
    // Language packs
    // -----------------------------------------------------------------------
    var locales = {

        // ===================================================================
        // English
        // ===================================================================
        "en": {
            // Titlebar
            "titlebar.label":                   "LazyEnv",
            "btn.minimize":                     "Minimize",
            "btn.maximize":                     "Maximize",
            "btn.close":                        "Close",

            // Sidebar
            "sidebar.search":                   "Search pages...",
            "sidebar.overview":                 "Overview",
            "sidebar.home":                     "Home",
            "sidebar.settings":                 "Settings",
            "sidebar.setup":                    "Setup",
            "sidebar.syscheck":                 "System Check",
            "sidebar.packages":                 "Packages",
            "sidebar.install":                  "Install",
            "sidebar.maintenance":              "Maintenance",
            "sidebar.recovery":                 "Recovery",
            "sidebar.summary":                  "Summary",
            "sidebar.info":                     "Info",
            "sidebar.about":                    "About",

            // Home
            "home.title":                       "Home",
            "home.desc":                        "Detected development environments on this machine.",
            "home.refresh":                     "Refresh",
            "home.addManual":                   "Add",
            "home.searchPlaceholder":           "Search environments...",
            "home.addTitle":                    "Manually add environment",
            "home.addCmdPlaceholder":           "e.g. python, node, rustc, gcc",
            "home.detect":                      "Detect",

            // Categories
            "category.language":                "Language / Runtime",
            "category.tool":                    "Build Tool",
            "category.runtime":                 "Container / VM",
            "category.utility":                 "Utility",
            "category.editor":                  "Editor / IDE",
            "category.database":                "Database",
            "category.other":                   "Other",

            // Environment cards
            "env.scanning":                     "Scanning system...",
            "env.noMatch":                      "No matching environments found.",
            "env.btnUninstall":                 "Uninstall",
            "env.btnOpenDir":                   "Open Folder",
            "env.confirmUninstallTitle":        "Confirm Uninstall",
            "env.confirmUninstall":             "Uninstall {0} via winget?",
            "env.uninstallSuccess":             "Uninstalled: {0}",
            "env.uninstallFailed":              "Uninstall failed.",

            // Probe
            "probe.detecting":                  "Detecting...",
            "probe.found":                      "Found: {0}",
            "probe.notFound":                   "Not found.",
            "probe.addedToast":                 "Added: {0} ({1})",

            // Settings (Environment Variable Editor)
            "settings.title":                   "Settings",
            "settings.desc":                    "Manage environment variables directly. Changes take effect immediately.",
            "settings.snapshotHint":            "A snapshot will be automatically created before any modification for safe rollback.",
            "settings.userVars":                "User Variables",
            "settings.systemVars":              "System Variables",
            "settings.scopeUser":               "User Variables",
            "settings.scopeSystem":             "System Variables",
            "settings.addNew":                  "New Variable",
            "settings.searchPlaceholder":       "Search variables...",
            "settings.colName":                 "Name",
            "settings.colValue":                "Value",
            "settings.colActions":              "Actions",
            "settings.loading":                 "Loading environment variables...",
            "settings.noMatch":                 "No matching variables found.",
            "settings.edit":                    "Edit",
            "settings.delete":                  "Delete",
            "settings.save":                    "Save",
            "settings.type":                    "Type",
            "settings.newVarTitle":             "New Variable",
            "settings.editVarTitle":            "Edit: {0}",
            "settings.viewVarTitle":            "View: {0}",
            "settings.viewScope":              "Scope",
            "settings.viewScopeUser":          "User",
            "settings.viewScopeSystem":        "System",
            "settings.viewClose":              "Close",
            "settings.nameRequired":            "Variable name is required.",
            "settings.pathAdd":                 "Add Entry",
            "settings.saveSuccess":             "Variable saved successfully.",
            "settings.saveFailed":              "Failed to save variable: {0}",
            "settings.deleteSuccess":           "Variable deleted successfully.",
            "settings.deleteFailed":            "Failed to delete variable: {0}",
            "settings.confirmDeleteTitle":      "Confirm Delete",
            "settings.confirmDelete":           "Delete environment variable \"{0}\"?",
            "settings.adminWarningShort":       "You are running without administrator privileges — system variables are read-only.",
            "settings.adminRequired":           "Administrator privileges required to modify system variables.",

            // System Check
            "syscheck.title":                   "System Check",
            "syscheck.desc":                    "Verify system prerequisites before installation.",
            "syscheck.rerun":                   "Re-run Checks",
            "check.os":                         "Operating System",
            "check.webview2":                   "WebView2 Runtime",
            "check.winget":                     "winget Package Manager",
            "check.detecting":                  "Detecting...",
            "check.available":                  "Available",
            "check.checking":                   "Checking...",
            "check.notFound":                   "Not found",

            // Packages
            "packages.title":                   "Packages",
            "packages.desc":                    "Select development tools to install.",
            "packages.selectAll":               "Select All",
            "packages.deselectAll":             "Deselect All",
            "packages.searchPlaceholder":       "Search packages...",
            "packages.startInstall":            "Start Installation",
            "packages.selectedCount":           "{0} selected",
            "packages.installed":               "Installed",
            "packages.alreadyInstalled":        "Already installed on system",
            "packages.installLocation":         "Install Location (leave empty for default)",
            "packages.installLocationPlaceholder": "e.g. D:\\Tools (each package gets its own subfolder)",
            "packages.locationHint":            "Tip: Use a non-system drive (e.g. D:\\) to keep C: clean. Each package is installed into its own subfolder.",
            "packages.noDriveSpaceAfterColon":  "Please include a drive letter and path (e.g. D:\\MyApps).",
            "packages.noSpacesInPath":          "Install path must not contain spaces. Some installers treat spaces as argument separators, which may truncate the path (e.g. \"E:\\Program\" instead of \"E:\\Program Files\").",

            // Install
            "install.title":                    "Installation",
            "install.desc":                     "Installing selected packages...",
            "install.progressText":             "{0} / {1} packages",
            "install.pending":                  "Pending",
            "install.installing":               "Installing...",
            "install.installed":                "Installed",
            "install.failed":                   "Failed (exit {0})",
            "install.skipped":                  "Already installed",
            "install.waitingOutput":            "Waiting for output...",
            "install.noPackages":               "No packages queued.",
            "install.btnRetry":                 "Retry",
            "install.retrying":                 "Retrying...",
            "install.waiting":                  "Waiting...",

            // Recovery
            "recovery.title":                   "Recovery",
            "recovery.desc":                    "Manage environment snapshots. Restore previous states or import/export configurations.",
            "recovery.create":                  "Create Snapshot",
            "recovery.import":                  "Import",
            "recovery.emptyState":              "No snapshots yet. Create one to get started.",
            "recovery.userCount":               "User: {0}",
            "recovery.systemCount":             "System: {0}",
            "recovery.btnRestore":              "Restore",
            "recovery.btnExport":               "Export",
            "recovery.btnDelete":               "Delete",
            "recovery.btnRestoreFull":           "Full Restore",
            "recovery.btnRestoreIncremental":    "Incremental Restore",
            "recovery.confirmRestoreTitle":     "Confirm Restore",
            "recovery.confirmRestore":          "Restore environment from this snapshot? Current variables will be overwritten.",
            "recovery.restoreModeHint":         "Full = wipe and replace all. Incremental = only restore changed variables.",
            "recovery.diffTitle":              "Changed Variables",
            "recovery.diffHint":               "Select which variables to restore from the snapshot. Unchanged items are not listed.",
            "recovery.diffSelectAll":          "Select All",
            "recovery.diffDeselectAll":        "Deselect All",
            "recovery.diffRestoreSelected":    "Restore Selected",
            "recovery.diffNoneSelected":       "Please select at least one variable.",
            "recovery.noDiff":                 "No differences found. Nothing to restore.",
            "recovery.diffDetailTitle":        "Diff Detail: {0}",
            "recovery.diffCurrentLabel":       "Current",
            "recovery.diffSnapshotLabel":      "Snapshot",
            "recovery.diffCurrentEmpty":       "(empty)",
            "recovery.diffSnapshotEmpty":      "(empty)",
            "recovery.diffPathAdded":          "New entry added in snapshot",
            "recovery.diffPathRemoved":        "Entry removed in snapshot",
            "recovery.diffPathUnchanged":      "Unchanged entry",
            "recovery.diffLineCount":          "{0} lines total · {1} added · {2} removed",
            "recovery.viewDiffBtn":            "View Diff",
            "recovery.badgeAdded":             "New",
            "recovery.badgeModified":          "Changed",
            "recovery.badgeRemoved":           "Removed",
            "recovery.willBeRemoved":          "WILL BE REMOVED",
            "recovery.fullRestoreHint":        "Full Restore will replace ALL current variables with the snapshot values. This cannot be undone.",
            "recovery.diffEmptyTitle":         "Restore Snapshot",
            "recovery.diffEmptyBody":          "Current environment is already identical to the snapshot. You can still perform a full restore.",
            "recovery.confirmDeleteTitle":      "Confirm Delete",
            "recovery.confirmDelete":           "Delete this snapshot permanently?",
            "recovery.restoreSuccess":          "Environment restored successfully.",
            "recovery.restoreFailed":           "Restore failed. Check admin permissions.",
            "recovery.exportSuccess":           "Snapshot exported successfully.",
            "recovery.exportFailed":            "Failed to export snapshot.",
            "recovery.importSuccess":           "Snapshot imported successfully.",
            "recovery.importFailed":            "Failed to import snapshot. Check file format.",
            "recovery.createTitle":             "Create Snapshot",
            "recovery.createSuccess":           "Snapshot created successfully.",
            "recovery.createFailed":            "Failed to create snapshot.",
            "recovery.descLabel":               "Description (optional)",
            "recovery.defaultDesc":             "Manual snapshot",

            // Summary
            "summary.title":                    "Summary",
            "summary.desc":                     "Installation results overview.",
            "summary.emptyState":               "No installation data yet.",
            "summary.colName":                  "Package",
            "summary.colStatus":                "Status",
            "summary.colCommand":               "Command",
            "summary.colMessage":               "Details",
            "summary.success":                  "Success",
            "summary.failed":                   "Failed",
            "summary.skipped":                  "Skipped",
            "summary.running":                  "Running",
            "summary.pending":                  "Pending",
            "summary.clearHistory":              "Clear",
            "summary.recordCount":               "{0} record(s)",
            "summary.copied":                    "Copied",

            // About
            "about.title":                      "About LazyEnv",
            "about.desc":                       "A cross-platform, recoverable, zero-pollution dev environment configurator.",
            "about.whatIs":                     "What is LazyEnv?",
            "about.whatIsText":                 "LazyEnv is a questionnaire-style development environment configuration tool for Windows. It automates the detection, installation, and management of 40+ development tools — all powered by winget under the hood. No manual PATH wrestling, no leftover pollution, and every change can be rolled back.",
            "about.features":                   "Key Features",
            "about.featuresList":               "<li><strong>System Check</strong> — Verify winget availability, admin privileges, and system prerequisites before starting.</li>"
                                                    + "<li><strong>Smart Detection</strong> — Automatically scans your machine for 40+ already-installed development tools (Python, Node, Go, Rust, Java, etc.).</li>"
                                                    + "<li><strong>One-Click Install</strong> — Select the tools you need from a curated catalog and install them all at once via winget, with real-time progress tracking.</li>"
                                                    + "<li><strong>Snapshot &amp; Rollback</strong> — Automatically creates environment snapshots before every change. Restore, diff, export, or import snapshots anytime.</li>"
                                                    + "<li><strong>Environment Variable Management</strong> — View, add, edit, and delete user/system environment variables directly. PATH is auto-managed for installed tools.</li>"
                                                    + "<li><strong>Zero Pollution</strong> — Only touch what you ask for. No background services, no registry bloat beyond environment variables. Everything lives under your control.</li>"
                                                    + "<li><strong>Cross-Platform Vision</strong> — Currently focused on Windows/winget, with macOS (Homebrew) and Linux (apt/dnf) planned for future releases.</li>",
            "about.howToUse":                   "Getting Started",
            "about.step1":                      "<strong>System Check</strong> — Open the System Check page to verify your environment meets the requirements.",
            "about.step2":                      "<strong>Browse Packages</strong> — Navigate to the Packages page, browse by category, and select the tools you need. Optionally set a custom install location.",
            "about.step3":                      "<strong>Start Installation</strong> — Click \"Start Installation\" and watch the progress. Each package is installed sequentially with live logs.",
            "about.step4":                      "<strong>Review Results</strong> — After installation, the Summary page shows success/failure status for each package, with detailed output and copyable commands.",
            "about.step5":                      "<strong>Manage Snapshots</strong> — Use the Recovery page to create, restore, compare, or export environment snapshots. Every install automatically creates a backup.",
            "about.step6":                      "<strong>Tweak Settings</strong> — In Settings, manually edit user/system environment variables. Snapshots are auto-created before any modification.",
            "about.privacyTitle":               "Data & Privacy",
            "about.privacyText":                "LazyEnv runs entirely on your machine. It does not collect telemetry, send usage data, or require network access beyond winget package downloads. Snapshots and configuration data are stored locally under <code>%LOCALAPPDATA%\\LazyEnv</code>. You own your data, always.",
            "about.licenseTitle":               "Open Source",
            "about.licenseText":                "LazyEnv is free and open-source software licensed under the <a href=\"https://opensource.org/licenses/MIT\" target=\"_blank\">MIT License</a>. You are free to use, modify, and distribute it. Contributions, issues, and feature requests are welcome on <a href=\"https://github.com/kemoving/LazyEnv\" target=\"_blank\">GitHub</a>.",

            // Dialog
            "dialog.cancel":                    "Cancel",

            // Language switcher
            "lang.label":                       "Language"
        },

        // ===================================================================
        // Simplified Chinese
        // ===================================================================
        "zh-CN": {
            // Titlebar
            "titlebar.label":                   "LazyEnv",
            "btn.minimize":                     "\u6700\u5c0f\u5316",
            "btn.maximize":                     "\u6700\u5927\u5316",
            "btn.close":                        "\u5173\u95ed",

            // Sidebar
            "sidebar.search":                   "\u641c\u7d22\u9875\u9762...",
            "sidebar.overview":                 "\u6982\u89c8",
            "sidebar.home":                     "\u4e3b\u9875",
            "sidebar.settings":                 "\u8bbe\u7f6e",
            "sidebar.setup":                    "\u914d\u7f6e",
            "sidebar.syscheck":                 "\u7cfb\u7edf\u68c0\u67e5",
            "sidebar.packages":                 "\u8f6f\u4ef6\u5305",
            "sidebar.install":                  "\u5b89\u88c5",
            "sidebar.maintenance":              "\u7ef4\u62a4",
            "sidebar.recovery":                 "\u6062\u590d",
            "sidebar.summary":                  "\u6458\u8981",
            "sidebar.info":                     "\u4fe1\u606f",
            "sidebar.about":                    "\u5173\u4e8e",

            // Home
            "home.title":                       "\u4e3b\u9875",
            "home.desc":                        "\u68c0\u6d4b\u5230\u7684\u672c\u673a\u5f00\u53d1\u73af\u5883\u3002",
            "home.refresh":                     "\u5237\u65b0",
            "home.addManual":                   "\u6dfb\u52a0",
            "home.searchPlaceholder":           "\u641c\u7d22\u73af\u5883...",
            "home.addTitle":                    "\u624b\u52a8\u6dfb\u52a0\u5f00\u53d1\u73af\u5883",
            "home.addCmdPlaceholder":           "\u4f8b\u5982 python, node, rustc, gcc",
            "home.detect":                      "\u68c0\u6d4b",

            // Categories
            "category.language":                "\u8bed\u8a00 / \u8fd0\u884c\u65f6",
            "category.tool":                    "\u6784\u5efa\u5de5\u5177",
            "category.runtime":                 "\u5bb9\u5668 / \u865a\u62df\u673a",
            "category.utility":                 "\u5b9e\u7528\u5de5\u5177",
            "category.editor":                  "\u7f16\u8f91\u5668 / IDE",
            "category.database":                "\u6570\u636e\u5e93",
            "category.other":                   "\u5176\u4ed6",

            // Environment cards
            "env.scanning":                     "\u6b63\u5728\u626b\u63cf\u7cfb\u7edf...",
            "env.noMatch":                      "\u672a\u627e\u5230\u5339\u914d\u7684\u73af\u5883\u3002",
            "env.btnUninstall":                 "\u5378\u8f7d",
            "env.btnOpenDir":                   "\u6253\u5f00\u76ee\u5f55",
            "env.confirmUninstallTitle":        "\u786e\u8ba4\u5378\u8f7d",
            "env.confirmUninstall":             "\u786e\u5b9a\u8981\u901a\u8fc7 winget \u5378\u8f7d {0} \u5417\uff1f",
            "env.uninstallSuccess":             "\u5df2\u5378\u8f7d: {0}",
            "env.uninstallFailed":              "\u5378\u8f7d\u5931\u8d25\u3002",

            // Probe
            "probe.detecting":                  "\u68c0\u6d4b\u4e2d...",
            "probe.found":                      "\u5df2\u627e\u5230: {0}",
            "probe.notFound":                   "\u672a\u627e\u5230\u3002",
            "probe.addedToast":                 "\u5df2\u6dfb\u52a0: {0} ({1})",

            // Settings (Environment Variable Editor)
            "settings.title":                   "\u8bbe\u7f6e",
            "settings.desc":                    "\u76f4\u63a5\u7ba1\u7406\u73af\u5883\u53d8\u91cf\u3002\u66f4\u6539\u7acb\u5373\u751f\u6548\u3002",
            "settings.snapshotHint":            "\u4efb\u4f55\u4fee\u6539\u524d\u4f1a\u81ea\u52a8\u521b\u5efa\u5feb\u7167\uff0c\u4ee5\u4fbf\u5b89\u5168\u56de\u6eda\u3002",
            "settings.userVars":                "\u7528\u6237\u53d8\u91cf",
            "settings.systemVars":              "\u7cfb\u7edf\u53d8\u91cf",
            "settings.scopeUser":               "\u7528\u6237\u53d8\u91cf",
            "settings.scopeSystem":             "\u7cfb\u7edf\u53d8\u91cf",
            "settings.addNew":                  "\u65b0\u5efa\u53d8\u91cf",
            "settings.searchPlaceholder":       "\u641c\u7d22\u53d8\u91cf...",
            "settings.colName":                 "\u540d\u79f0",
            "settings.colValue":                "\u503c",
            "settings.colActions":              "\u64cd\u4f5c",
            "settings.loading":                 "\u6b63\u5728\u52a0\u8f7d\u73af\u5883\u53d8\u91cf...",
            "settings.noMatch":                 "\u672a\u627e\u5230\u5339\u914d\u7684\u53d8\u91cf\u3002",
            "settings.edit":                    "\u7f16\u8f91",
            "settings.delete":                  "\u5220\u9664",
            "settings.save":                    "\u4fdd\u5b58",
            "settings.type":                    "\u7c7b\u578b",
            "settings.newVarTitle":             "\u65b0\u5efa\u53d8\u91cf",
            "settings.editVarTitle":            "\u7f16\u8f91: {0}",
            "settings.viewVarTitle":            "\u67e5\u770b: {0}",
            "settings.viewScope":              "\u4f5c\u7528\u8303\u56f4",
            "settings.viewScopeUser":          "\u7528\u6237",
            "settings.viewScopeSystem":        "\u7cfb\u7edf",
            "settings.viewClose":              "\u5173\u95ed",
            "settings.nameRequired":            "\u53d8\u91cf\u540d\u79f0\u4e0d\u80fd\u4e3a\u7a7a\u3002",
            "settings.pathAdd":                 "\u6dfb\u52a0\u6761\u76ee",
            "settings.saveSuccess":             "\u53d8\u91cf\u5df2\u4fdd\u5b58\u3002",
            "settings.saveFailed":              "\u4fdd\u5b58\u53d8\u91cf\u5931\u8d25: {0}",
            "settings.deleteSuccess":           "\u53d8\u91cf\u5df2\u5220\u9664\u3002",
            "settings.deleteFailed":            "\u5220\u9664\u53d8\u91cf\u5931\u8d25: {0}",
            "settings.confirmDeleteTitle":      "\u786e\u8ba4\u5220\u9664",
            "settings.confirmDelete":           "\u5220\u9664\u73af\u5883\u53d8\u91cf \"{0}\"\uff1f",
            "settings.adminWarningShort":       "\u60a8\u6b63\u5728\u4ee5\u975e\u7ba1\u7406\u5458\u6743\u9650\u8fd0\u884c \u2014 \u7cfb\u7edf\u53d8\u91cf\u4ec5\u53ef\u67e5\u770b\uff0c\u4e0d\u53ef\u4fee\u6539\u3002",
            "settings.adminRequired":           "\u4fee\u6539\u7cfb\u7edf\u53d8\u91cf\u9700\u8981\u7ba1\u7406\u5458\u6743\u9650\u3002",

            // System Check
            "syscheck.title":                   "\u7cfb\u7edf\u68c0\u67e5",
            "syscheck.desc":                    "\u5b89\u88c5\u524d\u9a8c\u8bc1\u7cfb\u7edf\u5148\u51b3\u6761\u4ef6\u3002",
            "syscheck.rerun":                   "\u91cd\u65b0\u68c0\u67e5",
            "check.os":                         "\u64cd\u4f5c\u7cfb\u7edf",
            "check.webview2":                   "WebView2 \u8fd0\u884c\u65f6",
            "check.winget":                     "winget \u5305\u7ba1\u7406\u5668",
            "check.detecting":                  "\u68c0\u6d4b\u4e2d...",
            "check.available":                  "\u53ef\u7528",
            "check.checking":                   "\u68c0\u67e5\u4e2d...",
            "check.notFound":                   "\u672a\u627e\u5230",

            // Packages
            "packages.title":                   "\u8f6f\u4ef6\u5305",
            "packages.desc":                    "\u9009\u62e9\u8981\u5b89\u88c5\u7684\u5f00\u53d1\u5de5\u5177\u3002",
            "packages.selectAll":               "\u5168\u9009",
            "packages.deselectAll":             "\u53d6\u6d88\u5168\u9009",
            "packages.searchPlaceholder":       "\u641c\u7d22\u8f6f\u4ef6\u5305...",
            "packages.startInstall":            "\u5f00\u59cb\u5b89\u88c5",
            "packages.selectedCount":           "\u5df2\u9009\u62e9 {0} \u4e2a",
            "packages.installed":               "\u5df2\u5b89\u88c5",
            "packages.alreadyInstalled":        "\u7cfb\u7edf\u4e2d\u5df2\u5b89\u88c5",
            "packages.installLocation":         "\u5b89\u88c5\u8def\u5f84\uff08\u7559\u7a7a\u5219\u4f7f\u7528\u9ed8\u8ba4\u8def\u5f84\uff09",
            "packages.installLocationPlaceholder": "\u4f8b\u5982 D:\\Tools\uff08\u6bcf\u4e2a\u5305\u4f1a\u81ea\u52a8\u521b\u5efa\u72ec\u7acb\u5b50\u76ee\u5f55\uff09",
            "packages.locationHint":            "\u63d0\u793a\uff1a\u6bcf\u4e2a\u5305\u4f1a\u5b89\u88c5\u5230\u72ec\u7acb\u7684\u5b50\u76ee\u5f55\u4e2d\u3002",
            "packages.noDriveSpaceAfterColon":  "\u8bf7\u8f93\u5165\u76d8\u7b26\u548c\u8def\u5f84\uff08\u4f8b\u5982 D:\\MyApps\uff09\u3002",
            "packages.noSpacesInPath":          "\u5b89\u88c5\u8def\u5f84\u4e0d\u80fd\u5305\u542b\u7a7a\u683c\u3002\u90e8\u5206\u5b89\u88c5\u5668\u4f1a\u5c06\u7a7a\u683c\u89c6\u4e3a\u53c2\u6570\u5206\u9694\u7b26\uff0c\u53ef\u80fd\u5bfc\u81f4\u8def\u5f84\u622a\u65ad\uff08\u5982\u53ea\u53d6 \"E:\\Program\" \u800c\u975e \"E:\\Program Files\"\uff09\u3002",

            // Install
            "install.title":                    "\u5b89\u88c5",
            "install.desc":                     "\u6b63\u5728\u5b89\u88c5\u5df2\u9009\u8f6f\u4ef6\u5305...",
            "install.progressText":             "{0} / {1} \u4e2a\u8f6f\u4ef6\u5305",
            "install.pending":                  "\u7b49\u5f85\u4e2d",
            "install.installing":               "\u5b89\u88c5\u4e2d...",
            "install.installed":                "\u5df2\u5b89\u88c5",
            "install.failed":                   "\u5931\u8d25 (\u9000\u51fa\u7801 {0})",
            "install.skipped":                  "\u5df2\u5b58\u5728",
            "install.waitingOutput":            "\u7b49\u5f85\u8f93\u51fa...",
            "install.noPackages":               "\u6ca1\u6709\u6392\u961f\u7684\u8f6f\u4ef6\u5305\u3002",
            "install.btnRetry":                 "\u91cd\u8bd5",
            "install.retrying":                 "\u91cd\u8bd5\u4e2d...",
            "install.waiting":                  "\u7b49\u5f85\u4e2d...",

            // Recovery
            "recovery.title":                   "\u6062\u590d",
            "recovery.desc":                    "\u7ba1\u7406\u73af\u5883\u5feb\u7167\u3002\u53ef\u6062\u590d\u5230\u5386\u53f2\u72b6\u6001\u6216\u5bfc\u5165\u5bfc\u51fa\u914d\u7f6e\u3002",
            "recovery.create":                  "\u521b\u5efa\u5feb\u7167",
            "recovery.import":                  "\u5bfc\u5165",
            "recovery.emptyState":              "\u6682\u65e0\u5feb\u7167\u3002\u521b\u5efa\u4e00\u4e2a\u4ee5\u5f00\u59cb\u4f7f\u7528\u3002",
            "recovery.userCount":               "\u7528\u6237\u53d8\u91cf: {0}",
            "recovery.systemCount":             "\u7cfb\u7edf\u53d8\u91cf: {0}",
            "recovery.btnRestore":              "\u6062\u590d",
            "recovery.btnExport":               "\u5bfc\u51fa",
            "recovery.btnDelete":               "\u5220\u9664",
            "recovery.btnRestoreFull":           "\u5168\u91cf\u6062\u590d",
            "recovery.btnRestoreIncremental":    "\u589e\u91cf\u6062\u590d",
            "recovery.confirmRestoreTitle":     "\u786e\u8ba4\u6062\u590d",
            "recovery.confirmRestore":          "\u786e\u5b9a\u8981\u4ece\u6b64\u5feb\u7167\u6062\u590d\u73af\u5883\u5417\uff1f\u5f53\u524d\u53d8\u91cf\u5c06\u88ab\u8986\u76d6\u3002",
            "recovery.restoreModeHint":         "\u5168\u91cf = \u6e05\u7a7a\u540e\u91cd\u5199\u5168\u90e8\u53d8\u91cf\u3002\u589e\u91cf = \u9009\u62e9\u5e76\u6062\u590d\u5df2\u6539\u53d8\u7684\u53d8\u91cf\u3002",
            "recovery.diffTitle":              "\u53d8\u66f4\u5bf9\u6bd4",
            "recovery.diffHint":               "\u8bf7\u9009\u62e9\u8981\u4ece\u5feb\u7167\u6062\u590d\u7684\u53d8\u91cf\uff0c\u672a\u53d8\u52a8\u7684\u53d8\u91cf\u4e0d\u4f1a\u663e\u793a\u3002",
            "recovery.diffSelectAll":          "\u5168\u9009",
            "recovery.diffDeselectAll":        "\u53d6\u6d88\u5168\u9009",
            "recovery.diffRestoreSelected":    "\u6062\u590d\u6240\u9009",
            "recovery.diffNoneSelected":       "\u8bf7\u81f3\u5c11\u9009\u62e9\u4e00\u4e2a\u53d8\u91cf\u3002",
            "recovery.noDiff":                 "\u672a\u53d1\u73b0\u5dee\u5f02\uff0c\u65e0\u9700\u6062\u590d\u3002",
            "recovery.diffDetailTitle":        "\u53d8\u91cf\u5dee\u5f02: {0}",
            "recovery.diffCurrentLabel":       "\u5f53\u524d\u503c",
            "recovery.diffSnapshotLabel":      "\u5feb\u7167\u503c",
            "recovery.diffCurrentEmpty":       "(\u4e3a\u7a7a)",
            "recovery.diffSnapshotEmpty":      "(\u4e3a\u7a7a)",
            "recovery.diffPathAdded":          "\u5feb\u7167\u4e2d\u65b0\u589e\u7684\u8def\u5f84",
            "recovery.diffPathRemoved":        "\u5feb\u7167\u4e2d\u79fb\u9664\u7684\u8def\u5f84",
            "recovery.diffPathUnchanged":      "\u672a\u53d8\u52a8\u7684\u8def\u5f84",
            "recovery.diffLineCount":          "\u5171 {0} \u884c \u00b7 \u65b0\u589e {1} \u00b7 \u79fb\u9664 {2}",
            "recovery.viewDiffBtn":            "\u67e5\u770b\u5dee\u5f02",
            "recovery.badgeAdded":             "\u65b0\u589e",
            "recovery.badgeModified":          "\u5df2\u4fee\u6539",
            "recovery.badgeRemoved":           "\u5c06\u88ab\u5220\u9664",
            "recovery.willBeRemoved":          "\u5c06\u88ab\u5220\u9664",
            "recovery.fullRestoreHint":        "\u5168\u91cf\u6062\u590d\u5c06\u4f1a\u7528\u5feb\u7167\u503c\u66ff\u6362\u5f53\u524d\u5168\u90e8\u73af\u5883\u53d8\u91cf\uff0c\u6b64\u64cd\u4f5c\u4e0d\u53ef\u64a4\u9500\u3002",
            "recovery.diffEmptyTitle":         "\u6062\u590d\u5feb\u7167",
            "recovery.diffEmptyBody":          "\u5f53\u524d\u73af\u5883\u5df2\u4e0e\u5feb\u7167\u4e00\u81f4\u3002\u4f60\u4ecd\u53ef\u6267\u884c\u5168\u91cf\u6062\u590d\u3002",
            "recovery.confirmDeleteTitle":      "\u786e\u8ba4\u5220\u9664",
            "recovery.confirmDelete":           "\u786e\u5b9a\u8981\u6c38\u4e45\u5220\u9664\u6b64\u5feb\u7167\u5417\uff1f",
            "recovery.restoreSuccess":          "\u73af\u5883\u5df2\u6210\u529f\u6062\u590d\u3002",
            "recovery.restoreFailed":           "\u6062\u590d\u5931\u8d25\u3002\u8bf7\u68c0\u67e5\u7ba1\u7406\u5458\u6743\u9650\u3002",
            "recovery.exportSuccess":           "\u5feb\u7167\u5df2\u6210\u529f\u5bfc\u51fa\u3002",
            "recovery.exportFailed":            "\u5bfc\u51fa\u5feb\u7167\u5931\u8d25\u3002",
            "recovery.importSuccess":           "\u5feb\u7167\u5df2\u6210\u529f\u5bfc\u5165\u3002",
            "recovery.importFailed":            "\u5bfc\u5165\u5feb\u7167\u5931\u8d25\u3002\u8bf7\u68c0\u67e5\u6587\u4ef6\u683c\u5f0f\u3002",
            "recovery.createTitle":             "\u521b\u5efa\u5feb\u7167",
            "recovery.createSuccess":           "\u5feb\u7167\u5df2\u6210\u529f\u521b\u5efa\u3002",
            "recovery.createFailed":            "\u521b\u5efa\u5feb\u7167\u5931\u8d25\u3002",
            "recovery.descLabel":               "\u63cf\u8ff0\uff08\u53ef\u9009\uff09",
            "recovery.defaultDesc":             "\u624b\u52a8\u5feb\u7167",

            // Summary
            "summary.title":                    "\u6458\u8981",
            "summary.desc":                     "\u5b89\u88c5\u7ed3\u679c\u6982\u89c8\u3002",
            "summary.emptyState":               "\u6682\u65e0\u5b89\u88c5\u6570\u636e\u3002",
            "summary.colName":                  "\u8f6f\u4ef6\u5305",
            "summary.colStatus":                "\u72b6\u6001",
            "summary.colCommand":               "\u547d\u4ee4",
            "summary.colMessage":               "\u8be6\u60c5",
            "summary.success":                  "\u6210\u529f",
            "summary.failed":                   "\u5931\u8d25",
            "summary.skipped":                  "\u5df2\u8df3\u8fc7",
            "summary.running":                  "\u8fdb\u884c\u4e2d",
            "summary.pending":                  "\u7b49\u5f85\u4e2d",
            "summary.clearHistory":              "\u6e05\u7a7a",
            "summary.recordCount":               "\u5171 {0} \u6761\u8bb0\u5f55",
            "summary.copied":                    "\u5df2\u590d\u5236",

            // About
            "about.title":                      "\u5173\u4e8e LazyEnv",
            "about.desc":                       "\u8de8\u5e73\u53f0\u3001\u53ef\u6062\u590d\u3001\u96f6\u6c61\u67d3\u7684\u5f00\u53d1\u73af\u5883\u914d\u7f6e\u5de5\u5177\u3002",
            "about.whatIs":                     "LazyEnv \u662f\u4ec0\u4e48\uff1f",
            "about.whatIsText":                 "LazyEnv \u662f\u4e00\u6b3e Windows \u5e73\u53f0\u7684\u95ee\u5377\u5f0f\u5f00\u53d1\u73af\u5883\u914d\u7f6e\u5de5\u5177\u3002\u5b83\u80fd\u81ea\u52a8\u68c0\u6d4b\u3001\u5b89\u88c5\u548c\u7ba1\u7406 40+ \u79cd\u5f00\u53d1\u5de5\u5177\uff0c\u5e95\u5c42\u57fa\u4e8e winget \u5b89\u88c5\u3002\u65e0\u9700\u624b\u52a8\u5904\u7406 PATH\uff0c\u65e0\u6b8b\u7559\u6c61\u67d3\uff0c\u6bcf\u6b21\u66f4\u6539\u90fd\u53ef\u56de\u6eda\u3002",
            "about.features":                   "\u6838\u5fc3\u529f\u80fd",
            "about.featuresList":               "<li><strong>\u7cfb\u7edf\u68c0\u67e5</strong> \u2014 \u68c0\u6d4b\u662f\u5426\u5b89\u88c5 winget\u3001\u7ba1\u7406\u5458\u6743\u9650\u53ca\u5176\u4ed6\u5148\u51b3\u6761\u4ef6\u3002</li>"
                                                    + "<li><strong>\u667a\u80fd\u68c0\u6d4b</strong> \u2014 \u81ea\u52a8\u626b\u63cf\u672c\u673a\u5df2\u5b89\u88c5\u7684 40+ \u79cd\u5f00\u53d1\u5de5\u5177\uff08Python\u3001Node\u3001Go\u3001Rust\u3001Java \u7b49\uff09\u3002</li>"
                                                    + "<li><strong>\u4e00\u952e\u5b89\u88c5</strong> \u2014 \u4ece\u7cbe\u9009\u8f6f\u4ef6\u5305\u76ee\u5f55\u9009\u62e9\u6240\u9700\u5de5\u5177\uff0c\u901a\u8fc7 winget \u4e00\u6b21\u6027\u5b89\u88c5\uff0c\u5e76\u5b9e\u65f6\u663e\u793a\u8fdb\u5ea6\u3002</li>"
                                                    + "<li><strong>\u5feb\u7167\u4e0e\u56de\u6eda</strong> \u2014 \u6bcf\u6b21\u4fee\u6539\u524d\u81ea\u52a8\u521b\u5efa\u73af\u5883\u5feb\u7167\uff0c\u53ef\u968f\u65f6\u6062\u590d\u3001\u5bf9\u6bd4\u3001\u5bfc\u51fa\u6216\u5bfc\u5165\u3002</li>"
                                                    + "<li><strong>\u73af\u5883\u53d8\u91cf\u7ba1\u7406</strong> \u2014 \u76f4\u63a5\u67e5\u770b\u3001\u65b0\u589e\u3001\u7f16\u8f91\u3001\u5220\u9664\u7528\u6237/\u7cfb\u7edf\u73af\u5883\u53d8\u91cf\uff0cPATH \u81ea\u52a8\u7ef4\u62a4\u3002</li>"
                                                    + "<li><strong>\u96f6\u6c61\u67d3</strong> \u2014 \u4ec0\u4e48\u9700\u8981\u6539\u4ec0\u4e48\uff0c\u4e0d\u52a8\u4f60\u4e0d\u8bf7\u6c42\u7684\u4e1c\u897f\u3002\u65e0\u540e\u53f0\u670d\u52a1\uff0c\u65e0\u989d\u5916\u6ce8\u518c\u8868\u5783\u573e\uff0c\u4e00\u5207\u638c\u63a7\u5728\u4f60\u624b\u4e2d\u3002</li>"
                                                    + "<li><strong>\u8de8\u5e73\u53f0\u613f\u666f</strong> \u2014 \u76ee\u524d\u4e13\u6ce8 Windows/winget\uff0cmacOS\uff08Homebrew\uff09\u4e0e Linux\uff08apt/dnf\uff09\u5728\u540e\u7eed\u7248\u672c\u4e2d\u63d0\u4f9b\u3002</li>",
            "about.howToUse":                   "\u4f7f\u7528\u6b65\u9aa4",
            "about.step1":                      "<strong>\u7cfb\u7edf\u68c0\u67e5</strong> \u2014 \u6253\u5f00\u7cfb\u7edf\u68c0\u67e5\u9875\u786e\u8ba4\u73af\u5883\u6ee1\u8db3\u8981\u6c42\u3002",
            "about.step2":                      "<strong>\u6d4f\u89c8\u8f6f\u4ef6\u5305</strong> \u2014 \u8fdb\u5165\u8f6f\u4ef6\u5305\u9875\uff0c\u6309\u5206\u7c7b\u6d4f\u89c8\u5e76\u9009\u62e9\u9700\u8981\u7684\u5de5\u5177\uff0c\u53ef\u81ea\u5b9a\u4e49\u5b89\u88c5\u8def\u5f84\u3002",
            "about.step3":                      "<strong>\u5f00\u59cb\u5b89\u88c5</strong> \u2014 \u70b9\u51fb\u201c\u5f00\u59cb\u5b89\u88c5\u201d\u5e76\u67e5\u770b\u8fdb\u5ea6\uff0c\u6bcf\u4e2a\u8f6f\u4ef6\u5305\u6309\u987a\u5e8f\u5b89\u88c5\u5e76\u5b9e\u65f6\u663e\u793a\u65e5\u5fd7\u3002",
            "about.step4":                      "<strong>\u67e5\u770b\u7ed3\u679c</strong> \u2014 \u5b89\u88c5\u5b8c\u6210\u540e\uff0c\u6458\u8981\u9875\u663e\u793a\u6bcf\u4e2a\u5305\u7684\u6210\u529f/\u5931\u8d25\u72b6\u6001\uff0c\u4ee5\u53ca\u8be6\u7ec6\u8f93\u51fa\u548c\u53ef\u590d\u5236\u7684\u547d\u4ee4\u3002",
            "about.step5":                      "<strong>\u7ba1\u7406\u5feb\u7167</strong> \u2014 \u5728\u6062\u590d\u9875\u521b\u5efa\u3001\u6062\u590d\u3001\u5bf9\u6bd4\u6216\u5bfc\u51fa\u73af\u5883\u5feb\u7167\uff0c\u6bcf\u6b21\u5b89\u88c5\u81ea\u52a8\u521b\u5efa\u5907\u4efd\u3002",
            "about.step6":                      "<strong>\u5fae\u8c03\u8bbe\u7f6e</strong> \u2014 \u5728\u8bbe\u7f6e\u9875\u624b\u52a8\u7f16\u8f91\u7528\u6237/\u7cfb\u7edf\u73af\u5883\u53d8\u91cf\uff0c\u4fee\u6539\u524d\u81ea\u52a8\u521b\u5efa\u5feb\u7167\u3002",
            "about.privacyTitle":               "\u6570\u636e\u4e0e\u9690\u79c1",
            "about.privacyText":                "LazyEnv \u5b8c\u5168\u8fd0\u884c\u5728\u60a8\u7684\u672c\u673a\u4e0a\uff0c\u4e0d\u6536\u96c6\u9065\u6d4b\u6570\u636e\u3001\u4e0d\u53d1\u9001\u4f7f\u7528\u6570\u636e\uff0c\u9664 winget \u4e0b\u8f7d\u5305\u4e4b\u5916\u65e0\u9700\u7f51\u7edc\u8bbf\u95ee\u3002\u5feb\u7167\u548c\u914d\u7f6e\u6570\u636e\u5b58\u50a8\u5728 <code>%LOCALAPPDATA%\\LazyEnv</code> \u76ee\u5f55\u4e0b\uff0c\u60a8\u59cb\u7ec8\u62e5\u6709\u81ea\u5df1\u7684\u6570\u636e\u3002",
            "about.licenseTitle":               "\u5f00\u6e90\u8bb8\u53ef",
            "about.licenseText":                "LazyEnv \u662f\u514d\u8d39\u5f00\u6e90\u8f6f\u4ef6\uff0c\u91c7\u7528 <a href=\"https://opensource.org/licenses/MIT\" target=\"_blank\">MIT \u8bb8\u53ef</a>\u3002\u60a8\u53ef\u4ee5\u81ea\u7531\u4f7f\u7528\u3001\u4fee\u6539\u548c\u5206\u53d1\u3002\u6b22\u8fce\u5728 <a href=\"https://github.com/kemoving/LazyEnv\" target=\"_blank\">GitHub</a> \u4e0a\u63d0\u4ea4\u4ee3\u7801\u3001\u53cd\u9988\u95ee\u9898\u548c\u529f\u80fd\u5efa\u8bae\u3002",

            // Dialog
            "dialog.cancel":                    "\u53d6\u6d88",

            // Language switcher
            "lang.label":                       "\u8bed\u8a00"
        }
    };

    // -----------------------------------------------------------------------
    // State
    // -----------------------------------------------------------------------
    var currentLocale = "en";

    // Detect system language
    (function detectLocale() {
        var lang = navigator.language || navigator.userLanguage || "en";
        if (lang.toLowerCase().startsWith("zh")) {
            currentLocale = "zh-CN";
        } else {
            currentLocale = "en";
        }
        // Check localStorage override
        var saved = localStorage.getItem("lazyenv_locale");
        if (saved && locales[saved]) {
            currentLocale = saved;
        }
    })();

    // -----------------------------------------------------------------------
    // Translation function
    // t(key)            -> returns translated string
    // t(key, arg1, ...) -> replaces {0}, {1}, ... with arguments
    // -----------------------------------------------------------------------
    function t(key) {
        var dict = locales[currentLocale] || locales["en"];
        var str = dict[key];
        if (str === undefined) {
            // Fallback to English
            str = locales["en"][key];
        }
        if (str === undefined) {
            // Return key itself as last resort
            return key;
        }
        // Replace placeholders {0}, {1}, ...
        if (arguments.length > 1) {
            for (var i = 1; i < arguments.length; i++) {
                str = str.replace("{" + (i - 1) + "}", arguments[i]);
            }
        }
        return str;
    }

    // -----------------------------------------------------------------------
    // Apply translations to static HTML elements
    // Elements with data-i18n attribute get their textContent replaced.
    // Elements with data-i18n-html get their innerHTML replaced (allows markup).
    // Elements with data-i18n-placeholder get their placeholder replaced.
    // Elements with data-i18n-title get their title replaced.
    // -----------------------------------------------------------------------
    function applyStaticTranslations() {
        document.querySelectorAll("[data-i18n]").forEach(function (el) {
            var key = el.getAttribute("data-i18n");
            if (key) el.textContent = t(key);
        });
        document.querySelectorAll("[data-i18n-html]").forEach(function (el) {
            var key = el.getAttribute("data-i18n-html");
            if (key) el.innerHTML = t(key);
        });
        document.querySelectorAll("[data-i18n-placeholder]").forEach(function (el) {
            var key = el.getAttribute("data-i18n-placeholder");
            if (key) el.placeholder = t(key);
        });
        document.querySelectorAll("[data-i18n-title]").forEach(function (el) {
            var key = el.getAttribute("data-i18n-title");
            if (key) el.title = t(key);
        });
    }

    // -----------------------------------------------------------------------
    // Set locale and re-apply
    // -----------------------------------------------------------------------
    function setLocale(locale) {
        if (!locales[locale]) return;
        currentLocale = locale;
        localStorage.setItem("lazyenv_locale", locale);
        document.documentElement.lang = locale === "zh-CN" ? "zh-CN" : "en";
        applyStaticTranslations();
        // Dispatch event so dynamic content can re-render
        window.dispatchEvent(new CustomEvent("lazyenv:localeChanged", { detail: { locale: locale } }));
    }

    function getLocale() {
        return currentLocale;
    }

    function getAvailableLocales() {
        return Object.keys(locales);
    }

    // -----------------------------------------------------------------------
    // Public API (attached to window)
    // -----------------------------------------------------------------------
    window.LazyEnvI18n = {
        t: t,
        setLocale: setLocale,
        getLocale: getLocale,
        getAvailableLocales: getAvailableLocales,
        applyStaticTranslations: applyStaticTranslations
    };

    // Apply on DOM ready
    if (document.readyState === "loading") {
        document.addEventListener("DOMContentLoaded", function () {
            applyStaticTranslations();
        });
    } else {
        applyStaticTranslations();
    }

})();

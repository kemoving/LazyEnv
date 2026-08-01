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
// LazyEnv - script.js
// Frontend: sidebar navigation, home env detection, settings env-var editor,
// install progress with streaming log, retry, window drag, i18n support
// ============================================================================

(function () {
    "use strict";

    // -----------------------------------------------------------------------
    // i18n shorthand
    // -----------------------------------------------------------------------
    var t = window.LazyEnvI18n.t;

    // -----------------------------------------------------------------------
    // State
    // -----------------------------------------------------------------------
    var currentPage = "home";
    var catalog = [];
    var selectedPackages = new Set();
    var installResults = new Map();
    var preInstallSnapshotId = "";
    var detectedEnvironments = [];
    var manualEnvironments = [];
    var installTotal = 0;
    var installCurrent = 0;
    var isMaximized = false;
    var isAdmin = true; // default true until proven otherwise — avoids false-positives
    var installedPackages = new Set(); // IDs of packages already installed on the system
    var currentInstallLocation = ""; // custom install location set by user

    // Settings state
    var envVarScope = "user";   // "user" or "system"
    var envVarCache = [];       // [{name, value, type}]

    // -----------------------------------------------------------------------
    // Diagnostic log bridge — sends to C++ debug_log via postMessage
    // -----------------------------------------------------------------------
    function dbgLog(msg) {
        try {
            if (window.chrome && window.chrome.webview) {
                window.chrome.webview.postMessage(JSON.stringify({
                    action: "debugLog",
                    msg: msg
                }));
            }
            console.log("[LazyEnv DIAG]", msg);
        } catch (e) { /* never fail on logging */ }
    }

    // -----------------------------------------------------------------------
    // Viewport fix — C++ sends windowState with native client-area dimensions
    // after put_Bounds. ResizeObserver / resize serve as a fallback for
    // incremental drag-resize.
    //
    // We avoid display:none toggles (visible flicker) and throttle with rAF
    // to prevent multiple reflows within the same animation frame.
    // -----------------------------------------------------------------------

    var _reflowRAF = null;
    var _setVarsCount = 0;

    function setViewportVars(w, h) {
        ++_setVarsCount;
        document.documentElement.style.setProperty("--vw", (w * 0.01) + "px");
        document.documentElement.style.setProperty("--vh", (h * 0.01) + "px");
        dbgLog("setViewportVars #" + _setVarsCount
               + " w=" + w + " h=" + h
               + " window.inner=" + window.innerWidth + "x" + window.innerHeight);
    }

    function triggerReflow() {
        var w = window.innerWidth  || document.documentElement.clientWidth;
        var h = window.innerHeight || document.documentElement.clientHeight;
        dbgLog("triggerReflow window.inner=" + w + "x" + h
               + " docElem.client=" + document.documentElement.clientWidth
               + "x" + document.documentElement.clientHeight);
        setViewportVars(w, h);

        // Throttle: at most one forceLayout per animation frame
        if (_reflowRAF === null) {
            _reflowRAF = requestAnimationFrame(function () {
                _reflowRAF = null;
                var app = document.querySelector(".app");
                if (app) {
                    void app.offsetHeight;  // non-visual reflow
                    var style = window.getComputedStyle(app);
                    dbgLog("triggerReflow(rAF) .app computed=" + style.width + "x" + style.height
                           + " offsetHeight=" + app.offsetHeight);
                }
            });
        }
    }

    // Keep ResizeObserver and resize event as fallbacks
    if (window.ResizeObserver) {
        new ResizeObserver(function (entries) {
            var e = entries[entries.length - 1];
            if (e && e.contentRect) {
                dbgLog("ResizeObserver contentRect="
                       + e.contentRect.width + "x" + e.contentRect.height);
            }
            triggerReflow();
        }).observe(document.documentElement);
    }
    window.addEventListener("resize", function () {
        dbgLog("resize EVENT window.inner=" + window.innerWidth + "x" + window.innerHeight);
        triggerReflow();
    });

    triggerReflow();

    // -----------------------------------------------------------------------
    // Native bridge
    // -----------------------------------------------------------------------
    function sendNative(obj) {
        if (window.chrome && window.chrome.webview) {
            window.chrome.webview.postMessage(JSON.stringify(obj));
        } else {
            console.log("[LazyEnv -> Native]", obj);
            handleMock(obj);
        }
    }

    if (window.chrome && window.chrome.webview) {
        window.chrome.webview.addEventListener("message", function (e) {
            var data;
            try { data = typeof e.data === "string" ? JSON.parse(e.data) : e.data; } catch (_) { return; }
            handleNative(data);
        });
    }

    // -----------------------------------------------------------------------
    // Native response handler
    // -----------------------------------------------------------------------
    function handleNative(d) {
        switch (d.action) {
            case "environmentsDetected":
                detectedEnvironments = d.environments || [];
                if (currentPage === "home") {
                    renderEnvironments(document.getElementById("homeSearch") ? document.getElementById("homeSearch").value : "");
                    // Now that CLI detection is done (no subprocess contention), refresh winget list
                    sendNative({ action: "checkInstalled" });
                }
                break;

            case "adminStatus":
                isAdmin = d.isAdmin === true;
                updateAdminWarning();
                break;

            case "folderSelected":
                if (d.path) {
                    var flInput = document.getElementById("txtInstallLocation");
                    // Reject paths with spaces: some installers tokenize --location by space
                    if (/\s/.test(d.path)) {
                        showToast(t("packages.noSpacesInPath"), "error");
                        if (flInput) { flInput.value = ""; }
                        currentInstallLocation = "";
                        saveInstallLocation("");
                        updateLocationTooltip("");
                    } else {
                        if (flInput) { flInput.value = d.path; }
                        currentInstallLocation = d.path;
                        saveInstallLocation(d.path);
                        updateLocationTooltip(d.path);
                    }
                }
                break;

            case "probeResult":
                handleProbeResult(d);
                break;

            case "wingetStatus":
                renderCheckItem("winget", d.available);
                break;

            case "catalogData":
                catalog = d.packages || [];
                renderCatalog();
                if (currentPage === "home") {
                    if (detectedEnvironments.length === 0) {
                        // Catalog just loaded — now safe to detect environments
                        document.getElementById("envList").innerHTML = '<div class="empty-state">' + t("env.scanning") + '</div>';
                        sendNative({ action: "detectEnvironments" });
                    } else {
                        // Re-render with catalog now available for proper winget mapping
                        renderEnvironments(document.getElementById("homeSearch").value);
                    }
                }
                break;

            case "installStarted":
                preInstallSnapshotId = d.snapshotId || "";
                break;

            case "installProgress":
                installResults.set(d.packageId, {
                    status: d.status,
                    message: d.message || "",
                    command: d.command || (installResults.has(d.packageId) ? installResults.get(d.packageId).command : ""),
                    output: d.output || "",
                    exitCode: d.exitCode
                });
                if (typeof d.current === "number") installCurrent = d.current;
                if (typeof d.total === "number") installTotal = d.total;
                renderInstallList();
                updateProgressBar();
                persistInstallResults();
                if (d.status === "success") {
                    installedPackages.add(d.packageId);
                    persistInstalledPackages();
                }
                break;

            case "installComplete":
                preInstallSnapshotId = d.snapshotId || preInstallSnapshotId;
                updateProgressBar();
                renderInstallList();
                persistInstallResults();
                break;

            case "installedList":
                installedPackages = new Set(d.packageIds || []);
                persistInstalledPackages();
                if (currentPage === "packages" && catalog.length > 0) {
                    renderCatalog(document.getElementById("pkgSearch").value);
                }
                if (currentPage === "home") {
                    renderEnvironments(document.getElementById("homeSearch").value);
                }
                break;

            case "snapshotCreated":
                if (d.success === false) {
                    showToast(t("recovery.createFailed"), "error");
                } else {
                    showToast(t("recovery.createSuccess"), "success");
                    loadSnapshots();
                }
                break;

            case "snapshotList":
                renderSnapshots(d.snapshots || []);
                break;

            case "restoreResult":
                if (d.success) showToast(t("recovery.restoreSuccess"), "success");
                else showToast(t("recovery.restoreFailed"), "error");
                break;

            case "snapshotDiff":
                showDiffDialog(d.snapshotId, d.diffs || []);
                break;

            case "deleteResult":
                loadSnapshots();
                break;

            case "exportResult":
                if (d.success) showToast(t("recovery.exportSuccess"), "success");
                else showToast(t("recovery.exportFailed"), "error");
                break;

            case "importResult":
                if (d.success) {
                    showToast(t("recovery.importSuccess"), "success");
                    loadSnapshots();
                } else {
                    showToast(t("recovery.importFailed"), "error");
                }
                break;

            case "windowState":
                dbgLog("windowState: max=" + d.maximized
                       + " inner=" + window.innerWidth + "x" + window.innerHeight);
                isMaximized = d.maximized;
                updateMaxBtn();
                // Signal reflow. After minimize→restore the CSS viewport
                // may not change size, so no 'resize' event fires — this
                // guarantees --vh/--vw are current. Uses window.inner*
                // (CSS pixels), never the physical-pixel values from C++.
                // On size-change transitions (maximize/custom resize),
                // the 'resize' event + ResizeObserver also call
                // triggerReflow(); rAF throttling prevents double work.
                triggerReflow();
                break;

            case "uninstallResult":
                if (d.success) {
                    showToast(t("env.uninstallSuccess", d.command), "success");
                    sendNative({ action: "detectEnvironments" });
                } else {
                    showToast(t("env.uninstallFailed"), "error");
                }
                break;

            // Settings: environment variable responses
            case "envVarList":
                envVarCache = d.variables || [];
                renderEnvVarTable();
                updateAdminWarning();
                break;

            case "envVarWriteResult":
                if (d.success) {
                    showToast(t("settings.saveSuccess"), "success");
                    loadEnvVars();
                } else {
                    showToast(t("settings.saveFailed", d.message || ""), "error");
                }
                break;

            case "envVarDeleteResult":
                if (d.success) {
                    showToast(t("settings.deleteSuccess"), "success");
                    loadEnvVars();
                } else {
                    showToast(t("settings.deleteFailed", d.message || ""), "error");
                }
                break;

            // Catch-all for native-side errors (e.g. exceptions caught by try-catch)
            case "error":
                showToast(d.message || "An unexpected error occurred.", "error");
                break;
        }
    }

    // -----------------------------------------------------------------------
    // Mock for dev (no native host)
    // -----------------------------------------------------------------------
    function handleMock(obj) {
        if (obj.action === "detectEnvironments") {
            setTimeout(function () {
                handleNative({
                    action: "environmentsDetected",
                    environments: [
                        { name: "Python", command: "python", version: "Python 3.12.0", category: "language" },
                        { name: "Node.js", command: "node", version: "v22.13.0", category: "language" },
                        { name: "Git", command: "git", version: "git version 2.43.0", category: "tool" },
                        { name: "CMake", command: "cmake", version: "cmake version 3.28.1", category: "tool" },
                        { name: "Rust (rustc)", command: "rustc", version: "rustc 1.75.0", category: "language" },
                        { name: "Docker", command: "docker", version: "Docker version 24.0.7", category: "runtime" },
                        { name: "curl", command: "curl", version: "curl 8.4.0", category: "utility" },
                    ]
                });
            }, 800);
        }
        if (obj.action === "probeCommand") {
            setTimeout(function () {
                handleNative({
                    action: "probeResult",
                    found: true,
                    name: obj.command,
                    command: obj.command,
                    version: obj.command + " 0.6.0 (mock)",
                    category: obj.category || "other"
                });
            }, 600);
        }
        if (obj.action === "checkWinget") {
            setTimeout(function () { handleNative({ action: "wingetStatus", available: true }); }, 300);
        }
        if (obj.action === "getCatalog") {
            setTimeout(function () {
                handleNative({
                    action: "catalogData",
                    packages: [
                        { id: "Python.Python.3.12", name: "Python 3.12", category: "language", description: "General-purpose programming language" },
                        { id: "OpenJS.NodeJS.LTS", name: "Node.js (LTS)", category: "language", description: "JavaScript runtime built on V8" },
                        { id: "Rustlang.Rustup", name: "Rust (rustup)", category: "language", description: "Systems programming language" },
                        { id: "GoLang.Go", name: "Go", category: "language", description: "Statically typed compiled language" },
                        { id: "Git.Git", name: "Git", category: "tool", description: "Distributed version control" },
                        { id: "Kitware.CMake", name: "CMake", category: "tool", description: "Build system generator" },
                        { id: "Microsoft.VisualStudioCode", name: "VS Code", category: "editor", description: "Code editor by Microsoft" },
                        { id: "Docker.DockerDesktop", name: "Docker Desktop", category: "runtime", description: "Container platform" },
                    ]
                });
            }, 200);
        }
        if (obj.action === "selectFolder") {
            // Mock: prompt for folder path (native WebView2 should show a real folder picker)
            setTimeout(function () {
                var path = prompt("Select install location:", currentInstallLocation || "D:\\Tools");
                if (path && path.trim()) {
                    path = path.trim();
                    if (/\s/.test(path)) {
                        showToast(t("packages.noSpacesInPath"), "error");
                    } else {
                        handleNative({ action: "folderSelected", path: path });
                    }
                }
            }, 100);
        }
        if (obj.action === "install") {
            setTimeout(function () { handleNative({ action: "installStarted", snapshotId: "mock-snap" }); }, 100);
            var pkgs = obj.packages || [];
            pkgs.forEach(function (id, i) {
                var cmd = "winget install --id " + id + " --exact --silent --accept-package-agreements --accept-source-agreements";
                if (obj.installLocation) cmd += ' --location "' + obj.installLocation + '"';
                setTimeout(function () {
                    handleNative({ action: "installProgress", packageId: id, status: "running", message: "Installing...", command: cmd, current: i, total: pkgs.length });
                }, 300 + i * 2500);
                for (var l = 0; l < 5; l++) {
                    (function(line, pkgId) {
                        setTimeout(function () {
                            handleNative({ action: "installLog", packageId: pkgId, line: "  [mock] Processing step " + (line + 1) + "..." });
                        }, 600 + i * 2500 + line * 300);
                    })(l, id);
                }
                var success = Math.random() > 0.3;
                setTimeout(function () {
                    handleNative({
                        action: "installProgress", packageId: id,
                        status: success ? "success" : "failed",
                        message: success ? "Done" : "Error: package not found",
                        command: cmd,
                        output: success ? "" : "ERROR: No package found matching input criteria.",
                        exitCode: success ? 0 : 1,
                        current: i + 1, total: pkgs.length
                    });
                }, 1800 + i * 2500);
            });
            setTimeout(function () { handleNative({ action: "installComplete", snapshotId: "mock-snap" }); }, 2500 + pkgs.length * 2500);
        }
        if (obj.action === "retryInstall") {
            var rid = obj.packageId;
            var rcmd = "winget install --id " + rid + " --exact --silent --accept-package-agreements --accept-source-agreements";
            if (obj.installLocation) rcmd += ' --location "' + obj.installLocation + '"';
            setTimeout(function () { handleNative({ action: "installProgress", packageId: rid, status: "running", message: "Retrying...", command: rcmd }); }, 200);
            setTimeout(function () { handleNative({ action: "installProgress", packageId: rid, status: "success", message: "Done", command: rcmd, output: "", exitCode: 0 }); }, 2000);
        }
        if (obj.action === "createSnapshot") {
            setTimeout(function () { handleNative({ action: "snapshotCreated", id: "mock-snap-" + Date.now() }); }, 200);
        }
        if (obj.action === "listSnapshots") {
            setTimeout(function () {
                handleNative({
                    action: "snapshotList",
                    snapshots: [
                        { id: "mock-001", timestamp: "2026-07-30T10:00:00Z", description: "Manual snapshot", userVarCount: 12, systemVarCount: 5 },
                    ]
                });
            }, 200);
        }
        if (obj.action === "restoreSnapshot") {
            setTimeout(function () { handleNative({ action: "restoreResult", success: true, snapshotId: obj.snapshotId || "" }); }, 300);
        }
        if (obj.action === "deleteSnapshot") {
            setTimeout(function () { handleNative({ action: "deleteResult", success: true, snapshotId: obj.snapshotId || "" }); }, 200);
        }
        if (obj.action === "adminCheck") {
            setTimeout(function () { handleNative({ action: "adminStatus", isAdmin: false }); }, 100);
        }
        if (obj.action === "listEnvVars") {
            setTimeout(function () {
                handleNative({
                    action: "envVarList",
                    variables: [
                        { name: "PATH", value: "C:\\Windows\\system32;C:\\Windows;C:\\Program Files\\Git\\cmd", type: "REG_EXPAND_SZ" },
                        { name: "JAVA_HOME", value: "C:\\Program Files\\Java\\jdk-21", type: "REG_SZ" },
                        { name: "GOPATH", value: "C:\\Users\\Rein\\go", type: "REG_SZ" },
                        { name: "CARGO_HOME", value: "C:\\Users\\Rein\\.cargo", type: "REG_SZ" },
                        { name: "TEMP", value: "%USERPROFILE%\\AppData\\Local\\Temp", type: "REG_EXPAND_SZ" },
                    ]
                });
            }, 300);
        }
        if (obj.action === "writeEnvVar") {
            setTimeout(function () { handleNative({ action: "envVarWriteResult", success: true }); }, 300);
        }
        if (obj.action === "deleteEnvVar") {
            setTimeout(function () { handleNative({ action: "envVarDeleteResult", success: true }); }, 300);
        }
        if (obj.action === "checkInstalled") {
            // Mock: simulate some common packages already installed on the system
            setTimeout(function () {
                handleNative({ action: "installedList", packageIds: ["Git.Git", "OpenJS.NodeJS.LTS", "WinSCP.WinSCP"] });
            }, 200);
        }
    }

    // -----------------------------------------------------------------------
    // Persistence helpers
    // -----------------------------------------------------------------------
    function persistInstallResults() {
        try {
            var obj = {};
            installResults.forEach(function (r, id) { obj[id] = r; });
            localStorage.setItem("lazyenv_install_results", JSON.stringify(obj));
        } catch (e) { /* ignore */ }
    }

    function persistInstalledPackages() {
        try {
            localStorage.setItem("lazyenv_installed_packages", JSON.stringify(Array.from(installedPackages)));
        } catch (e) { /* ignore */ }
    }

    function loadPersistedState() {
        try {
            var raw = localStorage.getItem("lazyenv_install_results");
            if (raw) {
                var parsed = JSON.parse(raw);
                Object.keys(parsed).forEach(function (id) {
                    installResults.set(id, parsed[id]);
                });
            }
            var rawInstalled = localStorage.getItem("lazyenv_installed_packages");
            if (rawInstalled) {
                JSON.parse(rawInstalled).forEach(function (id) {
                    installedPackages.add(id);
                });
            }
        } catch (e) { /* ignore corrupt data */ }
    }

    function saveInstallLocation(path) {
        try { localStorage.setItem("lazyenv_install_location", path); } catch (e) { /* ignore */ }
    }

    function loadInstallLocation() {
        try {
            var saved = localStorage.getItem("lazyenv_install_location");
            if (saved) {
                currentInstallLocation = saved;
                var liInput = document.getElementById("txtInstallLocation");
                if (liInput) liInput.value = saved;
                updateLocationTooltip(saved);
            }
        } catch (e) { /* ignore */ }
    }

    // -----------------------------------------------------------------------
    // Navigation
    // -----------------------------------------------------------------------
    function navigateTo(page) {
        currentPage = page;
        document.querySelectorAll(".page").forEach(function (p) {
            p.classList.remove("page--active");
        });
        var target = document.getElementById("page-" + page);
        if (target) target.classList.add("page--active");

        document.querySelectorAll(".sidebar__item").forEach(function (item) {
            item.classList.toggle("sidebar__item--active", item.dataset.page === page);
        });

        if (page === "home") {
            if (catalog.length === 0) {
                // Fetch catalog first; catalogData handler will chain detectEnvironments
                document.getElementById("envList").innerHTML = '<div class="empty-state">' + t("env.scanning") + '</div>';
                sendNative({ action: "getCatalog" });
            } else if (detectedEnvironments.length === 0) {
                document.getElementById("envList").innerHTML = '<div class="empty-state">' + t("env.scanning") + '</div>';
                sendNative({ action: "detectEnvironments" });
            } else {
                renderEnvironments(document.getElementById("homeSearch").value);
            }
        }
        if (page === "settings") { loadEnvVars(); updateAdminWarning(); }
        if (page === "syscheck") initCheck();
        if (page === "packages") {
            if (catalog.length === 0) sendNative({ action: "getCatalog" });
            sendNative({ action: "checkInstalled" });
        }
        if (page === "install") { renderInstallList(); updateProgressBar(); }
        if (page === "recovery") loadSnapshots();
        if (page === "summary") renderSummary();
    }

    document.getElementById("sidebarNav").addEventListener("click", function (e) {
        var item = e.target.closest(".sidebar__item");
        if (item && item.dataset.page) navigateTo(item.dataset.page);
    });

    // Sidebar search - filter nav items
    document.getElementById("sidebarSearch").addEventListener("input", function (e) {
        var q = e.target.value.toLowerCase();
        document.querySelectorAll(".sidebar__item").forEach(function (item) {
            var text = item.textContent.toLowerCase();
            item.style.display = (!q || text.includes(q)) ? "" : "none";
        });
        document.querySelectorAll(".sidebar__section-label").forEach(function (label) {
            label.style.display = q ? "none" : "";
        });
    });

    // -----------------------------------------------------------------------
    // Language switcher
    // -----------------------------------------------------------------------
    var langSelect = document.getElementById("langSelect");
    langSelect.value = window.LazyEnvI18n.getLocale();

    langSelect.addEventListener("change", function () {
        window.LazyEnvI18n.setLocale(langSelect.value);
    });

    window.addEventListener("lazyenv:localeChanged", function () {
        t = window.LazyEnvI18n.t;
        renderEnvironments(document.getElementById("homeSearch").value);
        if (currentPage === "syscheck") renderChecks();
        if (catalog.length > 0) renderCatalog(document.getElementById("pkgSearch").value);
        updateAdminWarning();
        updatePkgCount();
        if (installResults.size > 0) {
            renderInstallList();
            updateProgressBar();
        }
        if (currentPage === "settings") renderEnvVarTable();
        if (currentPage === "recovery") loadSnapshots();
        if (currentPage === "summary") renderSummary();
    });

    // -----------------------------------------------------------------------
    // Window controls
    // -----------------------------------------------------------------------
    document.getElementById("btnMin").addEventListener("click", function () {
        sendNative({ action: "windowMinimize" });
    });
    document.getElementById("btnMax").addEventListener("click", function () {
        sendNative({ action: "windowMaximize" });
    });
    document.getElementById("btnClose").addEventListener("click", function () {
        sendNative({ action: "windowClose" });
    });

    document.getElementById("dragRegion").addEventListener("dblclick", function () {
        sendNative({ action: "windowMaximize" });
    });

    document.getElementById("dragRegion").addEventListener("mousedown", function (e) {
        if (e.button !== 0) return;
        if (e.target.closest("button") || e.target.closest("input") || e.target.closest("a")) return;
        sendNative({ action: "windowDragStart" });
    });

    function updateMaxBtn() {
        var svg = document.getElementById("btnMax").querySelector("svg");
        if (isMaximized) {
            svg.innerHTML = '<rect x="2" y="0" width="7" height="7" stroke="currentColor" stroke-width="1" fill="none"/><rect x="0" y="2" width="7" height="7" stroke="currentColor" stroke-width="1" fill="var(--bg-base)"/>';
        } else {
            svg.innerHTML = '<rect x="0.5" y="0.5" width="9" height="9" stroke="currentColor" stroke-width="1" fill="none"/>';
        }
    }

    // -----------------------------------------------------------------------
    // Home: Installed Environments
    // -----------------------------------------------------------------------
    function getAllEnvironments() {
        var seen = new Set();
        var all = [];
        detectedEnvironments.forEach(function (e) {
            seen.add(e.command.toLowerCase());
            all.push(e);
        });
        manualEnvironments.forEach(function (e) {
            if (!seen.has(e.command.toLowerCase())) {
                seen.add(e.command.toLowerCase());
                all.push(e);
            }
        });
        // Merge winget-installed packages not already in the environment lists
        installedPackages.forEach(function (pkgId) {
            // Look up display name from catalog first
            var catPkg = catalog.find(function (c) { return c.id === pkgId; });
            var displayName = catPkg ? catPkg.name : pkgId;

            // Dedup: check if already shown via command or display name.
            // Also match the last segment of the winget ID (e.g. "Git.Git" -> "git")
            // so CLI-detected tools are not duplicated when catalog isn't loaded yet.
            var pkgIdLower = pkgId.toLowerCase();
            var pkgBase = pkgIdLower;
            var lastDot = pkgIdLower.lastIndexOf(".");
            if (lastDot >= 0) pkgBase = pkgIdLower.substring(lastDot + 1);
            var displayNameLower = displayName.toLowerCase();
            var alreadyShown = Array.from(all).some(function (e) {
                var eCmd = (e.command || "").toLowerCase();
                var eName = (e.name || "").toLowerCase();
                return eCmd === pkgIdLower ||
                       eCmd === pkgBase ||
                       eName === displayNameLower;
            });
            if (alreadyShown) return;

            var displayCategory = catPkg ? (catPkg.category || "gui") : "gui";
            var displayDesc = catPkg ? (catPkg.description || "") : "";

            all.push({
                name: displayName,
                command: pkgId,
                version: displayDesc,
                category: displayCategory,
                source: "winget"
            });
        });
        return all;
    }

    function getCategoryLabel(cat) {
        var key = "category." + cat;
        var result = t(key);
        return result === key ? cat : result;
    }

    function renderEnvironments(filter) {
        var container = document.getElementById("envList");
        var envs = getAllEnvironments();

        if (filter) {
            var f = filter.toLowerCase();
            envs = envs.filter(function (e) {
                return e.name.toLowerCase().includes(f) || e.version.toLowerCase().includes(f) || e.category.toLowerCase().includes(f);
            });
        }

        if (envs.length === 0) {
            container.innerHTML = '<div class="empty-state">' +
                (detectedEnvironments.length === 0 && manualEnvironments.length === 0 ? t("env.scanning") : t("env.noMatch")) +
                '</div>';
            return;
        }

        var groups = {};
        var order = ["language", "tool", "runtime", "utility", "editor", "database", "other"];
        envs.forEach(function (e) {
            var cat = e.category || "other";
            if (!groups[cat]) groups[cat] = [];
            groups[cat].push(e);
        });

        var html = "";
        order.concat(Object.keys(groups).filter(function (k) { return order.indexOf(k) === -1; })).forEach(function (cat) {
            if (!groups[cat]) return;
            html += '<div class="card-section"><div class="card-section__title">' + esc(getCategoryLabel(cat)) + ' (' + groups[cat].length + ')</div>';
            groups[cat].forEach(function (e) {
                var iconSvg = window.LazyEnvIcons.getIcon(e.command || e.name, e.category);
                html += '<div class="card-row" data-cmd="' + esc(e.command) + '">' +
                    '<div class="card-row__icon">' + iconSvg + '</div>' +
                    '<div class="card-row__body">' +
                    '<div class="card-row__title">' + esc(e.name) + '</div>' +
                    '<div class="card-row__subtitle">' + esc(e.version) + '</div>' +
                    '</div>' +
                    '<div class="card-row__actions">' +
                    '<button class="btn btn--sm btn-uninstall">' + esc(t("env.btnUninstall")) + '</button>' +
                    '</div></div>';
            });
            html += '</div>';
        });

        container.innerHTML = html;

        container.querySelectorAll(".btn-uninstall").forEach(function (btn) {
            btn.addEventListener("click", function (ev) {
                ev.stopPropagation();
                var row = btn.closest(".card-row");
                var name = row.querySelector(".card-row__title").textContent;
                showDialog(
                    t("env.confirmUninstallTitle"),
                    t("env.confirmUninstall", name),
                    [
                        { text: t("dialog.cancel"), cls: "" },
                        { text: t("env.btnUninstall"), cls: "btn--danger", action: function () { sendNative({ action: "uninstallPackage", command: name }); } }
                    ]
                );
            });
        });
    }

    document.getElementById("homeSearch").addEventListener("input", function (e) {
        renderEnvironments(e.target.value);
    });

    document.getElementById("btnRefreshEnv").addEventListener("click", function () {
        document.getElementById("envList").innerHTML = '<div class="empty-state">' + t("env.scanning") + '</div>';
        detectedEnvironments = [];
        sendNative({ action: "detectEnvironments" });
    });

    // -----------------------------------------------------------------------
    // Home: Manual Add Environment
    // -----------------------------------------------------------------------
    var addEnvPanel = document.getElementById("addEnvPanel");

    document.getElementById("btnAddEnv").addEventListener("click", function () {
        addEnvPanel.classList.toggle("add-panel--visible");
        if (addEnvPanel.classList.contains("add-panel--visible")) {
            document.getElementById("addEnvCmd").focus();
        }
    });

    document.getElementById("btnCloseAddPanel").addEventListener("click", function () {
        addEnvPanel.classList.remove("add-panel--visible");
    });

    document.getElementById("btnDetectEnv").addEventListener("click", function () {
        var cmd = document.getElementById("addEnvCmd").value.trim();
        if (!cmd) return;
        var cat = document.getElementById("addEnvCategory").value;
        sendNative({ action: "probeCommand", command: cmd, category: cat });
    });

    document.getElementById("addEnvCmd").addEventListener("keydown", function (e) {
        if (e.key === "Enter") {
            e.preventDefault();
            document.getElementById("btnDetectEnv").click();
        }
    });

    function handleProbeResult(d) {
        if (d.found) {
            manualEnvironments.push({
                name: d.name,
                command: d.command,
                version: d.version,
                category: d.category || "other"
            });
            document.getElementById("addEnvCmd").value = "";
            renderEnvironments(document.getElementById("homeSearch").value);
            showToast(t("probe.addedToast", d.name, d.version), "success");
        } else {
            showToast(d.message || t("probe.notFound"), "error");
        }
    }

    // -----------------------------------------------------------------------
    // Settings: Environment Variable Editor
    // -----------------------------------------------------------------------
    function updateAdminWarning() {
        var banner = document.getElementById("adminWarning");
        var addBtn = document.getElementById("btnAddEnvVar");
        if (!banner) return;

        if (isAdmin) {
            banner.classList.add("hidden");
            if (addBtn) addBtn.style.display = "";
        } else {
            if (envVarScope === "system"){
               banner.classList.remove("hidden");
            }else{
                banner.classList.add("hidden");
            }
            // Disable "Add Variable" button on system scope
            if (envVarScope === "system" && addBtn) addBtn.style.display = "none";
            else if (addBtn) addBtn.style.display = "";
        }
        // Re-render table to reflect disabled edit/delete on sys vars
        if (currentPage === "settings" && envVarCache.length > 0) renderEnvVarTable();
    }

    function loadEnvVars() {
        sendNative({ action: "listEnvVars", scope: envVarScope });
    }

    // Tab switching
    document.getElementById("envvarTabs").addEventListener("click", function (e) {
        var tab = e.target.closest(".tab");
        if (!tab || !tab.dataset.scope) return;
        envVarScope = tab.dataset.scope;
        document.querySelectorAll("#envvarTabs .tab").forEach(function (t) {
            t.classList.toggle("tab--active", t.dataset.scope === envVarScope);
        });
        loadEnvVars();
        updateAdminWarning();
    });

    function renderEnvVarTable() {
        var tbody = document.getElementById("envvarBody");
        var filter = document.getElementById("envvarSearch").value.toLowerCase();
        var vars = envVarCache;

        if (filter) {
            vars = vars.filter(function (v) {
                return v.name.toLowerCase().includes(filter) || v.value.toLowerCase().includes(filter);
            });
        }

        if (vars.length === 0) {
            tbody.innerHTML = '<tr><td colspan="3" class="empty-state">' +
                (envVarCache.length === 0 ? t("settings.loading") : t("settings.noMatch")) +
                '</td></tr>';
            return;
        }
       
        var html = "";
        var sysLock = !isAdmin && envVarScope === "system";
        vars.forEach(function (v) {
            var isPath = v.name.toUpperCase() === "PATH" || v.name.toUpperCase() === "PATHEXT";
            var displayVal = v.value;
            if (isPath && displayVal.length > 80) {
                displayVal = displayVal.substring(0, 80) + "...";
            }
            html += '<tr data-name="' + esc(v.name) + '">' +
                '<td class="envvar-name">' + esc(v.name) +
                (v.type === "REG_EXPAND_SZ" ? ' <span class="envvar-type-badge" title="可展开变量 — 值含 %VAR% 引用，Windows 读取时自动展开">EXP</span>' : '') +
                '</td>' +
                '<td class="envvar-value">' + esc(displayVal) + '</td>' +
                '<td class="envvar-actions">' +
                (sysLock
                    ? '<span class="envvar-lock" title="' + esc(t("settings.adminRequired")) + '">' +
                      '<svg width="14" height="14" viewBox="0 0 16 16" fill="currentColor"><path d="M8 1a2 2 0 0 1 2 2v4H6V3a2 2 0 0 1 2-2zm3 6V3a3 3 0 0 0-6 0v4a2 2 0 0 0-2 2v5a2 2 0 0 0 2 2h6a2 2 0 0 0 2-2V9a2 2 0 0 0-2-2z"/></svg>' +
                      '</span>'
                    : '<button class="btn btn--sm btn-edit-var" title="' + esc(t("settings.edit")) + '">' +
                      '<svg width="12" height="12" viewBox="0 0 16 16" fill="currentColor"><path d="M12.146.146a.5.5 0 0 1 .708 0l3 3a.5.5 0 0 1 0 .708l-10 10a.5.5 0 0 1-.168.11l-5 2a.5.5 0 0 1-.65-.65l2-5a.5.5 0 0 1 .11-.168l10-10zM11.207 2.5L13.5 4.793 14.793 3.5 12.5 1.207 11.207 2.5zm1.586 3L10.5 3.207 4 9.707V10h.5a.5.5 0 0 1 .5.5v.5h.5a.5.5 0 0 1 .5.5v.5h.293l6.5-6.5zm-9.761 5.175l-.106.106-1.528 3.821 3.821-1.528.106-.106A.5.5 0 0 1 5 12.5V12h-.5a.5.5 0 0 1-.5-.5V11h-.5a.5.5 0 0 1-.468-.325z"/></svg>' +
                      '</button>' +
                      '<button class="btn btn--sm btn--danger btn-delete-var" title="' + esc(t("settings.delete")) + '">' +
                      '<svg width="12" height="12" viewBox="0 0 16 16" fill="currentColor"><path d="M5.5 5.5A.5.5 0 0 1 6 6v6a.5.5 0 0 1-1 0V6a.5.5 0 0 1 .5-.5zm2.5 0a.5.5 0 0 1 .5.5v6a.5.5 0 0 1-1 0V6a.5.5 0 0 1 .5-.5zm3 .5a.5.5 0 0 0-1 0v6a.5.5 0 0 0 1 0V6z"/><path fill-rule="evenodd" d="M14.5 3a1 1 0 0 1-1 1H13v9a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V4h-.5a1 1 0 0 1-1-1V2a1 1 0 0 1 1-1H5.5l1-1h3l1 1H13a1 1 0 0 1 1 1v1zM4.118 4L4 4.059V13a1 1 0 0 0 1 1h6a1 1 0 0 0 1-1V4.059L11.882 4H4.118zM2.5 3V2h11v1h-11z"/></svg>' +
                      '</button>')
                + '</td></tr>';
        });

        tbody.innerHTML = html;

        // Double-click row to view variable details (skip if clicking on action buttons)
        tbody.querySelectorAll("tr[data-name]").forEach(function (row) {
            row.addEventListener("dblclick", function (e) {
                if (e.target.closest(".envvar-actions") || e.target.closest("button")) return;
                var name = row.dataset.name;
                var v = envVarCache.find(function (x) { return x.name === name; });
                if (v) showEnvVarViewDialog(v.name, v.value, v.type);
            });
        });

        // Bind edit buttons
        tbody.querySelectorAll(".btn-edit-var").forEach(function (btn) {
            btn.addEventListener("click", function () {
                var name = btn.closest("tr").dataset.name;
                var v = envVarCache.find(function (x) { return x.name === name; });
                if (v) showEnvVarEditDialog(v.name, v.value, v.type, false);
            });
        });

        // Bind delete buttons
        tbody.querySelectorAll(".btn-delete-var").forEach(function (btn) {
            btn.addEventListener("click", function () {
                var name = btn.closest("tr").dataset.name;
                showDialog(
                    t("settings.confirmDeleteTitle"),
                    t("settings.confirmDelete", name),
                    [
                        { text: t("dialog.cancel"), cls: "" },
                        { text: t("settings.delete"), cls: "btn--danger", action: function () {
                            sendNative({ action: "deleteEnvVar", name: name, scope: envVarScope });
                        }}
                    ]
                );
            });
        });
    }

    document.getElementById("envvarSearch").addEventListener("input", function () {
        renderEnvVarTable();
    });

    // New variable button
    document.getElementById("btnAddEnvVar").addEventListener("click", function () {
        showEnvVarEditDialog("", "", "REG_SZ", true);
    });

    // Read-only view dialog for environment variable details
    function showEnvVarViewDialog(name, value, type) {
        var title = t("settings.viewVarTitle", name);
        var scopeLabel = envVarScope === "system" ? t("settings.viewScopeSystem") : t("settings.viewScopeUser");
        var isPathVar = name.toUpperCase() === "PATH";

        var bodyHtml = '<div class="dialog-form">';
        bodyHtml += '<div class="dialog-form__group">';
        bodyHtml += '<label class="dialog-form__label">' + esc(t("settings.colName")) + '</label>';
        bodyHtml += '<div class="dialog-view-value">' + esc(name) + '</div>';
        bodyHtml += '</div>';

        bodyHtml += '<div class="dialog-form__group">';
        bodyHtml += '<label class="dialog-form__label">' + esc(t("settings.colValue")) + '</label>';
        if (isPathVar) {
            var pathEntries = value.split(";").filter(function (p) { return p.trim(); });
            bodyHtml += '<div class="dialog-path-readonly">';
            pathEntries.forEach(function (entry) {
                bodyHtml += '<div class="path-row">' + esc(entry) + '</div>';
            });
            bodyHtml += '</div>';
        } else {
            if (value.length > 200) {
                bodyHtml += '<textarea class="input input--mono input--readonly dialog-form__textarea" rows="6" readonly>' + esc(value) + '</textarea>';
            } else {
                bodyHtml += '<div class="dialog-view-value">' + esc(value) + '</div>';
            }
        }
        bodyHtml += '</div>';

        bodyHtml += '<div class="dialog-form__group">';
        bodyHtml += '<label class="dialog-form__label">' + esc(t("settings.type")) + '</label>';
        bodyHtml += '<div class="dialog-view-value">' + esc(type) +
            (type === "REG_EXPAND_SZ" ? ' <span class="envvar-type-badge">EXP</span>' : '') +
            '</div>';
        bodyHtml += '</div>';

        bodyHtml += '<div class="dialog-form__group">';
        bodyHtml += '<label class="dialog-form__label">' + esc(t("settings.viewScope")) + '</label>';
        bodyHtml += '<div class="dialog-view-value">' + esc(scopeLabel) + '</div>';
        bodyHtml += '</div>';
        bodyHtml += '</div>';

        showDialogRaw(title, bodyHtml, [
            { text: t("settings.viewClose"), cls: "btn--accent" }
        ]);
    }

    function showEnvVarEditDialog(name, value, type, isNew) {
        var isPathVar = name.toUpperCase() === "PATH";
        var title = isNew ? t("settings.newVarTitle") : t("settings.editVarTitle", name);

        var bodyHtml = '<div class="dialog-form">';
        bodyHtml += '<div class="dialog-form__group">';
        bodyHtml += '<label class="dialog-form__label">' + esc(t("settings.colName")) + '</label>';
        bodyHtml += '<input type="text" class="input" id="dlgVarName" value="' + esc(name) + '"' + (isNew ? '' : ' readonly') + '>';
        bodyHtml += '</div>';

        bodyHtml += '<div class="dialog-form__group">';
        bodyHtml += '<label class="dialog-form__label">' + esc(t("settings.colValue")) + '</label>';

        if (isPathVar) {
            // PATH editor: one entry per line
            var pathEntries = value.split(";").filter(function (p) { return p.trim(); });
            bodyHtml += '<div class="path-editor" id="dlgPathEditor">';
            pathEntries.forEach(function (entry, idx) {
                bodyHtml += '<div class="path-entry">' +
                    '<input type="text" class="input input--mono path-input" value="' + esc(entry) + '">' +
                    '<button class="btn--icon path-remove" data-idx="' + idx + '">' +
                    '<svg width="10" height="10" viewBox="0 0 10 10"><line x1="0" y1="0" x2="10" y2="10" stroke="currentColor" stroke-width="1.5"/><line x1="10" y1="0" x2="0" y2="10" stroke="currentColor" stroke-width="1.5"/></svg>' +
                    '</button></div>';
            });
            bodyHtml += '</div>';
            bodyHtml += '<button class="btn btn--sm mt-sm" id="dlgPathAdd">' + esc(t("settings.pathAdd")) + '</button>';
        } else {
            bodyHtml += '<textarea class="input input--mono dialog-form__textarea" id="dlgVarValue" rows="4">' + esc(value) + '</textarea>';
        }
        bodyHtml += '</div>';

        bodyHtml += '<div class="dialog-form__group">';
        bodyHtml += '<label class="dialog-form__label">' + esc(t("settings.type")) + '</label>';
        bodyHtml += '<select class="input" id="dlgVarType">';
        bodyHtml += '<option value="REG_SZ"' + (type === "REG_SZ" ? ' selected' : '') + '>REG_SZ</option>';
        bodyHtml += '<option value="REG_EXPAND_SZ"' + (type === "REG_EXPAND_SZ" ? ' selected' : '') + '>REG_EXPAND_SZ</option>';
        bodyHtml += '</select>';
        bodyHtml += '</div>';
        bodyHtml += '</div>';

        showDialogRaw(title, bodyHtml, [
            { text: t("dialog.cancel"), cls: "" },
            { text: t("settings.save"), cls: "btn--accent", action: function () {
                var newName = document.getElementById("dlgVarName").value.trim();
                if (!newName) { showToast(t("settings.nameRequired"), "error"); return false; }
                var newValue;
                if (isPathVar || (newName.toUpperCase() === "PATH")) {
                    var inputs = document.querySelectorAll("#dlgPathEditor .path-input");
                    var parts = [];
                    inputs.forEach(function (inp) { if (inp.value.trim()) parts.push(inp.value.trim()); });
                    newValue = parts.join(";");
                } else {
                    newValue = document.getElementById("dlgVarValue").value;
                }
                var newType = document.getElementById("dlgVarType").value;
                sendNative({
                    action: "writeEnvVar",
                    name: newName,
                    value: newValue,
                    type: newType,
                    scope: envVarScope
                });
            }}
        ]);

        // PATH editor: add/remove entries
        if (isPathVar) {
            var editor = document.getElementById("dlgPathEditor");
            document.getElementById("dlgPathAdd").addEventListener("click", function () {
                var div = document.createElement("div");
                div.className = "path-entry";
                div.innerHTML = '<input type="text" class="input input--mono path-input" value="">' +
                    '<button class="btn--icon path-remove"><svg width="10" height="10" viewBox="0 0 10 10"><line x1="0" y1="0" x2="10" y2="10" stroke="currentColor" stroke-width="1.5"/><line x1="10" y1="0" x2="0" y2="10" stroke="currentColor" stroke-width="1.5"/></svg></button>';
                editor.appendChild(div);
                div.querySelector("input").focus();
                div.querySelector(".path-remove").addEventListener("click", function () { div.remove(); });
            });
            editor.querySelectorAll(".path-remove").forEach(function (btn) {
                btn.addEventListener("click", function () { btn.closest(".path-entry").remove(); });
            });
        }
    }

    // -----------------------------------------------------------------------
    // System Check
    // -----------------------------------------------------------------------
    var checkStates = { os: null, webview2: true, winget: null };

    function initCheck() {
        checkStates.os = "Windows";
        sendNative({ action: "checkWinget" });
        renderChecks();
    }

    function renderCheckItem(name, ok) {
        checkStates[name] = ok;
        renderChecks();
    }

    function renderChecks() {
        var items = [
            { label: t("check.os"), value: checkStates.os || t("check.detecting"), ok: checkStates.os === "Windows" ? "ok" : null },
            { label: t("check.webview2"), value: t("check.available"), ok: "ok" },
            { label: t("check.winget"), value: checkStates.winget === null ? t("check.checking") : (checkStates.winget ? t("check.available") : t("check.notFound")), ok: checkStates.winget === null ? null : (checkStates.winget ? "ok" : "fail") }
        ];

        var container = document.getElementById("checkResults");
        container.innerHTML = items.map(function (c) {
            var icon = "";
            if (c.ok === "ok") icon = '<svg width="18" height="18" viewBox="0 0 16 16" fill="var(--status-success)"><path d="M16 8A8 8 0 1 1 0 8a8 8 0 0 1 16 0zm-3.97-3.03a.75.75 0 0 0-1.08.022L7.477 9.417 5.384 7.323a.75.75 0 0 0-1.06 1.06L6.97 11.03a.75.75 0 0 0 1.079-.02l3.992-4.99a.75.75 0 0 0-.01-1.05z"/></svg>';
            else if (c.ok === "fail") icon = '<svg width="18" height="18" viewBox="0 0 16 16" fill="var(--status-error)"><path d="M16 8A8 8 0 1 1 0 8a8 8 0 0 1 16 0zM5.354 4.646a.5.5 0 1 0-.708.708L7.293 8l-2.647 2.646a.5.5 0 0 0 .708.708L8 8.707l2.646 2.647a.5.5 0 0 0 .708-.708L8.707 8l2.647-2.646a.5.5 0 0 0-.708-.708L8 7.293 5.354 4.646z"/></svg>';
            else icon = '<div class="spinner-sm"></div>';
            return '<div class="card-row"><div class="card-row__icon">' + icon + '</div>' +
                '<div class="card-row__body"><div class="card-row__title">' + esc(c.label) + '</div>' +
                '<div class="card-row__subtitle">' + esc(c.value) + '</div></div></div>';
        }).join("");
    }

    document.getElementById("btnRunChecks").addEventListener("click", function () {
        checkStates = { os: null, webview2: true, winget: null };
        initCheck();
    });

    // -----------------------------------------------------------------------
    // Packages
    // -----------------------------------------------------------------------
    function renderCatalog(filter) {
        var container = document.getElementById("packageList");
        var pkgs = catalog;

        if (filter) {
            var f = filter.toLowerCase();
            pkgs = pkgs.filter(function (p) {
                return p.name.toLowerCase().includes(f) || p.id.toLowerCase().includes(f) || p.category.toLowerCase().includes(f);
            });
        }

        var groups = {};
        var order = ["language", "tool", "editor", "runtime", "database", "utility"];
        pkgs.forEach(function (p) {
            var cat = p.category || "other";
            if (!groups[cat]) groups[cat] = [];
            groups[cat].push(p);
        });

        var html = "";
        order.concat(Object.keys(groups).filter(function (k) { return order.indexOf(k) === -1; })).forEach(function (cat) {
            if (!groups[cat]) return;
            html += '<div class="card-section"><div class="card-section__title">' + esc(getCategoryLabel(cat)) + '</div>';
            groups[cat].forEach(function (p) {
                var isInstalled = installedPackages.has(p.id);
                var sel = selectedPackages.has(p.id);
                var iconSvg = window.LazyEnvIcons.getIcon(p.id || p.name, p.category);
                var cls = isInstalled ? ' card-row--installed' : ' card-row--selectable';
                if (sel) cls += ' card-row--selected';
                html += '<div class="card-row' + cls + '" data-id="' + esc(p.id) + '">' +
                    '<div class="card-row__check">' + (isInstalled
                        ? '<svg width="16" height="16" viewBox="0 0 16 16" fill="var(--status-success)"><path d="M2 0a2 2 0 0 0-2 2v12a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V2a2 2 0 0 0-2-2H2zm10.03 4.97a.75.75 0 0 1 .011 1.05l-3.992 4.99a.75.75 0 0 1-1.08.02L4.324 8.384a.75.75 0 1 1 1.06-1.06l2.094 2.093 3.473-4.425a.75.75 0 0 1 1.08-.022z"/></svg>'
                        : '<svg width="16" height="16" viewBox="0 0 16 16" fill="currentColor"><path d="M13.854 3.646a.5.5 0 0 1 0 .708l-7 7a.5.5 0 0 1-.708 0l-3.5-3.5a.5.5 0 1 1 .708-.708L6.5 10.293l6.646-6.647a.5.5 0 0 1 .708 0z"/></svg>'
                    ) + '</div>' +
                    '<div class="card-row__icon">' + iconSvg + '</div>' +
                    '<div class="card-row__body">' +
                    '<div class="card-row__title">' + esc(p.name) + '</div>' +
                    '<div class="card-row__subtitle">' + esc(p.description) + '</div>' +
                    '</div>' +
                    (isInstalled ? '<span class="badge badge--success" style="margin-right:8px">' + esc(t("packages.installed")) + '</span>' : '') +
                    '</div>';
            });
            html += '</div>';
        });

        container.innerHTML = html;

        container.querySelectorAll(".card-row--selectable").forEach(function (row) {
            row.addEventListener("click", function () {
                var id = row.dataset.id;
                if (selectedPackages.has(id)) { selectedPackages.delete(id); row.classList.remove("card-row--selected"); }
                else { selectedPackages.add(id); row.classList.add("card-row--selected"); }
                updatePkgCount();
            });
        });
    }

    function updatePkgCount() {
        var n = selectedPackages.size;
        document.getElementById("pkgSelectedCount").textContent = t("packages.selectedCount", n);
        document.getElementById("btnStartInstall").disabled = n === 0;
    }

    document.getElementById("pkgSearch").addEventListener("input", function (e) {
        renderCatalog(e.target.value);
    });

    document.getElementById("btnSelectAll").addEventListener("click", function () {
        catalog.forEach(function (p) {
            if (!installedPackages.has(p.id)) selectedPackages.add(p.id);
        });
        renderCatalog(document.getElementById("pkgSearch").value);
        updatePkgCount();
    });

    document.getElementById("btnDeselectAll").addEventListener("click", function () {
        selectedPackages.clear();
        renderCatalog(document.getElementById("pkgSearch").value);
        updatePkgCount();
    });

    document.getElementById("btnStartInstall").addEventListener("click", function () {
        if (selectedPackages.size === 0) return;
        navigateTo("install");
        startInstall();
    });

    // -----------------------------------------------------------------------
    // Installation
    // -----------------------------------------------------------------------
    function startInstall() {
        installResults.clear();

        // Read and validate custom install location (from titlebar)
        var locInput = document.getElementById("txtInstallLocation");
        var rawLoc = (locInput.value || "").trim();
        currentInstallLocation = "";

        if (rawLoc) {
            // Basic validation: must contain a drive letter followed by colon and backslash
            if (!/^[A-Za-z]:\\/.test(rawLoc)) {
                showToast(t("packages.noDriveSpaceAfterColon"), "error");
                locInput.focus();
                return;
            }
            // Reject paths with spaces: some installers tokenize --location by space
            if (/\s/.test(rawLoc)) {
                showToast(t("packages.noSpacesInPath"), "error");
                locInput.focus();
                return;
            }
            currentInstallLocation = rawLoc;
            saveInstallLocation(rawLoc);
        } else {
            currentInstallLocation = "";
            saveInstallLocation("");
        }

        // Split into to-install and already-installed
        var toInstall = [];
        selectedPackages.forEach(function (id) {
            if (installedPackages.has(id)) {
                installResults.set(id, { status: "skipped", message: t("packages.alreadyInstalled"), command: "", output: "" });
            } else {
                toInstall.push(id);
                installResults.set(id, { status: "pending", message: t("install.waiting"), command: "", output: "" });
            }
        });

        installTotal = toInstall.length;
        installCurrent = 0;
        renderInstallList();
        updateProgressBar();

        if (toInstall.length > 0) {
            sendNative({ action: "install", packages: toInstall, installLocation: currentInstallLocation });
        }
    }

    function renderInstallList() {
        var list = document.getElementById("installList");
        var html = "";

        installResults.forEach(function (r, id) {
            var pkg = catalog.find(function (p) { return p.id === id; });
            var name = pkg ? pkg.name : id;
            var iconHtml = "";
            var cls = "";
            var statusText = "";

            switch (r.status) {
                case "pending":
                    iconHtml = '<div class="status-dot status-dot--pending"></div>';
                    statusText = t("install.pending");
                    break;
                case "running":
                    iconHtml = '<div class="spinner-sm"></div>';
                    cls = " card-row--running";
                    statusText = t("install.installing");
                    break;
                case "success":
                    iconHtml = '<svg width="16" height="16" viewBox="0 0 16 16" fill="var(--status-success)"><path d="M16 8A8 8 0 1 1 0 8a8 8 0 0 1 16 0zm-3.97-3.03a.75.75 0 0 0-1.08.022L7.477 9.417 5.384 7.323a.75.75 0 0 0-1.06 1.06L6.97 11.03a.75.75 0 0 0 1.079-.02l3.992-4.99a.75.75 0 0 0-.01-1.05z"/></svg>';
                    cls = " card-row--success";
                    statusText = t("install.installed");
                    break;
                case "failed":
                    iconHtml = '<svg width="16" height="16" viewBox="0 0 16 16" fill="var(--error)"><path d="M16 8A8 8 0 1 1 0 8a8 8 0 0 1 16 0zM5.354 4.646a.5.5 0 1 0-.708.708L7.293 8l-2.647 2.646a.5.5 0 0 0 .708.708L8 8.707l2.646 2.647a.5.5 0 0 0 .708-.708L8.707 8l2.647-2.646a.5.5 0 0 0-.708-.708L8 7.293 5.354 4.646z"/></svg>';
                    cls = " card-row--failed";
                    statusText = t("install.failed", r.exitCode);
                    break;
            }

            var pkgIcon = window.LazyEnvIcons.getIcon(id || name, pkg ? pkg.category : "");
            var itemCls = "install-item" + cls;
            if (r.status === "running") itemCls += " install-item--expanded";
            html += '<div class="' + itemCls + '" data-pkg-id="' + esc(id) + '">';
            html += '<div class="card-row install-item__header">';
            html += '<div class="card-row__icon">' + pkgIcon + '</div>';
            html += '<div class="card-row__status">' + iconHtml + '</div>';
            html += '<div class="card-row__body">';
            html += '<div class="card-row__title">' + esc(name) + '</div>';
            html += '<div class="card-row__subtitle">' + esc(statusText) + '</div>';
            html += '</div>';
            if (r.status === "failed") {
                html += '<div class="card-row__actions"><button class="btn btn--sm btn--accent btn-retry" data-id="' + esc(id) + '">' + esc(t("install.btnRetry")) + '</button></div>';
            }
            html += '</div>';

            // Command + log output
            if (r.command) {
                var cmdEscaped = esc(r.command);
                html += '<div class="install-item__cmd-wrap"><div class="install-item__cmd" title="' + cmdEscaped + '">' + cmdEscaped + '</div></div>';
            }

            var outText = r.output || r.message || "";
            if (outText) {
                html += '<div class="install-item__log" id="log-' + esc(id) + '"><div class="log-line">' + esc(outText) + '</div></div>';
            } else if (r.status === "running") {
                html += '<div class="install-item__log" id="log-' + esc(id) + '"><div class="log-line log-line--dim">' + esc(t("install.waitingOutput")) + '</div></div>';
            }

            html += '</div>';
        });

        list.innerHTML = html || '<div class="empty-state">' + esc(t("install.noPackages")) + '</div>';

        // Retry buttons
        list.querySelectorAll(".btn-retry").forEach(function (btn) {
            btn.addEventListener("click", function () {
                var pkgId = btn.dataset.id;
                installResults.set(pkgId, { status: "pending", message: t("install.retrying"), command: "", output: "" });
                renderInstallList();
                sendNative({ action: "retryInstall", packageId: pkgId, installLocation: currentInstallLocation });
            });
        });

        // Click header to toggle log expand/collapse
        list.querySelectorAll(".install-item__header").forEach(function (header) {
            header.addEventListener("click", function () {
                var item = this.closest(".install-item");
                if (item) item.classList.toggle("install-item--expanded");
            });
        });

    }

    function updateProgressBar() {
        var wrap = document.getElementById("installProgressWrap");
        var fill = document.getElementById("installProgressFill");
        var text = document.getElementById("installProgress");
        var pct = document.getElementById("installPercent");

        if (installResults.size === 0) {
            if (wrap) wrap.classList.add("hidden");
            return;
        }
        if (wrap) wrap.classList.remove("hidden");

        var done = 0;
        var failed = 0;
        installResults.forEach(function (r) {
            if (r.status === "success" || r.status === "skipped" || r.status === "failed") done++;
            if (r.status === "failed") failed++;
        });

        var total = installResults.size || 1;
        var percent = Math.round((done / total) * 100);

        fill.style.width = percent + "%";
        if (failed > 0 && done === total) fill.classList.add("progress-bar__fill--error");
        else fill.classList.remove("progress-bar__fill--error");

        text.textContent = t("install.progressText", done, total);
        pct.textContent = percent + "%";
    }

    // -----------------------------------------------------------------------
    // Recovery
    // -----------------------------------------------------------------------
    function loadSnapshots() {
        sendNative({ action: "listSnapshots" });
    }

    function renderSnapshots(snaps) {
        var list = document.getElementById("snapshotList");
        if (snaps.length === 0) {
            list.innerHTML = '<div class="empty-state">' + esc(t("recovery.emptyState")) + '</div>';
            return;
        }

        var html = "";
        snaps.forEach(function (s) {
            html += '<div class="card-row" data-id="' + esc(s.id) + '">' +
                '<div class="card-row__icon"><svg width="18" height="18" viewBox="0 0 16 16" fill="var(--accent)"><path d="M8 3a5 5 0 1 0 4.546 2.914.5.5 0 0 1 .908-.417A6 6 0 1 1 8 2v1z"/><path d="M8 4.466V.534a.25.25 0 0 1 .41-.192l2.36 1.966c.12.1.12.284 0 .384L8.41 4.658A.25.25 0 0 1 8 4.466z"/></svg></div>' +
                '<div class="card-row__body">' +
                '<div class="card-row__title">' + esc(s.description) + '</div>' +
                '<div class="card-row__subtitle">' + esc(s.timestamp) + ' | ' + t("recovery.userCount", s.userVarCount || 0) + ' | ' + t("recovery.systemCount", s.systemVarCount || 0) + '</div>' +
                '</div>' +
                '<div class="card-row__actions">' +
                '<button class="btn btn--sm btn-restore">' + esc(t("recovery.btnRestore")) + '</button>' +
                '<button class="btn btn--sm btn-export">' + esc(t("recovery.btnExport")) + '</button>' +
                '<button class="btn btn--sm btn--danger btn-delete">' + esc(t("recovery.btnDelete")) + '</button>' +
                '</div></div>';
        });

        list.innerHTML = html;

        list.querySelectorAll(".card-row").forEach(function (row) {
            var id = row.dataset.id;
            if (!id) return;
            row.querySelector(".btn-restore").addEventListener("click", function (e) {
                e.stopPropagation();
                // One-step restore: compute diff immediately, show single dialog
                sendNative({ action: "diffSnapshot", snapshotId: id });
            });
            row.querySelector(".btn-export").addEventListener("click", function (e) {
                e.stopPropagation();
                sendNative({ action: "exportSnapshot", snapshotId: id });
            });
            row.querySelector(".btn-delete").addEventListener("click", function (e) {
                e.stopPropagation();
                showDialog(
                    t("recovery.confirmDeleteTitle"),
                    t("recovery.confirmDelete"),
                    [
                        { text: t("dialog.cancel"), cls: "" },
                        { text: t("recovery.btnDelete"), cls: "btn--danger", action: function () { sendNative({ action: "deleteSnapshot", snapshotId: id }); } }
                    ]
                );
            });
        });
    }

    document.getElementById("btnCreateSnapshot").addEventListener("click", function () {
        showDialogRaw(
            t("recovery.createTitle"),
            '<div class="dialog-form"><div class="dialog-form__group">' +
            '<label class="dialog-form__label">' + esc(t("recovery.descLabel")) + '</label>' +
            '<input type="text" class="input" id="dlgSnapDesc" value="">' +
            '</div></div>',
            [
                { text: t("dialog.cancel"), cls: "" },
                { text: t("recovery.create"), cls: "btn--accent", action: function () {
                    var desc = document.getElementById("dlgSnapDesc").value.trim() || t("recovery.defaultDesc");
                    sendNative({ action: "createSnapshot", description: desc });
                }}
            ]
        );
        setTimeout(function () { var el = document.getElementById("dlgSnapDesc"); if (el) el.focus(); }, 100);
    });

    document.getElementById("btnImportSnapshot").addEventListener("click", function () {
        sendNative({ action: "importSnapshot" });
    });

    // -----------------------------------------------------------------------
    // Summary
    // -----------------------------------------------------------------------
    function clearInstallHistory() {
        installResults.clear();
        persistInstallResults();
        renderSummary();
    }
    window.clearInstallHistory = clearInstallHistory;

    function renderSummary() {
        var container = document.getElementById("summaryContent");
        if (installResults.size === 0) {
            container.innerHTML = '<div class="empty-state">' + esc(t("summary.emptyState")) + '</div>';
            return;
        }

        var html = '';
        html += '<div class="summary-header">';
        html += '<span class="summary-header__count">' + esc(t("summary.recordCount", installResults.size)) + '</span>';
        html += '<button type="button" class="btn btn--sm btn--danger-outline" onclick="clearInstallHistory()">'
            + '<svg class="btn-icon-svg" viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round">'
            + '<polyline points="1 4 3 4 15 4"/>'
            + '<path d="M5 4V2.5a1 1 0 011-1h4a1 1 0 011 1V4"/>'
            + '<path d="M3 4l1.2 9.6a1 1 0 001 .9h5.6a1 1 0 001-.9L13 4"/>'
            + '</svg>'
            + esc(t("summary.clearHistory")) + '</button>';
        html += '</div>';

        html += '<table class="data-table"><thead><tr>' +
            '<th>' + esc(t("summary.colName")) + '</th>' +
            '<th>' + esc(t("summary.colStatus")) + '</th>' +
            '<th>' + esc(t("summary.colCommand")) + '</th>' +
            '<th>' + esc(t("summary.colMessage")) + '</th>' +
            '</tr></thead><tbody>';

        installResults.forEach(function (r, id) {
            var pkg = catalog.find(function (p) { return p.id === id; });
            var name = pkg ? pkg.name : id;
            var badgeCls = "";
            var badgeText = "";
            switch (r.status) {
                case "success": badgeCls = "badge--success"; badgeText = t("summary.success"); break;
                case "failed":  badgeCls = "badge--error";   badgeText = t("summary.failed");  break;
                case "skipped": badgeCls = "badge--warning"; badgeText = t("summary.skipped"); break;
                default:        badgeCls = "";               badgeText = r.status;  break;
            }
            var cmd = r.command || "-";
            var cmdEsc = esc(cmd);
            html += '<tr><td>' + esc(name) + '</td>' +
                '<td><span class="badge ' + badgeCls + '">' + badgeText + '</span></td>' +
                '<td class="text-mono text-sm col-command"><code class="cmd-text" title="' + cmdEsc + '">' + cmdEsc + '</code></td>' +
                '<td class="text-sm summary-detail">' + esc(r.output || r.message || "-") + '</td></tr>';
        });

        html += '</tbody></table>';
        container.innerHTML = html;
    }



    // -----------------------------------------------------------------------
    // Dialog
    // -----------------------------------------------------------------------
    function showDialog(title, message, buttons) {
        showDialogRaw(title, '<p>' + esc(message) + '</p>', buttons);
    }

    // Dialog stack: supports nested dialogs (e.g. diff detail over diff list).
    // When a new dialog is shown while one is already visible, the current
    // dialog's render function is pushed to the stack. When the new dialog is
    // closed, the previous dialog is restored from the stack. This lets users
    // open a detail view, close it, and seamlessly return to the previous
    // dialog (e.g. the diff list) without losing their work.
    var _dialogStack = [];
    var _currentDialogRender = null;

    function showDialogRaw(title, bodyHtml, buttons, dialogClass, renderExtra) {
        var overlay = document.getElementById("dialogOverlay");
        var dialog = overlay.querySelector(".dialog");

        // If a dialog is already visible, save its render function to the stack
        if (overlay.classList.contains("dialog-overlay--visible")) {
            _dialogStack.push(_currentDialogRender);
        }

        _currentDialogRender = function () {
            document.getElementById("dialogTitle").textContent = title;
            document.getElementById("dialogBody").innerHTML = bodyHtml;

            // Toggle dialog width class
            dialog.classList.remove("dialog--wide");
            if (dialogClass) dialog.classList.add(dialogClass);

            var footer = document.getElementById("dialogFooter");
            footer.innerHTML = "";
            buttons.forEach(function (b) {
                var btn = document.createElement("button");
                btn.className = "btn " + (b.cls || "");
                if (b.id) btn.id = b.id;
                btn.textContent = b.text;
                btn.addEventListener("click", function () {
                    if (b.action) {
                        var result = b.action();
                        if (result === false) return; // prevent close
                    }
                    _closeTopDialog();
                });
                footer.appendChild(btn);
            });

            // Allow the caller to re-attach event listeners / set up DOM
            // references that depend on the just-rendered body. This is
            // invoked both on the initial show AND when the dialog is
            // restored from the stack after a child dialog closes.
            if (renderExtra) renderExtra();
        };

        _currentDialogRender();
        overlay.classList.add("dialog-overlay--visible");
    }

    // Close the topmost dialog. If there's a stacked dialog underneath,
    // re-render it; otherwise hide the overlay.
    function _closeTopDialog() {
        var overlay = document.getElementById("dialogOverlay");
        if (_dialogStack.length > 0) {
            _currentDialogRender = _dialogStack.pop();
            _currentDialogRender();
        } else {
            _currentDialogRender = null;
            overlay.classList.remove("dialog-overlay--visible");
        }
    }

    // Change-type label map (shared by showDiffDialog & showDiffDetailDialog)
    var changeTypeLabels = {
        "added":    t("recovery.badgeAdded")    || "New",
        "modified": t("recovery.badgeModified") || "Changed",
        "removed":  t("recovery.badgeRemoved")  || "Removed"
    };

    // -----------------------------------------------------------------------
    // Diff detail dialog — 只读对比详情（点击 diff 行的查看按钮触发）
    // -----------------------------------------------------------------------
    function showDiffDetailDialog(name, currentValue, snapshotValue, changeType, isSystem) {
        console.log("[diffDebug] showDiffDetailDialog called:", name, changeType, "system:", isSystem);
        console.log("[diffDebug] currentValue length:", (currentValue || "").length);
        console.log("[diffDebug] snapshotValue length:", (snapshotValue || "").length);

        var title = t("recovery.diffDetailTitle", name);
        console.log("[diffDebug] title:", title);
        var scopeLabel = isSystem ? t("settings.viewScopeSystem") : t("settings.viewScopeUser");
        var badgeCls = "diff-badge diff-badge--" + changeType;
        var badgeText = changeTypeLabels[changeType] || changeType;
        var isPath = name.toUpperCase() === "PATH";

        var bodyHtml = '<div class="dialog-form">';

        // Meta row: scope + type
        bodyHtml += '<div class="diff-detail-meta">';
        bodyHtml += '<div><span class="dialog-form__label">' + esc(t("settings.viewScope")) + '</span> ';
        bodyHtml += '<span class="diff-detail-scope">' + esc(scopeLabel) + '</span></div>';
        bodyHtml += '<div><span class="dialog-form__label">' + esc(t("settings.type")) + '</span> ';
        bodyHtml += '<span class="' + badgeCls + '">' + esc(badgeText) + '</span></div>';
        bodyHtml += '</div>';

        if (isPath && changeType === "modified") {
            // PATH-style diff: per-entry added/removed
            var oldEntries = (currentValue || "").split(";").map(function (s) { return s.trim(); }).filter(Boolean);
            var newEntries = (snapshotValue || "").split(";").map(function (s) { return s.trim(); }).filter(Boolean);
            var oldSet = {}; oldEntries.forEach(function (e) { oldSet[e] = (oldSet[e] || 0) + 1; });
            var newSet = {}; newEntries.forEach(function (e) { newSet[e] = (newSet[e] || 0) + 1; });
            var added = []; var removed = []; var unchanged = [];
            oldEntries.forEach(function (e) {
                if (newSet[e] > 0) { newSet[e]--; unchanged.push(e); }
                else { removed.push(e); }
            });
            newEntries.forEach(function (e) {
                if (newSet[e] > 0) { added.push(e); newSet[e]--; }
            });

            var totalLines = oldEntries.length + added.length;
            bodyHtml += '<div class="diff-detail-summary">';
            bodyHtml += esc(t("recovery.diffLineCount", totalLines, added.length, removed.length));
            bodyHtml += '</div>';

            bodyHtml += '<div class="diff-detail-paths">';
            added.forEach(function (e) {
                bodyHtml += '<div class="diff-detail-path diff-detail-path--added">'
                    + '<span class="diff-detail-path__marker">+</span>'
                    + '<span class="diff-detail-path__text">' + esc(e) + '</span>'
                    + '<span class="diff-detail-path__label">' + esc(t("recovery.diffPathAdded")) + '</span>'
                    + '</div>';
            });
            removed.forEach(function (e) {
                bodyHtml += '<div class="diff-detail-path diff-detail-path--removed">'
                    + '<span class="diff-detail-path__marker">-</span>'
                    + '<span class="diff-detail-path__text">' + esc(e) + '</span>'
                    + '<span class="diff-detail-path__label">' + esc(t("recovery.diffPathRemoved")) + '</span>'
                    + '</div>';
            });
            unchanged.forEach(function (e) {
                bodyHtml += '<div class="diff-detail-path diff-detail-path--unchanged">'
                    + '<span class="diff-detail-path__marker">·</span>'
                    + '<span class="diff-detail-path__text">' + esc(e) + '</span>'
                    + '<span class="diff-detail-path__label">' + esc(t("recovery.diffPathUnchanged")) + '</span>'
                    + '</div>';
            });
            bodyHtml += '</div>';
        } else if (changeType === "added") {
            bodyHtml += '<div class="diff-detail-side">';
            bodyHtml += '<div class="diff-detail-side__label">' + esc(t("recovery.diffSnapshotLabel")) + '</div>';
            bodyHtml += '<textarea class="input input--mono input--readonly diff-detail-textarea" rows="8" readonly>' + esc(snapshotValue || "") + '</textarea>';
            bodyHtml += '</div>';
        } else if (changeType === "removed") {
            bodyHtml += '<div class="diff-detail-side">';
            bodyHtml += '<div class="diff-detail-side__label">' + esc(t("recovery.diffCurrentLabel")) + '</div>';
            bodyHtml += '<textarea class="input input--mono input--readonly diff-detail-textarea" rows="8" readonly>' + esc(currentValue || "") + '</textarea>';
            bodyHtml += '</div>';
        } else {
            // modified (non-PATH): side-by-side
            bodyHtml += '<div class="diff-detail-grid">';
            bodyHtml += '<div class="diff-detail-side">';
            bodyHtml += '<div class="diff-detail-side__label">' + esc(t("recovery.diffCurrentLabel")) + '</div>';
            bodyHtml += '<textarea class="input input--mono input--readonly diff-detail-textarea" rows="10" readonly>' + esc(currentValue || "") + '</textarea>';
            bodyHtml += '</div>';
            bodyHtml += '<div class="diff-detail-side">';
            bodyHtml += '<div class="diff-detail-side__label">' + esc(t("recovery.diffSnapshotLabel")) + '</div>';
            bodyHtml += '<textarea class="input input--mono input--readonly diff-detail-textarea" rows="10" readonly>' + esc(snapshotValue || "") + '</textarea>';
            bodyHtml += '</div>';
            bodyHtml += '</div>';
        }

        bodyHtml += '</div>';

        showDialogRaw(title, bodyHtml, [
            { text: t("settings.viewClose") || "Close", cls: "btn--accent" }
        ]);
    }

    // -----------------------------------------------------------------------
    // Diff dialog — 增量恢复前的差异对比选择
    // -----------------------------------------------------------------------
    function showDiffDialog(snapshotId, diffs) {
        var hasDiffs = !!(diffs && diffs.length > 0);

        // Group diffs by scope
        var userDiffs = diffs.filter(function (d) { return !d.system; });
        var sysDiffs  = diffs.filter(function (d) { return  d.system; });

        var html = '';
        html += '<div class="diff-dialog">';

        if (hasDiffs) {
            html += '<p class="diff-hint">' + (t("recovery.diffHint") || "Select variables to restore from the snapshot. Unchanged variables are not listed.") + '</p>';
        } else {
            html += '<div class="diff-empty">' +
                    '<div class="diff-empty-icon">&#x2705;</div>' +
                    '<div class="diff-empty-title">' + (t("recovery.diffEmptyTitle") || "Restore Snapshot") + '</div>' +
                    '<div class="diff-empty-body">' + (t("recovery.diffEmptyBody") || "Current environment is already identical to the snapshot.") + '</div>' +
                    '</div>';
        }

        function renderScope(diffList, label) {
            if (diffList.length === 0) return '';
            var out = '';
            out += '<div class="diff-scope">';
            out += '<div class="diff-scope-title">' + esc(label) + ' <span class="diff-scope-count">' + diffList.length + '</span></div>';
            diffList.forEach(function (d) {
                var badgeCls = "diff-badge diff-badge--" + d.changeType;
                var badgeText = changeTypeLabels[d.changeType] || d.changeType;
                // Single-line layout: [checkbox] key  old→new  [badge]  [view-btn]
                out += '<label class="diff-item" data-dbg-name="' + esc(d.name) + '">';
                out += '<input type="checkbox" class="diff-check" checked value="' + esc(d.name) + '|' + (d.system ? '1' : '0') + '">';
                out += '<span class="diff-name">' + esc(d.name) + '</span>';
                out += '<span class="diff-values">';
                if (d.changeType === 'removed') {
                    out += '<span class="diff-old diff-old--full">' + esc(d.currentValue || '(empty)') + '</span>';
                    out += '<span class="diff-arrow">→</span>';
                    out += '<span class="diff-new diff-new--removed">' + (t("recovery.willBeRemoved") || "WILL BE REMOVED") + '</span>';
                } else {
                    if (d.changeType === 'modified') {
                        out += '<span class="diff-old">' + esc(d.currentValue || '(empty)') + '</span>';
                        out += '<span class="diff-arrow">→</span>';
                    } else {
                        // added: show arrow before the new value
                        out += '<span class="diff-arrow">→</span>';
                    }
                    out += '<span class="diff-new">' + esc(d.snapshotValue || '') + '</span>';
                }
                out += '</span>';
                out += '<span class="' + badgeCls + '">' + esc(badgeText) + '</span>';
                out += '<button type="button" class="diff-detail-btn" title="' + esc(t("recovery.viewDiffBtn")) + '" aria-label="' + esc(t("recovery.viewDiffBtn")) + '">'
                    + '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"><path d="M1.5 8s2.5-5 6.5-5 6.5 5 6.5 5-2.5 5-6.5 5-6.5-5-6.5-5z"/><circle cx="8" cy="8" r="2.2"/></svg>'
                    + '</button>';
                out += '</label>';
            });
            out += '</div>';
            return out;
        }

        html += renderScope(userDiffs,  t("settings.scopeUser")  || "User Variables");
        html += renderScope(sysDiffs,   t("settings.scopeSystem")|| "System Variables");

        html += '</div>';

        // Toggle state — tracked in closure for button sync
        var allSelected = true;
        // Button DOM refs will be stored here for text update
        var selectAllBtn = null;
        var selectAllTextSel  = t("recovery.diffSelectAll")   || "Select All";
        var selectAllTextDesel = t("recovery.diffDeselectAll") || "Deselect All";

        function syncSelectAllBtn() {
            if (selectAllBtn) {
                selectAllBtn.textContent = allSelected ? selectAllTextDesel : selectAllTextSel;
            }
        }

        var buttons = [];

        buttons.push({
            text: t("recovery.btnRestoreFull") || "Full Restore",
            cls: "btn--danger btn--left",
            id: "btnFullRestore",
            action: function () {
                showDialogRaw(
                    t("recovery.confirmRestoreTitle") || "Confirm Full Restore",
                    '<div class="dialog-warning">' +
                    '<svg class="dialog-warning__icon" viewBox="0 0 20 20" fill="currentColor"><path fill-rule="evenodd" d="M8.257 3.099c.765-1.36 2.722-1.36 3.486 0l5.58 9.92c.75 1.334-.213 2.98-1.742 2.98H4.42c-1.53 0-2.493-1.646-1.743-2.98l5.58-9.92zM11 13a1 1 0 11-2 0 1 1 0 012 0zm-1-8a1 1 0 00-1 1v3a1 1 0 002 0V6a1 1 0 00-1-1z" clip-rule="evenodd"/></svg>' +
                    '<div class="dialog-warning__text">' + esc(t("recovery.fullRestoreHint") || "Full Restore will replace ALL current variables with the snapshot values. This cannot be undone.") + '</div>' +
                    '</div>',
                    [
                        // Cancel: just close. The dialog stack will automatically
                        // restore the parent diff dialog that opened this confirm.
                        { text: t("dialog.cancel"), cls: "" },
                        { text: t("recovery.btnRestoreFull") || "Full Restore", cls: "btn--danger", action: function () {
                            sendNative({ action: "restoreSnapshot", snapshotId: snapshotId, mode: "full" });
                            // Clear the entire dialog stack so the user returns
                            // to the recovery page rather than the diff dialog
                            // after the restore completes.
                            _dialogStack.length = 0;
                        } }
                    ]
                );
                return false;
            }
        });

        if (hasDiffs) {
            buttons.push({
                text: allSelected ? selectAllTextDesel : selectAllTextSel,
                cls: "btn--secondary",
                id: "btnToggleAll",
                action: function () {
                    var checks = document.querySelectorAll(".diff-check");
                    allSelected = !allSelected;
                    checks.forEach(function (c) { c.checked = allSelected; });
                    syncSelectAllBtn();
                    return false;
                }
            });
        }

        if (hasDiffs) {
            buttons.push({
                text: t("recovery.diffRestoreSelected") || "Restore Selected",
                cls: "btn--accent",
                action: function () {
                    var checks = document.querySelectorAll(".diff-check:checked");
                    var names = [];
                    checks.forEach(function (c) {
                        var parts = c.value.split("|");
                        var prefix = (parts[1] === "1") ? "system:" : "user:";
                        names.push(prefix + parts[0]);
                    });
                    if (names.length === 0) {
                        showToast(t("recovery.diffNoneSelected") || "Please select at least one variable.", "warn");
                        return false;
                    }
                    sendNative({
                        action: "restoreSnapshot",
                        snapshotId: snapshotId,
                        mode: "incremental",
                        names: names
                    });
                    // Clear the dialog stack so the user returns to the
                    // recovery page rather than the diff dialog after the
                    // incremental restore completes.
                    _dialogStack.length = 0;
                }
            });
        }

        buttons.push({ text: t("dialog.cancel"), cls: "" });

        var dialogTitleText = hasDiffs ? (t("recovery.diffTitle") || "Changed Variables")
                                       : (t("recovery.diffEmptyTitle") || "Restore Snapshot");
        // The renderExtra callback is invoked both on the initial show AND when
        // this dialog is restored from the stack after a child dialog (e.g. the
        // per-variable detail view) closes. Re-binding event listeners inside the
        // callback ensures they survive a restore.
        showDialogRaw(dialogTitleText, html, buttons, "dialog--wide", function () {
            // Inject warning hint into dialog header (same line as title)
            var titleEl = document.getElementById("dialogTitle");
            if (titleEl) {
                var hintText = t("recovery.fullRestoreHint") || "Full Restore will replace ALL current variables with the snapshot values. This cannot be undone.";
                titleEl.innerHTML = esc(dialogTitleText) + '<span class="dialog-title-hint" title="' + esc(hintText) + '">' + esc(hintText) + '</span>';
            }

            // After DOM is built, grab the toggle button ref so we can update its text later
            selectAllBtn = document.getElementById("btnToggleAll");

            // Sync toggle button label when user manually checks/unchecks individual items
            if (hasDiffs) {
                var diffChecks = document.querySelectorAll(".diff-check");
                diffChecks.forEach(function (chk) {
                    chk.addEventListener("change", function () {
                        var total = document.querySelectorAll(".diff-check").length;
                        var checked = document.querySelectorAll(".diff-check:checked").length;
                        allSelected = (checked === total);
                        syncSelectAllBtn();
                    });
                });

                // View-detail button — use mousedown to fire BEFORE the label grabs the click
                var detailBtns = document.querySelectorAll(".diff-detail-btn");
                detailBtns.forEach(function (btn) {
                    btn.addEventListener("mousedown", function (e) {
                        e.preventDefault();
                        e.stopPropagation();

                        var parentRow = btn.closest(".diff-item");
                        if (!parentRow) return;

                        var name = parentRow.querySelector(".diff-name").textContent;
                        var item = diffs.find(function (d) { return d.name === name; });
                        if (!item) return;

                        showDiffDetailDialog(item.name, item.currentValue, item.snapshotValue, item.changeType, !!item.system);
                    });
                });
            }
        });
    }

    document.getElementById("dialogOverlay").addEventListener("click", function (e) {
        // Click on the dimmed backdrop (not the dialog itself) closes the
        // topmost dialog. If a child dialog is open, this restores the
        // parent dialog from the stack rather than hiding the overlay.
        if (e.target === this) _closeTopDialog();
    });

    // -----------------------------------------------------------------------
    // Toast notification
    // -----------------------------------------------------------------------
    function showToast(msg, type) {
        var container = document.getElementById("toastContainer");
        var el = document.createElement("div");
        el.className = "toast" + (type ? " toast--" + type : "");
        el.textContent = msg;
        container.appendChild(el);
        requestAnimationFrame(function () { el.classList.add("toast--visible"); });
        setTimeout(function () { el.classList.remove("toast--visible"); }, 3000);
        setTimeout(function () { el.remove(); }, 3500);
    }

    // -----------------------------------------------------------------------
    // Utility
    // -----------------------------------------------------------------------
    function esc(s) {
        if (!s) return "";
        var d = document.createElement("div");
        d.textContent = s;
        return d.innerHTML;
    }

    // -----------------------------------------------------------------------
    // Cmd tooltip + double-click copy
    // -----------------------------------------------------------------------

    var cmdTooltip = (function () {
        var el = document.createElement("div");
        el.className = "cmd-tooltip";
        el.setAttribute("aria-hidden", "true");
        document.body.appendChild(el);

        var currentTarget = null;
        var hideTimer = 0;

        function show(e) {
            var target = e.target.closest(".cmd-text, .install-item__cmd, .summary-detail");
            if (!target) return;
            var text = target.textContent;
            if (!text || text === "-") return;
            currentTarget = target;
            clearTimeout(hideTimer);
            el.textContent = text;
            el.classList.toggle("cmd-tooltip--detail", target.classList.contains("summary-detail"));
            el.classList.add("is-visible");
            position(e);
        }

        function position(e) {
            var pad = 12;
            var x = e.clientX + pad;
            var y = e.clientY - pad;
            var tw = el.offsetWidth;
            var th = el.offsetHeight;
            if (x + tw > window.innerWidth - pad)  x = e.clientX - tw - pad;
            if (y + th > window.innerHeight - pad) y = e.clientY - th - pad;
            if (x < pad) x = pad;
            if (y < pad) y = pad;
            el.style.left = x + "px";
            el.style.top  = y + "px";
        }

        function move(e) {
            if (!currentTarget) return;
            position(e);
        }

        function hide() {
            currentTarget = null;
            hideTimer = setTimeout(function () {
                el.classList.remove("is-visible");
            }, 80);
        }

        document.addEventListener("mouseover", function (e) { show(e); });
        document.addEventListener("mousemove", function (e) { move(e); });
        document.addEventListener("mouseout", function (e) {
            if (currentTarget && !currentTarget.contains(e.relatedTarget)) hide();
        });
    })();

    // Double-click command text → auto-copy to clipboard
    document.addEventListener("dblclick", function (e) {
        var el = e.target.closest(".cmd-text, .install-item__cmd");
        if (!el) return;
        var text = el.textContent;
        if (!text || text === "-") return;
        // Select all text so user can also Ctrl+C manually
        var sel = window.getSelection();
        var range = document.createRange();
        range.selectNodeContents(el);
        sel.removeAllRanges();
        sel.addRange(range);
        // Copy to clipboard
        try {
            if (navigator.clipboard && navigator.clipboard.writeText) {
                navigator.clipboard.writeText(text).then(function () {
                    showToast(t("summary.copied") || "Copied");
                }).catch(function () {});
            }
        } catch (_) {}
    });

    // -----------------------------------------------------------------------
    // Init
    // -----------------------------------------------------------------------
    loadPersistedState();
    loadInstallLocation();
    sendNative({ action: "adminCheck" });
    navigateTo("home");

    // Tooltip helper for install location
    function updateLocationTooltip(val) {
        var tooltip = document.getElementById("installLocationTooltip");
        var input = document.getElementById("txtInstallLocation");
        if (!tooltip) return;
        if (val && /\s/.test(val)) {
            tooltip.textContent = t("packages.noSpacesInPath");
            tooltip.style.display = "block";
            if (input) input.classList.add("titlebar__install-input--error");
        } else {
            tooltip.style.display = "none";
            tooltip.textContent = "";
            if (input) input.classList.remove("titlebar__install-input--error");
        }
    }

    // Install location input persistence (titlebar)
    var _txtInstallLoc = document.getElementById("txtInstallLocation");
    if (_txtInstallLoc) {
        _txtInstallLoc.addEventListener("input", function () {
            var val = this.value;
            saveInstallLocation(val);
            updateLocationTooltip(val);
        });
    }

    // Browse folder button
    var _btnBrowse = document.getElementById("btnBrowseFolder");
    if (_btnBrowse) {
        _btnBrowse.addEventListener("click", function () {
            sendNative({ action: "selectFolder" });
        });
    }

})();

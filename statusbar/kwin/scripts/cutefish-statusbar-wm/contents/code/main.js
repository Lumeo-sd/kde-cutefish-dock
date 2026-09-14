/*
 * KWin script for the Cutefish status bar (Wayland).
 *
 * The org_kde_plasma_window_management protocol is restricted to Plasma
 * session clients ("only one client can bind this interface at a time",
 * "regular clients must not use this protocol"), so a normal LayerShell
 * panel can never receive the active window or the application menu of the
 * focused window directly. This script runs inside KWin instead (which has
 * full access to the workspace) and pushes the window state to the status
 * bar over DBus.
 *
 * Keep this script lean and defensive: every exotic access is guarded, and
 * a periodic refresh covers cases where the app menu is registered shortly
 * after the window becomes active.
 */

var SERVICE = "com.cutefish.Statusbar";
var PATH = "/WindowManager";
var IFACE = "com.cutefish.Statusbar.WindowManager";
var REFRESH_MS = 1500;

function print(line) {
    console.info("cutefish-statusbar-wm: " + line);
}

/**
 * Publish the current active window state to the status bar.
 */
function push(window) {
    var caption = "";
    var appId = "";
    var skipTaskbar = true;
    var menuService = "";
    var menuPath = "";

    if (window) {
        try { caption = window.caption || ""; } catch (e) {}
        try { appId = window.resourceClass || ""; } catch (e) {}
        try { skipTaskbar = window.skipTaskbar !== undefined ? window.skipTaskbar : true; } catch (e) {}
        // Application menu (org_kde_kwin_appmenu based) — exposed by KWin on
        // the window object. Guard every access: on older releases these
        // properties are missing entirely.
        try { menuService = window.applicationMenuServiceName || ""; } catch (e) {}
        try { menuPath = window.applicationMenuObjectPath || ""; } catch (e) {}
    }

    try {
        callDBus(SERVICE, PATH, IFACE, "setActiveWindow",
                 caption, appId, skipTaskbar, menuService, menuPath);
    } catch (e) {
        print("push() callDBus failed: " + e);
    }
}

function refresh() {
    push(workspace.activeWindow);
}

function main() {
    workspace.activeWindowChanged.connect(refresh);
    workspace.windowAdded.connect(refresh);
    workspace.windowActivated.connect(refresh);

    // The app menu can also be (re)registered while a window keeps focus.
    try {
        workspace.windowList().forEach(function (win) {
            if (win === workspace.activeWindow) {
                return;
            }
            // no-op: we refresh on a timer anyway
        });
    } catch (e) {
        print("windowList iteration failed: " + e);
    }

    // Periodic refresh: reliable, catches late app-menu registration and any
    // property change signal we do not know about on this KWin version.
    var timer = null;
    try {
        timer = Qt.createQmlObject(
            "import QtQuick 2.0; Timer { id: t; interval: " + REFRESH_MS + "; repeat: true; running: true; }",
            workspace, "cutefishStatusbarRefresh");
        timer.triggered.connect(refresh);
    } catch (e) {
        print("timer creation failed: " + e);
    }

    // Publish the initial state.
    refresh();

    print("initialized");
}

main();
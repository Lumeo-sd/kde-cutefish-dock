/*
 * Copyright (C) 2021 CutefishOS Team.
 *
 * Author:     cutefishos <cutefishos@foxmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef WINDOWMANAGER_H
#define WINDOWMANAGER_H

#include <QObject>
#include <QIcon>

namespace KWayland
{
namespace Client
{
class ConnectionThread;
class Registry;
class PlasmaWindowManagement;
class PlasmaWindow;
}
}

/**
 * Window tracking for the status bar.
 *
 * The original implementation polled the X11 root window through KX11Extras /
 * NETWM (active window, close/minimize/maximize/move requests, the
 * _KDE_NET_WM_APPMENU_* application menu properties).
 *
 * On a KWin Wayland session the compositor restricts
 * org_kde_plasma_window_management to Plasma session clients ("only one
 * client can bind this interface at a time"), so a plain LayerShell panel
 * never receives the active window or the application menu directly. This
 * class therefore has two interchangeable backends:
 *
 *  - KWayland PlasmaWindowManagement, used whenever the compositor offers the
 *    global (untested on KWin 6, kept as fallback);
 *  - a DBus channel fed by the "cutefish-statusbar-wm" KWin script, which has
 *    full compositor access and pushes the active window state (title, app
 *    id, skip-taskbar, application menu service/object path) to this object
 *    at /WindowManager on the session bus.
 */
class WindowManager : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.cutefish.Statusbar.WindowManager")

public:
    static WindowManager *instance();
    explicit WindowManager(QObject *parent = nullptr);

    bool isReady() const;

    // State of the active window (either backend).
    QString currentTitle() const;
    QString currentAppId() const;
    bool currentSkipTaskbar() const;

    // Actions on the active window, equivalent to the old KX11Extras calls.
    void closeActiveWindow();
    void toggleMinimizeActiveWindow();
    void toggleMaximizeActiveWindow();
    void moveActiveWindow();

    // Application menu (org_kde_kwin_appmenu), replaces the X11
    // _KDE_NET_WM_APPMENU_SERVICE_NAME / _OBJECT_PATH properties.
    QString activeAppMenuServiceName() const;
    QString activeAppMenuObjectPath() const;

    bool activeWindowAcceptable() const;

public Q_SLOTS:
    /**
     * Called by the "cutefish-statusbar-wm" KWin script (via DBus).
     * The KWayland backend is preferred whenever it is available; this slot
     * only feeds the state when no PlasmaWindowManagement global is offered
     * by the compositor.
     */
    void setActiveWindow(const QString &title,
                         const QString &appId,
                         bool skipTaskbar,
                         const QString &appMenuService,
                         const QString &appMenuObjectPath);

signals:
    void activeWindowChanged();
    void windowStateChanged();

private:
    void setupWaylandConnection();
    void onActiveWindowChanged();
    void storeState(const QString &title,
                    const QString &appId,
                    bool skipTaskbar,
                    const QString &appMenuService,
                    const QString &appMenuObjectPath);
    void emitStateChanged();

    KWayland::Client::ConnectionThread *m_connection = nullptr;
    KWayland::Client::Registry *m_registry = nullptr;
    KWayland::Client::PlasmaWindowManagement *m_management = nullptr;
    KWayland::Client::PlasmaWindow *m_activeWindow = nullptr;

    // DBus-fed state (KWin script backend).
    QString m_title;
    QString m_appId;
    bool m_skipTaskbar = true;
    QString m_appMenuService;
    QString m_appMenuObjectPath;
};

#endif // WINDOWMANAGER_H
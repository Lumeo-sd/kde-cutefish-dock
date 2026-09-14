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

#include "windowmanager.h"

#include <QDebug>
#include <QDBusConnection>

#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/registry.h>
#include <KWayland/Client/plasmawindowmanagement.h>

using namespace KWayland::Client;

static WindowManager *INSTANCE = nullptr;

WindowManager *WindowManager::instance()
{
    if (!INSTANCE)
        INSTANCE = new WindowManager;

    return INSTANCE;
}

WindowManager::WindowManager(QObject *parent)
    : QObject(parent)
{
    // Expose the DBus object the KWin script pushes the active window state
    // into. The bus name itself ("com.cutefish.Statusbar") is registered by
    // the main application; this object is an additional path on it.
    QDBusConnection::sessionBus().registerObject(QStringLiteral("/WindowManager"),
                                                 this,
                                                 QDBusConnection::ExportAllSlots);

    setupWaylandConnection();
}

bool WindowManager::isReady() const
{
    return m_management != nullptr;
}

QString WindowManager::currentTitle() const
{
    return m_title;
}

QString WindowManager::currentAppId() const
{
    return m_appId;
}

bool WindowManager::currentSkipTaskbar() const
{
    return m_skipTaskbar;
}

void WindowManager::setupWaylandConnection()
{
    // Reuse the QtWayland connection instead of opening a second socket to the
    // compositor: fromApplication() returns nullptr off Wayland, which is fine
    // for this port (the status bar is a Wayland panel only).
    m_connection = ConnectionThread::fromApplication(this);
    if (!m_connection) {
        qWarning() << "WindowManager: not running on Wayland, relying on the KWin script DBus channel.";
        return;
    }

    m_registry = new Registry(this);
    m_registry->create(m_connection);
    // Finalizes the registry setup: without this the global announcements
    // (plasma window management) are never requested from the compositor. The
    // events flow over QtWayland's own connection, so no dedicated EventQueue
    // is attached here (QtWayland only dispatches the default queue).
    m_registry->setup();

    connect(m_registry, &Registry::interfaceAnnounced, this,
            [this](QByteArray interface, quint32 name, quint32 version) {
                if (interface != QByteArrayLiteral("org_kde_plasma_window_management"))
                    return;

                // KWin 6 only advertises this global to clients whose desktop
                // file lists it in X-KDE-Wayland-Interfaces (plasmashell does,
                // the status bar does via ~/.local/share/applications/
                // cutefish-statusbar.desktop).
                m_management = m_registry->createPlasmaWindowManagement(name, version, this);

                connect(m_management, &PlasmaWindowManagement::activeWindowChanged,
                        this, &WindowManager::onActiveWindowChanged);

                qInfo() << "WindowManager: plasma window management bound (version" << version << ")";

                onActiveWindowChanged();
            });
}

void WindowManager::onActiveWindowChanged()
{
    PlasmaWindow *window = m_management ? m_management->activeWindow() : nullptr;

    if (m_activeWindow != nullptr) {
        disconnect(m_activeWindow, nullptr, this, nullptr);
    }

    m_activeWindow = window;

    if (m_activeWindow) {
        // Reflect property changes of the active window (title, icon, appId,
        // taskbar visibility) without requiring an active window switch.
        connect(m_activeWindow, &PlasmaWindow::titleChanged, this, &WindowManager::windowStateChanged);
        connect(m_activeWindow, &PlasmaWindow::appIdChanged, this, &WindowManager::windowStateChanged);
        connect(m_activeWindow, &PlasmaWindow::iconChanged, this, &WindowManager::windowStateChanged);
        connect(m_activeWindow, &PlasmaWindow::skipTaskbarChanged, this, &WindowManager::windowStateChanged);
        // The application menu (org_kde_kwin_appmenu) can be registered at any
        // time, not only when the window becomes active.
        connect(m_activeWindow, &PlasmaWindow::applicationMenuChanged, this, &WindowManager::windowStateChanged);
    }

    // Mirror the KWayland state into the shared state fields so that all
    // consumers read from one place.
    storeState(m_activeWindow ? m_activeWindow->title() : QString(),
               m_activeWindow ? m_activeWindow->appId() : QString(),
               m_activeWindow ? m_activeWindow->skipTaskbar() : true,
               m_activeWindow ? m_activeWindow->applicationMenuServiceName() : QString(),
               m_activeWindow ? m_activeWindow->applicationMenuObjectPath() : QString());

    emitStateChanged();
}

void WindowManager::setActiveWindow(const QString &title,
                                    const QString &appId,
                                    bool skipTaskbar,
                                    const QString &appMenuService,
                                    const QString &appMenuObjectPath)
{
    // The KWayland backend wins whenever the compositor offers it: it is
    // richer (window objects for actions) and already mirrors into storeState.
    if (m_management && m_activeWindow)
        return;

    qInfo() << "WindowManager: setActiveWindow from KWin script:"
            << title << appId << skipTaskbar << appMenuService << appMenuObjectPath;

    storeState(title, appId, skipTaskbar, appMenuService, appMenuObjectPath);
    emitStateChanged();
}

void WindowManager::storeState(const QString &title,
                               const QString &appId,
                               bool skipTaskbar,
                               const QString &appMenuService,
                               const QString &appMenuObjectPath)
{
    const bool changed = (m_title != title || m_appId != appId
                          || m_skipTaskbar != skipTaskbar
                          || m_appMenuService != appMenuService
                          || m_appMenuObjectPath != appMenuObjectPath);

    m_title = title;
    m_appId = appId;
    m_skipTaskbar = skipTaskbar;
    m_appMenuService = appMenuService;
    m_appMenuObjectPath = appMenuObjectPath;

    // Note: activeWindowChanged() is emitted unconditionally on every push:
    // the panel also needs to react when a *different* window became active.
    if (changed) {
        emit windowStateChanged();
        emit activeWindowChanged();
    }
}

void WindowManager::emitStateChanged()
{
    emit windowStateChanged();
    emit activeWindowChanged();
}

void WindowManager::closeActiveWindow()
{
    if (m_activeWindow)
        m_activeWindow->requestClose();
    else
        qWarning() << "WindowManager: closeActiveWindow() without a tracked window (no KWayland backend)";
}

void WindowManager::toggleMinimizeActiveWindow()
{
    if (m_activeWindow)
        m_activeWindow->requestToggleMinimized();
    else
        qWarning() << "WindowManager: toggleMinimizeActiveWindow() without a tracked window (no KWayland backend)";
}

void WindowManager::toggleMaximizeActiveWindow()
{
    if (m_activeWindow)
        m_activeWindow->requestToggleMaximized();
    else
        qWarning() << "WindowManager: toggleMaximizeActiveWindow() without a tracked window (no KWayland backend)";
}

void WindowManager::moveActiveWindow()
{
    if (m_activeWindow)
        m_activeWindow->requestMove();
    else
        qWarning() << "WindowManager: moveActiveWindow() without a tracked window (no KWayland backend)";
}

QString WindowManager::activeAppMenuServiceName() const
{
    if (m_activeWindow)
        return m_activeWindow->applicationMenuServiceName();
    return m_appMenuService;
}

QString WindowManager::activeAppMenuObjectPath() const
{
    if (m_activeWindow)
        return m_activeWindow->applicationMenuObjectPath();
    return m_appMenuObjectPath;
}

bool WindowManager::activeWindowAcceptable() const
{
    // The compositor tells us which windows are meant for the task bar; the
    // X11 version replicated the same list (desktops, docks, menus,
    // notifications and skip-taskbar windows).
    if (m_skipTaskbar)
        return false;

    // Without an appId there is no way to show a meaningful app label.
    if (m_appId.isEmpty())
        return false;

    return true;
}
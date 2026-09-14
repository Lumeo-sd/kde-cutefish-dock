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

#include "activity.h"
#include "windowmanager.h"

#include <QFile>
#include <QDirIterator>
#include <QSettings>
#include <QRegularExpression>

static const QStringList blockList = {"cutefish-launcher",
                                      "cutefish-statusbar"};

Activity::Activity(QObject *parent)
    : QObject(parent)
    , m_cApps(CApplications::self())
{
    WindowManager *wm = WindowManager::instance();

    connect(wm, &WindowManager::activeWindowChanged, this, &Activity::onActiveWindowChanged);
    connect(wm, &WindowManager::windowStateChanged, this, &Activity::onActiveWindowChanged);

    onActiveWindowChanged();
}

bool Activity::launchPad() const
{
    return m_launchPad;
}

QString Activity::title() const
{
    return m_title;
}

QString Activity::icon() const
{
    return m_icon;
}

void Activity::close()
{
    WindowManager::instance()->closeActiveWindow();
}

void Activity::minimize()
{
    WindowManager::instance()->toggleMinimizeActiveWindow();
}

void Activity::restore()
{
    // Wayland has no NET::Max state to clear: un-maximize instead.
    WindowManager::instance()->toggleMaximizeActiveWindow();
}

void Activity::maximize()
{
    WindowManager::instance()->toggleMaximizeActiveWindow();
}

void Activity::toggleMaximize()
{
    WindowManager::instance()->toggleMaximizeActiveWindow();
}

void Activity::move()
{
    WindowManager::instance()->moveActiveWindow();
}

bool Activity::isAcceptableWindow(quint64 wid)
{
    Q_UNUSED(wid);
    return WindowManager::instance()->activeWindowAcceptable();
}

void Activity::onActiveWindowChanged()
{
    WindowManager *wm = WindowManager::instance();

    QString appId = wm->currentAppId();
    m_launchPad = (appId == "cutefish-launcher");
    emit launchPadChanged();

    if (appId.isEmpty()) {
        clearTitle();
        clearIcon();
        return;
    }

    if (blockList.contains(appId)) {
        clearTitle();
        clearIcon();
        return;
    }

    CAppItem *item = nullptr;
    for (CAppItem *candidate : m_cApps->items()) {
        if (candidate->fileName == appId || candidate->startupWMClass == appId) {
            item = candidate;
            break;
        }
    }

    if (item) {
        m_title = item->localName;
        emit titleChanged();

        if (m_icon != item->icon) {
            m_icon = item->icon;
            emit iconChanged();
        }
    } else {
        // Generic windows (no desktop entry) fall back to the compositor title.
        QString title = wm->currentTitle();
        if (title != m_title) {
            m_title = title;
            emit titleChanged();
            clearIcon();
        }
    }
}

void Activity::clearTitle()
{
    m_title.clear();
    emit titleChanged();
}

void Activity::clearIcon()
{
    m_icon.clear();
    emit iconChanged();
}
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

#include "statusbar.h"
#include "battery.h"
#include "processprovider.h"
#include "appmenu/appmenu.h"
#include "statusbaradaptor.h"

#include <QQmlEngine>
#include <QQmlContext>

#include <QDBusConnection>
#include <QApplication>
#include <QSettings>
#include <QScreen>

#include <KWindowEffects>
#include <LayerShellQt/window.h>

StatusBar::StatusBar(QQuickView *parent)
    : QQuickView(parent)
    , m_acticity(new Activity)
{
    QSettings settings("cutefishos", "locale");
    m_twentyFourTime = settings.value("twentyFour", false).toBool();

    setFlags(Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);
    setColor(Qt::transparent);

    // Find the framework modules (Cutefish.*) and FishUI installed into the
    // private ~/.local prefix, like the dock and the launcher do.
    engine()->addImportPath(QCoreApplication::applicationDirPath() + QStringLiteral("/../lib64/qt6/qml"));
    engine()->addImportPath(QCoreApplication::applicationDirPath() + QStringLiteral("/../lib/qt6/qml"));

    new StatusbarAdaptor(this);
    new AppMenu(this);

    engine()->rootContext()->setContextProperty("StatusBar", this);
    engine()->rootContext()->setContextProperty("acticity", m_acticity);
    engine()->rootContext()->setContextProperty("process", new ProcessProvider);
    engine()->rootContext()->setContextProperty("battery", Battery::self());

    setSource(QUrl(QStringLiteral("qrc:/qml/main.qml")));
    setResizeMode(QQuickView::SizeRootObjectToView);
    setScreen(qApp->primaryScreen());

    m_layerShell = LayerShellQt::Window::get(this);
    updateLayerShell();
    updateGeometry();
    setVisible(true);

    connect(screen(), &QScreen::virtualGeometryChanged, this, &StatusBar::updateGeometry);
    connect(screen(), &QScreen::geometryChanged, this, &StatusBar::updateGeometry);

    // Always show on the main screen
    connect(qGuiApp, &QGuiApplication::primaryScreenChanged, this, &StatusBar::onPrimaryScreenChanged);
}

QRect StatusBar::screenRect()
{
    return m_screenRect;
}

bool StatusBar::twentyFourTime()
{
    return m_twentyFourTime;
}

void StatusBar::setBatteryPercentage(bool enabled)
{
    Battery::self()->setShowPercentage(enabled);
}

void StatusBar::setTwentyFourTime(bool t)
{
    if (m_twentyFourTime != t) {
        m_twentyFourTime = t;
        emit twentyFourTimeChanged();
    }
}

void StatusBar::updateLayerShell()
{
    if (!m_layerShell)
        return;

    // The bar is a full-width top panel that reserves the top screen edge.
    m_layerShell->setLayer(LayerShellQt::Window::LayerTop);

    LayerShellQt::Window::Anchors anchors = LayerShellQt::Window::AnchorNone;
    anchors |= LayerShellQt::Window::AnchorLeft;
    anchors |= LayerShellQt::Window::AnchorTop;
    anchors |= LayerShellQt::Window::AnchorRight;
    m_layerShell->setAnchors(anchors);
    m_layerShell->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
    m_layerShell->setExclusiveEdge(LayerShellQt::Window::AnchorTop);
}

void StatusBar::updateGeometry()
{
    const QRect rect = screen()->geometry();

    if (m_screenRect != rect) {
        m_screenRect = rect;
        emit screenRectChanged();
    }

    QRect windowRect = QRect(rect.x(), rect.y(), rect.width(), 25);
    setGeometry(windowRect);

    // The available area below the bar, exposed to the QML as screenRect, and
    // the exclusive zone let KWin place maximized windows below the panel.
    if (m_layerShell)
        m_layerShell->setExclusiveZone(windowRect.height());

    KWindowEffects::enableBlurBehind(this, true);
}

void StatusBar::onPrimaryScreenChanged(QScreen *screen)
{
    disconnect(this->screen());

    setScreen(screen);
    updateLayerShell();
    updateGeometry();

    connect(screen, &QScreen::virtualGeometryChanged, this, &StatusBar::updateGeometry);
    connect(screen, &QScreen::geometryChanged, this, &StatusBar::updateGeometry);
}

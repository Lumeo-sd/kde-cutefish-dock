/*
 * Copyright (C) 2021 CutefishOS Team.
 *
 * Author:     Reion Wong <reionwong@gmail.com>
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

#include "mainwindow.h"
#include "processprovider.h"
#include "xwindowinterface.h"
#include "dockadaptor.h"

#include <QGuiApplication>
#include <QScreen>
#include <QCoreApplication>
#include <QFile>
#include <QUrl>

#include <QQmlEngine>
#include <QQmlContext>
#include <QQmlProperty>
#include <QQuickItem>
#include <QMetaEnum>

#include <LayerShellQt/window.h>

MainWindow::MainWindow(QQuickView *parent)
    : QQuickView(parent)
    , m_activity(Activity::self())
    , m_settings(DockSettings::self())
    , m_appModel(new ApplicationModel)
    , m_trashManager(new TrashManager)
    , m_layerShell(nullptr)
    , m_hideBlocked(false)
    , m_dockHidden(false)
    , m_showTimer(new QTimer(this))
    , m_hideTimer(new QTimer(this))
    , m_shrinkTimer(new QTimer(this))
{
    new DockAdaptor(this);

    installEventFilter(this);

    // With the private ~/.local prefix the FishUI module lives outside Qt's
    // default import paths. Qt6 does not honor QT_QML_IMPORT_PATH at runtime,
    // so register the prefix path explicitly; for a system install this simply
    // resolves back to the default Qt QML directory.
    engine()->addImportPath(QCoreApplication::applicationDirPath() + QStringLiteral("/../lib64/qt6/qml"));
    engine()->addImportPath(QCoreApplication::applicationDirPath() + QStringLiteral("/../lib/qt6/qml"));

    // Wayland needs an alpha channel for the rounded translucent panel; the
    // original forced a matching X11 visual with setDefaultAlphaBuffer(false).
    QQuickWindow::setDefaultAlphaBuffer(true);
    setColor(Qt::transparent);

    setFlags(Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);

    // The dock is a floating layer-shell surface anchored to the screen edge.
    // The exclusive zone replaces the X11 NET::Dock type and its extended
    // struts: maximized windows stop right at the dock's edge and auto-hide
    // modes release the space by setting the zone to zero.
    XWindowInterface::instance()->setPanelWindow(this);
    m_layerShell = LayerShellQt::Window::get(this);
    // Never take keyboard focus. With KeyboardInteractivityOnDemand a click on
    // the panel makes the layer surface the *active* window in KWin, which
    // breaks the dock's "click the active app's icon to minimize it" logic
    // (activeWindow() would resolve to the panel, not the app). Pointer events
    // are unaffected by this setting.
    m_layerShell->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
    m_layerShell->setLayer(LayerShellQt::Window::LayerTop);

    engine()->rootContext()->setContextProperty("appModel", m_appModel);
    engine()->rootContext()->setContextProperty("process", new ProcessProvider);
    engine()->rootContext()->setContextProperty("Settings", m_settings);
    engine()->rootContext()->setContextProperty("mainWindow", this);
    engine()->rootContext()->setContextProperty("trash", m_trashManager);

    setSource(QUrl(QStringLiteral("qrc:/qml/main.qml")));
    setScreen(qApp->primaryScreen());
    setResizeMode(QQuickView::SizeRootObjectToView);
    initScreens();

    resizeWindow();
    onVisibilityChanged();

    m_showTimer->setSingleShot(true);
    m_showTimer->setInterval(200);
    connect(m_showTimer, &QTimer::timeout, this, [=] { setDockHidden(false); });

    m_hideTimer->setSingleShot(true);
    m_hideTimer->setInterval(500);
    connect(m_hideTimer, &QTimer::timeout, this, &MainWindow::onHideTimeout);

    // Slightly longer than the QML opacity Behaviour (200 ms) so the panel is
    // fully transparent by the time the surface shrinks to the edge strip.
    m_shrinkTimer->setSingleShot(true);
    m_shrinkTimer->setInterval(260);
    connect(m_shrinkTimer, &QTimer::timeout, this, &MainWindow::onShrinkTimeout);

    // When the current window changes.
    connect(m_activity, &Activity::launchPadChanged, this, &MainWindow::onVisibilityChanged);
    connect(m_activity, &Activity::existsWindowMaximizedChanged, this, &MainWindow::onVisibilityChanged);

    // Screen change.
    connect(qGuiApp, &QGuiApplication::primaryScreenChanged, this, &MainWindow::onPrimaryScreenChanged);
    connect(screen(), &QScreen::virtualGeometryChanged, this, &MainWindow::resizeWindow);
    connect(screen(), &QScreen::geometryChanged, this, &MainWindow::resizeWindow);

    connect(m_appModel, &ApplicationModel::countChanged, this, &MainWindow::resizeWindow);
    connect(m_settings, &DockSettings::directionChanged, this, &MainWindow::onPositionChanged);
    connect(m_settings, &DockSettings::iconSizeChanged, this, &MainWindow::onIconSizeChanged);
    connect(m_settings, &DockSettings::visibilityChanged, this, &MainWindow::onVisibilityChanged);
    connect(m_settings, &DockSettings::styleChanged, this, &MainWindow::resizeWindow);
}

MainWindow::~MainWindow()
{
}

void MainWindow::add(const QString &desktop)
{
    m_appModel->addItem(desktop);
}

bool MainWindow::addDesktopFile(const QString &desktop)
{
    const QString path = desktop.startsWith("file://")
                             ? QUrl(desktop).toLocalFile()
                             : desktop;

    if (!path.endsWith(".desktop", Qt::CaseInsensitive) || !QFile::exists(path))
        return false;

    m_appModel->addItem(path);
    return true;
}

void MainWindow::remove(const QString &desktop)
{
    m_appModel->removeItem(desktop);
}

bool MainWindow::pinned(const QString &desktop)
{
    return m_appModel->isDesktopPinned(desktop);
}

QRect MainWindow::primaryGeometry() const
{
    return geometry();
}

int MainWindow::direction() const
{
    return DockSettings::self()->direction();
}

int MainWindow::visibility() const
{
    return DockSettings::self()->visibility();
}

void MainWindow::setDirection(int direction)
{
    DockSettings::self()->setDirection(static_cast<DockSettings::Direction>(direction));
}

void MainWindow::setIconSize(int iconSize)
{
    DockSettings::self()->setIconSize(iconSize);
}

void MainWindow::setVisibility(int visibility)
{
    DockSettings::self()->setVisibility(static_cast<DockSettings::Visibility>(visibility));
}

int MainWindow::style() const
{
    return DockSettings::self()->style();
}

void MainWindow::setStyle(int style)
{
    DockSettings::self()->setStyle(static_cast<DockSettings::Style>(style));
}

void MainWindow::updateSize()
{
    resizeWindow();
}

QRect MainWindow::windowRect() const
{
    const QRect screenGeometry = screen()->geometry();
    const QRect availableGeometry = screen()->availableGeometry();

    bool isHorizontal = m_settings->direction() == DockSettings::Bottom;
    bool compositing = false;
    QQuickItem *item = qobject_cast<QQuickItem *>(rootObject());

    if (item) {
        compositing = item->property("compositing").toBool();
    }

    QSize newSize(0, 0);
    QPoint position(0, 0);
    int maxLength = isHorizontal ? screenGeometry.width() - m_settings->edgeMargins()
                                 : availableGeometry.height() - m_settings->edgeMargins();;

    // Add trash item.
    int appCount = m_appModel->rowCount() + 1;
    int iconSize = m_settings->iconSize();
    iconSize += iconSize * 0.1;
    int length = appCount * iconSize;
    int margins = compositing ? DockSettings::self()->edgeMargins() / 2 : 0;

    if (length >= maxLength) {
        iconSize = (maxLength - (maxLength % appCount)) / appCount;
        length = appCount * iconSize;
    }

    switch (m_settings->style()) {
    case DockSettings::Round: {
        switch (m_settings->direction()) {
        case DockSettings::Left:
            newSize = QSize(iconSize, length);
            position.setX(screenGeometry.x() + margins);
            // Handle the top statusbar.
            position.setY(availableGeometry.y() + (availableGeometry.height() - newSize.height()) / 2);
            break;
        case DockSettings::Bottom:
            newSize = QSize(length, iconSize);
            position.setX(screenGeometry.x() + (screenGeometry.width() - newSize.width()) / 2);
            position.setY(screenGeometry.y() + screenGeometry.height() - newSize.height() - margins);
            break;
        case DockSettings::Right:
            newSize = QSize(iconSize, length);
            position.setX(screenGeometry.x() + screenGeometry.width() - newSize.width() - margins);
            position.setY(availableGeometry.y() + (availableGeometry.height() - newSize.height()) / 2);
            break;
        default:
            break;
        }

        break;
    }
    case DockSettings::Straight: {
        switch (m_settings->direction()) {
        case DockSettings::Left:
            newSize = QSize(iconSize, screenGeometry.height());
            position.setX(screenGeometry.x());
            position.setY(screenGeometry.y());
            break;
        case DockSettings::Bottom:
            newSize = QSize(screenGeometry.width(), iconSize);
            position.setX(screenGeometry.x());
            position.setY(screenGeometry.y() + screenGeometry.height() - newSize.height());
            break;
        case DockSettings::Right:
            newSize = QSize(iconSize, screenGeometry.height());
            position.setX(screenGeometry.x() + screenGeometry.width() - newSize.width());
            position.setY(screenGeometry.y() + screenGeometry.height() - newSize.height());
            break;
        default:
            break;
        }
        break;
    }
    default:
        break;
    }

    return QRect(position, newSize);
}

QRect MainWindow::stripRect() const
{
    QRect rect = windowRect();
    const bool horizontal = m_settings->direction() == DockSettings::Bottom;

    if (horizontal)
        rect.setHeight(2);
    else
        rect.setWidth(2);

    return rect;
}

void MainWindow::resizeWindow()
{
    // Keep the edge strip while the panel is hidden: a geometry refresh (new
    // app, icon size change, ...) must not re-expand an invisible panel.
    if (m_dockHidden)
        setGeometry(stripRect());
    else
        setGeometry(windowRect());

    updateLayerShell();

    // The panel surface is re-created by Qt whenever the window is hidden and
    // re-exposed; refresh the anchor the taskbar entries attach to.
    XWindowInterface::instance()->setPanelWindow(this);

    emit resizingFished();
}

void MainWindow::initScreens()
{
    switch (m_settings->direction()) {
    default:
        setScreen(qGuiApp->primaryScreen());
        break;
    }
}

void MainWindow::updateLayerShell()
{
    if (!m_layerShell)
        return;

    bool compositing = false;
    QQuickItem *item = qobject_cast<QQuickItem *>(rootObject());

    if (item) {
        compositing = item->property("compositing").toBool();
    }

    const QRect rect = windowRect();
    const bool round = m_settings->style() == DockSettings::Round;
    // Floating panels keep a small gap to the screen edge, like the original
    // X11 position math did; straight panels hug the edge everywhere.
    const int gap = compositing && round ? m_settings->edgeMargins() / 2 : 0;

    LayerShellQt::Window::Anchors anchors = LayerShellQt::Window::AnchorNone;
    QMargins margins;

    switch (m_settings->direction()) {
    case DockSettings::Left:
        anchors |= LayerShellQt::Window::AnchorLeft;
        margins.setLeft(gap);
        if (!round) {
            anchors.setFlag(LayerShellQt::Window::AnchorTop);
            anchors.setFlag(LayerShellQt::Window::AnchorBottom);
        }
        break;
    case DockSettings::Bottom:
        anchors |= LayerShellQt::Window::AnchorBottom;
        margins.setBottom(gap);
        if (!round) {
            anchors.setFlag(LayerShellQt::Window::AnchorLeft);
            anchors.setFlag(LayerShellQt::Window::AnchorRight);
        }
        break;
    case DockSettings::Right:
        anchors |= LayerShellQt::Window::AnchorRight;
        margins.setRight(gap);
        if (!round) {
            anchors.setFlag(LayerShellQt::Window::AnchorTop);
            anchors.setFlag(LayerShellQt::Window::AnchorBottom);
        }
        break;
    default:
        break;
    }

    m_layerShell->setAnchors(anchors);
    m_layerShell->setMargins(margins);

    // Reserve the screen edge like the old extended struts: only when the
    // dock is supposed to stay visible. Auto-hide modes hand the space back,
    // leaving a thin strip the mouse can cross to reveal the dock.
    if (m_settings->visibility() == DockSettings::AlwaysShow || m_activity->launchPad()) {
        LayerShellQt::Window::Anchor edge = LayerShellQt::Window::AnchorNone;
        int zone = 0;

        switch (m_settings->direction()) {
        case DockSettings::Left:
            edge = LayerShellQt::Window::AnchorLeft;
            zone = rect.width();
            break;
        case DockSettings::Bottom:
            edge = LayerShellQt::Window::AnchorBottom;
            zone = rect.height();
            break;
        case DockSettings::Right:
            edge = LayerShellQt::Window::AnchorRight;
            zone = rect.width();
            break;
        default:
            break;
        }

        m_layerShell->setExclusiveEdge(edge);
        m_layerShell->setExclusiveZone(zone);
    } else {
        m_layerShell->setExclusiveEdge(LayerShellQt::Window::AnchorNone);
        m_layerShell->setExclusiveZone(0);
    }
}

void MainWindow::setDockHidden(bool hidden)
{
    if (m_dockHidden == hidden)
        return;

    m_dockHidden = hidden;

    if (hidden) {
        // Auto-hide: fade the QML layer out first (opacity is bound to
        // dockHidden) and only then shrink to the ~2px edge strip. Resizing a
        // layer surface while its content is still semi-visible makes the
        // compositor show artifacts; a fully transparent shrink does not. The
        // mouse can still cross the strip to reveal the panel.
        m_shrinkTimer->start();
    } else {
        // Grow back to the full panel before the fade-in starts, so the
        // content fades in over the complete surface instead of snapping.
        m_shrinkTimer->stop();
        setGeometry(windowRect());
    }

    XWindowInterface::instance()->setPanelWindow(this);

    emit dockHiddenChanged();
}

void MainWindow::onShrinkTimeout()
{
    if (!m_dockHidden)
        return;

    setGeometry(stripRect());
}

void MainWindow::onPrimaryScreenChanged(QScreen *screen)
{
    initScreens();
    setScreen(screen);
    resizeWindow();
}

void MainWindow::onPositionChanged()
{
    initScreens();
    updateLayerShell();
    resizeWindow();

    emit directionChanged();
    onVisibilityChanged();
}

void MainWindow::onIconSizeChanged()
{
    setGeometry(windowRect());
    updateLayerShell();

    emit iconSizeChanged();
}

void MainWindow::onVisibilityChanged()
{
    emit visibilityChanged();

    if (m_activity->launchPad()) {
        m_hideTimer->stop();
        updateLayerShell();
        setDockHidden(false);
        setVisible(true);
        return;
    }

    // Always show
    // Must remain displayed when launchpad is opened.
    if (m_settings->visibility() == DockSettings::AlwaysShow) {
        m_hideTimer->stop();

        updateLayerShell();
        setDockHidden(false);
        setVisible(true);
    }

    if (m_settings->visibility() == DockSettings::IntellHide) {
        updateLayerShell();
        setVisible(true);

        if (m_activity->existsWindowMaximized() && !m_hideBlocked) {
            setDockHidden(true);
        } else {
            setDockHidden(false);
        }
    }

    // Always hide
    if (m_settings->visibility() == DockSettings::AlwaysHide) {
        updateLayerShell();
        setVisible(true);
        setDockHidden(!m_hideBlocked);
    }
}

void MainWindow::onHideTimeout()
{
    if (m_activity->launchPad())
        return;

    if (m_settings->visibility() == DockSettings::AlwaysShow)
        return;

    if (m_settings->visibility() == DockSettings::IntellHide
            && !m_activity->existsWindowMaximized()) {
        return;
    }

    setDockHidden(true);
}

bool MainWindow::eventFilter(QObject *obj, QEvent *e)
{
    switch (e->type()) {
    case QEvent::Enter:
        m_hideTimer->stop();
        m_hideBlocked = true;

        // The mouse crossed the edge strip, reveal the panel.
        if (m_dockHidden && !m_showTimer->isActive())
            m_showTimer->start();
        break;
    case QEvent::Leave:
        m_hideBlocked = false;

        // The auto-hide timer only applies to the hiding visibilities; the
        // original guarded this through the fake window, which only existed in
        // those modes.
        if (!m_dockHidden
                && (m_settings->visibility() == DockSettings::AlwaysHide
                    || m_settings->visibility() == DockSettings::IntellHide)) {
            m_hideTimer->start();
        }
        break;
    case QEvent::DragEnter:
    case QEvent::DragMove:
        m_hideTimer->stop();
        if (m_dockHidden)
            setDockHidden(false);
        break;
    case QEvent::DragLeave:
    case QEvent::Drop:
        m_hideTimer->stop();
        break;
    default:
        break;
    }

    return QQuickView::eventFilter(obj, e);
}

void MainWindow::resizeEvent(QResizeEvent *e)
{
    emit primaryGeometryChanged();

    QQuickView::resizeEvent(e);
}

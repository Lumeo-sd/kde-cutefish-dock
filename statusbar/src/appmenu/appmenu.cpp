/*
  Copyright (c) 2011 Lionel Chauvin <megabigbug@yahoo.fr>
  Copyright (c) 2011,2012 Cédric Bellegarde <gnumdk@gmail.com>
  Copyright (c) 2016 Kai Uwe Broulik <kde@privat.broulik.de>

  Permission is hereby granted, free of charge, to any person obtaining a
  copy of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom the
  Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
  DEALINGS IN THE SOFTWARE.
*/

#include "appmenu.h"
#include "appmenudbus.h"
#include "appmenuadaptor.h"
#include "kdbusimporter.h"
#include "menuimporteradaptor.h"
#include "verticalmenu.h"

// Qt
#include <QApplication>
#include <QDBusInterface>
#include <QMenu>

AppMenu::AppMenu(QObject *parent)
    : QObject(parent)
//    , m_appmenuDBus(new AppmenuDBus(this))
{
    reconfigure();

//    m_appmenuDBus->connectToBus();

//    connect(m_appmenuDBus, &AppmenuDBus::appShowMenu, this, &AppMenu::slotShowMenu);
//    connect(m_appmenuDBus, &AppmenuDBus::reconfigured, this, &AppMenu::reconfigure);

//    // transfer our signals to dbus
//    connect(this, &AppMenu::showRequest, m_appmenuDBus, &AppmenuDBus::showRequest);
//    connect(this, &AppMenu::menuHidden, m_appmenuDBus, &AppmenuDBus::menuHidden);
//    connect(this, &AppMenu::menuShown, m_appmenuDBus, &AppmenuDBus::menuShown);

    m_menuViewWatcher = new QDBusServiceWatcher(QStringLiteral("com.cutefish.cappmenuview"),
                                                QDBusConnection::sessionBus(),
                                                QDBusServiceWatcher::WatchForRegistration | QDBusServiceWatcher::WatchForUnregistration,
                                                this);

    auto setupMenuImporter = [this]() {
        QDBusConnection::sessionBus().connect({},
                                              {},
                                              QStringLiteral("com.canonical.dbusmenu"),
                                              QStringLiteral("ItemActivationRequested"),
                                              this,
                                              SLOT(itemActivationRequested(int, uint)));

        // Setup a menu importer if needed
        if (!m_menuImporter) {
            m_menuImporter = new MenuImporter(this);
            connect(m_menuImporter, &MenuImporter::WindowRegistered, this, &AppMenu::slotWindowRegistered);
            m_menuImporter->connectToBus();
        }
    };

    connect(m_menuViewWatcher, &QDBusServiceWatcher::serviceRegistered, this, setupMenuImporter);
    connect(m_menuViewWatcher, &QDBusServiceWatcher::serviceUnregistered, this, [this](const QString &service) {
        Q_UNUSED(service)
        QDBusConnection::sessionBus().disconnect({},
                                                 {},
                                                 QStringLiteral("com.canonical.dbusmenu"),
                                                 QStringLiteral("ItemActivationRequested"),
                                                 this,
                                                 SLOT(itemActivationRequested(int, uint)));
        delete m_menuImporter;
        m_menuImporter = nullptr;
    });

    // if (QDBusConnection::sessionBus().interface()->isServiceRegistered(QStringLiteral("org.kde.kappmenuview"))) {
        setupMenuImporter();
    // }
}

AppMenu::~AppMenu()
{
}

bool AppMenu::eventFilter(QObject *object, QEvent *event)
{
    // HACK we need an input serial to create popups but Qt only sets them on click
//    if (object == m_menu && event->type() == QEvent::Enter && m_plasmashell) {
//        auto waylandWindow = dynamic_cast<QtWaylandClient::QWaylandWindow *>(m_menu->windowHandle()->handle());
//        if (waylandWindow) {
//            const auto device = waylandWindow->display()->currentInputDevice();
//            waylandWindow->display()->setLastInputDevice(device, device->pointer()->mEnterSerial, waylandWindow);
//        }
//    }
    return AppMenu::eventFilter(object, event);
}

void AppMenu::slotWindowRegistered(WId id, const QString &serviceName, const QDBusObjectPath &menuObjectPath)
{
    Q_UNUSED(id);

    // On X11 the status bar wrote the DBus service name and menu object path
    // back onto the window (_KDE_NET_WM_APPMENU_* properties) so that the
    // model could pick them up. On Wayland KWin keeps the registration itself
    // (org_kde_kwin_appmenu) and exposes it per-window through
    // PlasmaWindow::applicationMenuServiceName/ObjectPath, which AppMenuModel
    // reads directly; recording it here is a no-op.
    Q_UNUSED(serviceName);
    Q_UNUSED(menuObjectPath);
}

void AppMenu::slotShowMenu(int x, int y, const QString &serviceName, const QDBusObjectPath &menuObjectPath, int actionId)
{
    if (!m_menuImporter) {
        return;
    }

    // If menu visible, hide it
    if (m_menu && m_menu.data()->isVisible()) {
        m_menu.data()->hide();
        return;
    }

    // dbus call by user (for khotkey shortcut)
    if (x == -1 || y == -1) {
        // We do not know kwin button position, so tell kwin to show menu
        emit showRequest(serviceName, menuObjectPath, actionId);
        return;
    }

    auto *importer = new KDBusMenuImporter(serviceName, menuObjectPath.path(), this);
    QMetaObject::invokeMethod(importer, "updateMenu", Qt::QueuedConnection);
    disconnect(importer, nullptr, this, nullptr); // ensure we don't popup multiple times in case the menu updates again later

    connect(importer, &KDBusMenuImporter::menuUpdated, this, [=](QMenu *m) {
        QMenu *menu = importer->menu();
        if (!menu || menu != m) {
            return;
        }
        m_menu = qobject_cast<VerticalMenu *>(menu);

        m_menu.data()->setServiceName(serviceName);
        m_menu.data()->setMenuObjectPath(menuObjectPath);

        connect(m_menu.data(), &QMenu::aboutToHide, this, [this, importer] {
            hideMenu();
            importer->deleteLater();
        });

//        if (m_plasmashell) {
//            connect(m_menu.data(), &QMenu::aboutToShow, this, &AppMenuModule::initMenuWayland, Qt::UniqueConnection);
//            m_menu.data()->popup(QPoint(x, y));
//        } else {
            m_menu.data()->popup(QPoint(x, y) / qApp->devicePixelRatio());
//        }

        QAction *actiontoActivate = importer->actionForId(actionId);

        emit menuShown(serviceName, menuObjectPath);

        if (actiontoActivate) {
            m_menu.data()->setActiveAction(actiontoActivate);
        }
    });
}

void AppMenu::reconfigure()
{

}

void AppMenu::itemActivationRequested(int actionId, uint timeStamp)
{
    Q_UNUSED(timeStamp);
    emit showRequest(message().service(), QDBusObjectPath(message().path()), actionId);
}

void AppMenu::hideMenu()
{
    if (m_menu) {
        emit menuHidden(m_menu.data()->serviceName(), m_menu->menuObjectPath());
    }
}

void AppMenu::fakeUnityAboutToShow(const QString &service, const QDBusObjectPath &menuObjectPath)
{

}

KDBusMenuImporter *AppMenu::getImporter(const QString &service, const QString &path)
{

}

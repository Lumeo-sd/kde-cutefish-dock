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

#include <QApplication>
#include <QTranslator>
#include <QLocale>
#include <QStandardPaths>

#include "statusbar.h"
#include "controlcenterdialog.h"
#include "systemtray/systemtraymodel.h"
#include "appmenu/appmenumodel.h"
#include "appmenu/appmenuapplet.h"
#include "poweractions.h"
#include "notifications.h"
#include "backgroundhelper.h"

#include "appearance.h"
#include "brightness.h"
#include "battery.h"
#include "keyboardlayout.h"
#include "bluetoothcontrol.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    app.setApplicationName(QStringLiteral("cutefish-statusbar"));
    app.setOrganizationName(QStringLiteral("cutefishos"));

    const char *uri = "Cutefish.StatusBar";
    qmlRegisterType<SystemTrayModel>(uri, 1, 0, "SystemTrayModel");
    qmlRegisterType<ControlCenterDialog>(uri, 1, 0, "ControlCenterDialog");
    qmlRegisterType<Appearance>(uri, 1, 0, "Appearance");
    qmlRegisterType<Brightness>(uri, 1, 0, "Brightness");
    qmlRegisterType<Battery>(uri, 1, 0, "Battery");
    qmlRegisterType<AppMenuModel>(uri, 1, 0, "AppMenuModel");
    qmlRegisterType<AppMenuApplet>(uri, 1, 0, "AppMenuApplet");
    qmlRegisterType<PowerActions>(uri, 1, 0, "PowerActions");
    qmlRegisterType<Notifications>(uri, 1, 0, "Notifications");
    qmlRegisterType<BackgroundHelper>(uri, 1, 0, "BackgroundHelper");
    qmlRegisterType<KeyboardLayout>(uri, 1, 0, "KeyboardLayout");
    qmlRegisterType<BluetoothControl>(uri, 1, 0, "BluetoothControl");

    // Look for the translations in the standard data dirs (the private
    // ~/.local prefix first), like the dock and the launcher do.
    const QString translationsDir = QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                                           QStringLiteral("cutefish-statusbar/translations"),
                                                           QStandardPaths::LocateDirectory);
    const QString qmFilePath = translationsDir + QLatin1Char('/') + QLocale::system().name() + QLatin1String(".qm");
    if (QFile::exists(qmFilePath)) {
        QTranslator *translator = new QTranslator(QApplication::instance());
        if (translator->load(qmFilePath)) {
            QGuiApplication::installTranslator(translator);
        } else {
            translator->deleteLater();
        }
    }

    StatusBar bar;

    if (!QDBusConnection::sessionBus().registerService("com.cutefish.Statusbar")) {
        return -1;
    }

    if (!QDBusConnection::sessionBus().registerObject("/Statusbar", &bar)) {
        return -1;
    }

    return app.exec();
}

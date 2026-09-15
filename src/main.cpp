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

#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QLocale>
#include <QStandardPaths>
#include <QTranslator>
#include <QFile>
#include <QIcon>

#include "processhelper.h"
#include "utils.h"
#include "fonts.h"

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps, true);
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling, true);
    QApplication app(argc, argv);
    app.setOrganizationName("cutefishos");
    app.setWindowIcon(QIcon::fromTheme("terminal"));

    QQmlApplicationEngine engine;

    // Private-prefix install: FishUI and Cutefish.TermWidget live under
    // <prefix>/lib64/qt6/qml (Qt6 ignores QT_QML_IMPORT_PATH at runtime).
    engine.addImportPath(QCoreApplication::applicationDirPath()
                         + QStringLiteral("/../lib64/qt6/qml"));

    // Translations: look in every XDG data directory (private ~/.local
    // install keeps them under ~/.local/share/cutefish-terminal/translations).
    QLocale locale;
    const QString localeFile = QStringLiteral("%1.qm").arg(locale.name());
    const QStringList translationDirs = QStandardPaths::locateAll(
        QStandardPaths::GenericDataLocation,
        QStringLiteral("cutefish-terminal/translations"),
        QStandardPaths::LocateDirectory);

    for (const QString &dir : translationDirs) {
        const QString qmFilePath = dir + QLatin1Char('/') + localeFile;
        if (!QFile::exists(qmFilePath))
            continue;

        QTranslator *translator = new QTranslator(QGuiApplication::instance());
        if (translator->load(qmFilePath)) {
            QGuiApplication::installTranslator(translator);
        } else {
            translator->deleteLater();
        }
        break;
    }

    engine.rootContext()->setContextProperty("Process", new ProcessHelper);
    engine.rootContext()->setContextProperty("Utils", new Utils);
    engine.rootContext()->setContextProperty("Fonts", new Fonts);

    engine.addImportPath(QStringLiteral("qrc:/"));
    engine.load(QUrl(QStringLiteral("qrc:/qml/main.qml")));

    return app.exec();
}

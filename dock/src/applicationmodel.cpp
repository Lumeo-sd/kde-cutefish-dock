/*
 * Copyright (C) 2021 CutefishOS Team.
 *
 * Author:     rekols <revenmartin@gmail.com>
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

#include "applicationmodel.h"
#include "processprovider.h"
#include "utils.h"

#include <QDir>
#include <QIcon>
#include <QProcess>
#include <QPixmap>
#include <QRegularExpression>
#include <QUrl>

ApplicationModel::ApplicationModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_iface(XWindowInterface::instance())
    , m_sysAppMonitor(SystemAppMonitor::self())
{
    connect(m_iface, &XWindowInterface::windowAdded, this, &ApplicationModel::onWindowAdded);
    connect(m_iface, &XWindowInterface::windowRemoved, this, &ApplicationModel::onWindowRemoved);
    connect(m_iface, &XWindowInterface::activeChanged, this, &ApplicationModel::onActiveChanged);

    initPinnedApplications();

    qInfo() << "cutefish-dock debug build ready, rowCount=" << rowCount() << "debug-fork-3";

    QTimer::singleShot(100, m_iface, &XWindowInterface::startInitWindows);
}

int ApplicationModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)

    return m_appItems.size();
}

QHash<int, QByteArray> ApplicationModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[AppIdRole] = "appId";
    roles[IconNameRole] = "iconName";
    roles[VisibleNameRole] = "visibleName";
    roles[ActiveRole] = "isActive";
    roles[WindowCountRole] = "windowCount";
    roles[IsPinnedRole] = "isPinned";
    roles[DesktopFileRole] = "desktopFile";
    roles[FixedItemRole] = "fixed";
    roles[DropSlotRole] = "dropSlot";
    return roles;
}

QVariant ApplicationModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    ApplicationItem *item = m_appItems.at(index.row());

    switch (role) {
    case AppIdRole:
        return item->id;
    case IconNameRole:
        return item->iconName;
    case VisibleNameRole:
        return item->visibleName;
    case ActiveRole:
        return item->isActive;
    case WindowCountRole:
        return item->wids.count();
    case IsPinnedRole:
        return item->isPinned;
    case DesktopFileRole:
        return item->desktopPath;
    case FixedItemRole:
        return item->fixed;
    case DropSlotRole:
        return item->dropSlot;
    default:
        return QVariant();
    }

    return QVariant();
}

void ApplicationModel::addItem(const QString &desktopFile)
{
    ApplicationItem *existsItem = findItemByDesktop(desktopFile);

    if (existsItem) {
        existsItem->isPinned = true;
        return;
    }

    beginInsertRows(QModelIndex(), rowCount(), rowCount());
    ApplicationItem *item = new ApplicationItem;
    QMap<QString, QString> desktopInfo = Utils::instance()->readInfoFromDesktop(desktopFile);
    item->iconName = desktopInfo.value("Icon");
    item->visibleName = desktopInfo.value("Name");
    item->exec = desktopInfo.value("Exec");
    item->desktopPath = desktopFile;
    item->isPinned = true;

    // First use filename as the id of the item.
    // Why not use exec? Because exec contains the file path,
    // QSettings will have problems, resulting in unrecognized next time.
    QFileInfo fi(desktopFile);
    item->id = fi.completeBaseName();

    m_appItems << item;
    endInsertRows();

    savePinAndUnPinList();

    emit itemAdded();
    emit countChanged();
}

// Insert a new pinned app at a specific row so drag & drop can place it in the
// middle instead of always appending at the end.
void ApplicationModel::insertItem(const QString &desktopFile, int index)
{
    ApplicationItem *existsItem = findItemByDesktop(desktopFile);

    if (existsItem) {
        // Already present (pinned or running) -> just relocate it.
        existsItem->isPinned = true;
        moveItem(existsItem, index);
        handleDataChangedFromItem(existsItem);
        savePinAndUnPinList();
        return;
    }

    // A drop slot is visible where the user releases: reuse that row in place
    // (no insert/remove pair), so the gap the user aimed at becomes the pin.
    int slot = dropSlotIndex();

    if (slot != -1) {
        ApplicationItem *item = m_appItems.at(slot);

        QMap<QString, QString> desktopInfo = Utils::instance()->readInfoFromDesktop(desktopFile);
        item->iconName = desktopInfo.value("Icon");
        item->visibleName = desktopInfo.value("Name");
        item->exec = desktopInfo.value("Exec");
        item->desktopPath = desktopFile;
        item->isPinned = true;
        item->fixed = false;
        item->dropSlot = false;

        QFileInfo fi(desktopFile);
        item->id = fi.completeBaseName();

        moveItem(item, index);

        // The slot row became a real pinned app; refresh the delegate so the
        // icon/name appear immediately (dropSlot=false, fixed=false, ...).
        handleDataChangedFromItem(item);

        savePinAndUnPinList();
        emit itemAdded();
        emit countChanged();
        qInfo() << "slot insert" << desktopFile << "at" << index;
        return;
    }

    // Plain insert (e.g. no slot was ever created).
    int from = qBound(1, index, rowCount());
    beginInsertRows(QModelIndex(), from, from);
    ApplicationItem *item = new ApplicationItem;
    QMap<QString, QString> desktopInfo = Utils::instance()->readInfoFromDesktop(desktopFile);
    item->iconName = desktopInfo.value("Icon");
    item->visibleName = desktopInfo.value("Name");
    item->exec = desktopInfo.value("Exec");
    item->desktopPath = desktopFile;
    item->isPinned = true;

    QFileInfo fi(desktopFile);
    item->id = fi.completeBaseName();

    m_appItems.insert(from, item);
    endInsertRows();

    savePinAndUnPinList();
    emit itemAdded();
    emit countChanged();
}

// --- Drop slot (live insertion gap while an external drag hovers the dock) ---

void ApplicationModel::beginDropSlot(int index)
{
    int slot = dropSlotIndex();

    if (slot != -1) {
        moveDropSlot(index);
        return;
    }

    index = qBound(1, index, rowCount());

    beginInsertRows(QModelIndex(), index, index);
    ApplicationItem *item = new ApplicationItem;
    item->id = "__dropslot__";
    item->dropSlot = true;
    item->fixed = true;
    m_appItems.insert(index, item);
    endInsertRows();
    qInfo() << "slot begin" << index;
}

void ApplicationModel::moveDropSlot(int index)
{
    int from = dropSlotIndex();

    if (from == -1 || from == index)
        return;

    index = qBound(1, index, rowCount() - 1);

    moveItem(m_appItems.at(from), index);
    qInfo() << "slot move" << from << "->" << index;
}

bool ApplicationModel::endDropSlot()
{
    int from = dropSlotIndex();

    if (from == -1)
        return false;

    beginRemoveRows(QModelIndex(), from, from);
    ApplicationItem *item = m_appItems.takeAt(from);
    endRemoveRows();
    delete item;
    qInfo() << "slot end";
    return true;
}

int ApplicationModel::dropSlotIndex() const
{
    for (int i = 0; i < m_appItems.size(); ++i) {
        if (m_appItems.at(i)->dropSlot)
            return i;
    }

    return -1;
}

bool ApplicationModel::dropSlotActive() const
{
    return dropSlotIndex() != -1;
}

QString ApplicationModel::internalDragIconName()
{
    ApplicationItem *item = findItemById(m_dragItemId);

    if (!item || item->dropSlot)
        return QString();

    return item->iconName;
}

// QML debug bridge — see header.
void ApplicationModel::dbg(const QString &msg)
{
    qInfo().noquote() << "[qml]" << msg;
}

// Render the app's icon to a temp PNG and return its file URL for the drag
// ghost. AppItem binds Drag.imageSource to this URL, so Qt's drag manager
// starts loading the image at delegate creation — long before any press — and
// by the time a drag actually starts the pixmap is ready, which is the only
// way a Wayland ghost can show the icon on the very first drag. The size
// matches the dock icon size (in device pixels) so the ghost is not enlarged.
QString ApplicationModel::dragIconSource(const QString &appId, int size)
{
    ApplicationItem *item = findItemById(appId);

    // Fixed anchors (launcher) are never dragged — skip the render entirely.
    if (!item || item->iconName.isEmpty() || item->fixed || size <= 0)
        return QString();

    QPixmap pixmap;

    if (item->iconName.startsWith(QLatin1String("qrc:"))) {
        // Qt Quick's "qrc:" scheme maps to the resource system's ":/" prefix,
        // which is what QPixmap understands.
        const QString res = QStringLiteral(":/") + item->iconName.mid(4);
        pixmap = QPixmap(res);

        if (pixmap.isNull())
            return QString();

        pixmap = pixmap.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    } else if (item->iconName.startsWith(QLatin1Char('/'))) {
        QIcon icon(item->iconName);

        if (icon.isNull())
            return QString();

        pixmap = icon.pixmap(size, size);
    } else {
        QIcon icon = QIcon::fromTheme(item->iconName);

        if (icon.isNull())
            return QString();

        pixmap = icon.pixmap(size, size);
    }

    if (pixmap.isNull())
        return QString();

    QString safeId = appId;
    safeId.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_.-]")), QStringLiteral("_"));

    const QString path = QDir::tempPath()
                         + QStringLiteral("/cutefish-dock-drag-") + safeId + QStringLiteral(".png");

    if (pixmap.save(path, "PNG"))
        return QUrl::fromLocalFile(path).toString();

    return QString();
}

// --- Internal reorder (dragging an already-pinned icon) ---

// Remember where an internal drag started. The dragged icon stays in the list
// (its icon is hidden by the delegate), so the row it occupies is the live
// gap; moveInternalGap() slides that empty row along the cursor. This gives
// the same "icons slide apart, gap follows the pointer" feel as the external
// drag & drop.
void ApplicationModel::beginInternalDrag(const QString &id)
{
    m_dragItemId = id;
    m_dragFrom = indexOf(id);
    qInfo() << "internal begin" << id << "from" << m_dragFrom;
}

void ApplicationModel::moveInternalGap(int index)
{
    ApplicationItem *item = findItemById(m_dragItemId);

    if (!item)
        return;

    int from = m_appItems.indexOf(item);

    if (from == -1 || from == index)
        return;

    moveItem(item, qBound(1, index, rowCount() - 1));
    qInfo() << "internal move" << index << "(from" << from << ") ->" << m_appItems.indexOf(item);
}

// The user dropped the icon on the dock: it already sits exactly where the gap
// was (it followed the cursor), so persisting the new order is all that is
// needed.
void ApplicationModel::endInternalDrag()
{
    qInfo() << "internal end (save)" << m_dragItemId << "at" << indexOf(m_dragItemId);
    m_dragItemId.clear();
    m_dragFrom = -1;
    savePinAndUnPinList();
}

// The drag left the dock (or was cancelled) without a drop: put the icon back
// where it was picked up.
void ApplicationModel::restoreInternalDrag()
{
    ApplicationItem *item = findItemById(m_dragItemId);
    m_dragItemId.clear();

    if (!item)
        return;

    int from = m_appItems.indexOf(item);

    if (from == -1)
        return;

    int to = qBound(0, m_dragFrom, rowCount() - 1);
    m_dragFrom = -1;

    if (from != to)
        moveItem(item, to);
    qInfo() << "internal restore" << "from" << from << "to" << to;
}

void ApplicationModel::moveItem(ApplicationItem *item, int to)
{
    int from = m_appItems.indexOf(item);

    if (from == -1 || from == to)
        return;

    to = qBound(0, to, rowCount() - 1);

    qInfo() << "model move" << item->id << "from" << from << "to" << to;

    m_appItems.move(from, to);

    if (from < to)
        beginMoveRows(QModelIndex(), from, from, QModelIndex(), to + 1);
    else
        beginMoveRows(QModelIndex(), from, from, QModelIndex(), to);

    endMoveRows();
}

void ApplicationModel::removeItem(const QString &desktopFile)
{
    ApplicationItem *item = findItemByDesktop(desktopFile);

    if (item) {
        ApplicationModel::unPin(item->id);
    }
}

bool ApplicationModel::desktopContains(const QString &desktopFile)
{
    if (desktopFile.isEmpty())
        return false;

    return findItemByDesktop(desktopFile) != nullptr;
}

bool ApplicationModel::isDesktopPinned(const QString &desktopFile)
{
    ApplicationItem *item = findItemByDesktop(desktopFile);

    if (item) {
        return item->isPinned;
    }

    return false;
}

void ApplicationModel::clicked(const QString &id)
{
    ApplicationItem *item = findItemById(id);

    if (!item)
        return;

    // Application Item that has been pinned,
    // We need to open it.
    if (item->wids.isEmpty()) {
        // open application
        openNewInstance(item->id);
    }
    // Multiple windows have been opened and need to switch between them,
    // The logic here needs to be improved.
    else if (item->wids.count() > 1) {
        item->currentActive++;

        if (item->currentActive == item->wids.count())
            item->currentActive = 0;

        m_iface->forceActiveWindow(item->wids.at(item->currentActive));
    } else if (m_iface->activeWindow() == item->wids.first()) {
        m_iface->minimizeWindow(item->wids.first());
    } else {
        m_iface->forceActiveWindow(item->wids.first());
    }
}

void ApplicationModel::raiseWindow(const QString &id)
{
    ApplicationItem *item = findItemById(id);

    if (!item || item->wids.isEmpty())
        return;

    if (item->currentActive < 0 || item->currentActive >= item->wids.size())
        item->currentActive = 0;

    m_iface->forceActiveWindow(item->wids.at(item->currentActive));
}

// The Exec value of a .desktop file carries desktop field codes (%U, %F, %i,
// %c, %k, ...) and, for Flatpak apps, "@@ ... @@" file-forwarding groups. The
// dock launches an app without any file arguments, so those codes must be
// dropped: passing a literal "%U" makes e.g. Flatpak refuse to start. Parsed
// shell-style (quotes/backslash escapes) like the desktop spec requires.
static QStringList launchArguments(const QString &exec)
{
    QStringList args;
    QString token;
    bool inSingle = false;
    bool inDouble = false;

    for (int i = 0; i < exec.size(); ++i) {
        const QChar c = exec.at(i);

        if (inSingle) {
            if (c == QLatin1Char('\''))
                inSingle = false;
            else
                token += c;
            continue;
        }

        if (inDouble) {
            if (c == QLatin1Char('"'))
                inDouble = false;
            else if (c == QLatin1Char('\\') && i + 1 < exec.size())
                token += exec.at(++i);
            else
                token += c;
            continue;
        }

        if (c == QLatin1Char('\'')) {
            inSingle = true;
        } else if (c == QLatin1Char('"')) {
            inDouble = true;
        } else if (c.isSpace()) {
            if (!token.isEmpty()) {
                args << token;
                token.clear();
            }
        } else if (c == QLatin1Char('\\') && i + 1 < exec.size()) {
            token += exec.at(++i);
        } else {
            token += c;
        }
    }

    if (!token.isEmpty())
        args << token;

    // Drop field codes and flatpak file-forwarding groups.
    QStringList cleaned;
    bool inForwardGroup = false;

    for (const QString &arg : args) {
        if (arg.startsWith(QStringLiteral("@@"))) {
            inForwardGroup = true;
            continue;
        }
        if (inForwardGroup) {
            if (arg == QStringLiteral("@@"))
                inForwardGroup = false;
            continue;
        }

        const QString lower = arg.toLower();
        if (lower == QStringLiteral("%u") || lower == QStringLiteral("%f")
                || lower == QStringLiteral("%i") || lower == QStringLiteral("%c")
                || lower == QStringLiteral("%k") || lower == QStringLiteral("%v")
                || lower == QStringLiteral("%m"))
            continue;

        QString cleanedArg = arg;
        cleaned << cleanedArg.replace(QStringLiteral("%%"), QStringLiteral("%"));
    }

    return cleaned;
}

bool ApplicationModel::openNewInstance(const QString &appId)
{
    ApplicationItem *item = findItemById(appId);

    if (!item)
        return false;

    if (!item->exec.isEmpty()) {
        const QStringList launchArgs = launchArguments(item->exec);

        if (launchArgs.isEmpty())
            return false;

        if (launchArgs.size() > 1)
            ProcessProvider::startDetached(launchArgs.first(), launchArgs.mid(1));
        else
            ProcessProvider::startDetached(launchArgs.first());
    } else {
        ProcessProvider::startDetached(appId);
    }

    return true;
}

void ApplicationModel::closeAllByAppId(const QString &appId)
{
    ApplicationItem *item = findItemById(appId);

    if (!item)
        return;

    for (quint64 wid : item->wids) {
        m_iface->closeWindow(wid);
    }
}

void ApplicationModel::pin(const QString &appId)
{
    ApplicationItem *item = findItemById(appId);

    if (!item)
        return;

    item->isPinned = true;

    handleDataChangedFromItem(item);
    savePinAndUnPinList();
}

void ApplicationModel::unPin(const QString &appId)
{
    qInfo() << "unpin" << appId;
    ApplicationItem *item = findItemById(appId);

    if (!item)
        return;

    item->isPinned = false;
    handleDataChangedFromItem(item);

    // Need to be removed after unpin
    if (item->wids.isEmpty()) {
        int index = indexOf(item->id);
        if (index != -1) {
            beginRemoveRows(QModelIndex(), index, index);
            m_appItems.removeAll(item);
            endRemoveRows();

            emit itemRemoved();
            emit countChanged();
        }
    }

    savePinAndUnPinList();
}

void ApplicationModel::updateGeometries(const QString &id, QRect rect)
{
    ApplicationItem *item = findItemById(id);

    // If not found
    if (!item)
        return;

    for (quint64 id : item->wids) {
        m_iface->setIconGeometry(id, rect);
    }
}

void ApplicationModel::move(int from, int to)
{
    if (from == to)
        return;

    m_appItems.move(from, to);

    if (from < to)
        beginMoveRows(QModelIndex(), from, from, QModelIndex(), to + 1);
    else
        beginMoveRows(QModelIndex(), from, from, QModelIndex(), to);

    endMoveRows();
}

ApplicationItem *ApplicationModel::findItemByWId(quint64 wid)
{
    for (ApplicationItem *item : m_appItems) {
        for (quint64 winId : item->wids) {
            if (winId == wid)
                return item;
        }
    }

    return nullptr;
}

ApplicationItem *ApplicationModel::findItemById(const QString &id)
{
    for (ApplicationItem *item : m_appItems) {
        if (item->id == id)
            return item;
    }

    return nullptr;
}

ApplicationItem *ApplicationModel::findItemByDesktop(const QString &desktop)
{
    for (ApplicationItem *item : m_appItems) {
        if (item->desktopPath == desktop)
            return item;
    }

    return nullptr;
}

bool ApplicationModel::contains(const QString &id)
{
    for (ApplicationItem *item : qAsConst(m_appItems)) {
        if (item->id == id)
            return true;
    }

    return false;
}

int ApplicationModel::indexOf(const QString &id)
{
    for (ApplicationItem *item : m_appItems) {
        if (item->id == id)
            return m_appItems.indexOf(item);
    }

    return -1;
}

void ApplicationModel::initPinnedApplications()
{
    QSettings settings(QSettings::UserScope, "cutefishos", "dock_pinned");
    QSettings systemSettings("/etc/cutefish-dock-list.conf", QSettings::IniFormat);
    QSettings *set = (QFile(settings.fileName()).exists()) ? &settings
                                                           : &systemSettings;
    QStringList groups = set->childGroups();

    // Launcher
    ApplicationItem *item = new ApplicationItem;
    item->id = "cutefish-launcher";
    item->exec = "cutefish-launcher";
    item->iconName = "qrc:/images/launcher.svg";
    item->visibleName = tr("Launcher");
    item->fixed = true;
    m_appItems.append(item);

    // Pinned Apps
    for (int i = 0; i < groups.size(); ++i) {
        for (const QString &id : groups) {
            set->beginGroup(id);
            int index = set->value("Index").toInt();

            if (index == i) {
                beginInsertRows(QModelIndex(), rowCount(), rowCount());
                ApplicationItem *item = new ApplicationItem;

                item->desktopPath = set->value("DesktopPath").toString();
                item->id = id;
                item->isPinned = true;

                if (!QFile(item->desktopPath).exists()) {
                    set->endGroup();
                    continue;
                }

                // Read from desktop file.
                if (!item->desktopPath.isEmpty()) {
                    QMap<QString, QString> desktopInfo = Utils::instance()->readInfoFromDesktop(item->desktopPath);
                    item->iconName = desktopInfo.value("Icon");
                    item->visibleName = desktopInfo.value("Name");
                    item->exec = desktopInfo.value("Exec");
                }

                // Read from config file.
                if (item->iconName.isEmpty())
                    item->iconName = set->value("Icon").toString();

                if (item->visibleName.isEmpty())
                    item->visibleName = set->value("VisibleName").toString();

                if (item->exec.isEmpty())
                    item->exec = set->value("Exec").toString();

                m_appItems.append(item);
                endInsertRows();

                emit itemAdded();
                emit countChanged();

                set->endGroup();
                break;
            } else {
                set->endGroup();
            }
        }
    }
}

void ApplicationModel::savePinAndUnPinList()
{
    QSettings settings(QSettings::UserScope, "cutefishos", "dock_pinned");
    settings.clear();

    int index = 0;

    for (ApplicationItem *item : m_appItems) {
        if (item->isPinned) {
            settings.beginGroup(item->id);
            settings.setValue("Index", index);
            settings.setValue("Icon", item->iconName);
            settings.setValue("VisibleName", item->visibleName);
            settings.setValue("Exec", item->exec);
            settings.setValue("DesktopPath", item->desktopPath);
            settings.endGroup();
            ++index;
        }
    }

    settings.sync();
}

void ApplicationModel::handleDataChangedFromItem(ApplicationItem *item)
{
    if (!item)
        return;

    QModelIndex idx = index(indexOf(item->id), 0, QModelIndex());

    if (idx.isValid()) {
        emit dataChanged(idx, idx);
    }
}

void ApplicationModel::onWindowAdded(quint64 wid)
{
    QMap<QString, QVariant> info = m_iface->requestInfo(wid);
    const QString id = info.value("id").toString();

    // Skip...
    if (id == "cutefish-launcher")
        return;

    QString desktopPath = m_iface->desktopFilePath(wid);
    ApplicationItem *desktopItem = findItemByDesktop(desktopPath);

    // Use desktop find
    if (!desktopPath.isEmpty() && desktopItem != nullptr) {
        desktopItem->wids.append(wid);
        // Need to update application active status.
        desktopItem->isActive = info.value("active").toBool();

        if (desktopItem->id != id) {
            desktopItem->id = id;
            savePinAndUnPinList();
        }

        handleDataChangedFromItem(desktopItem);
    }
    // Find from id
    else if (contains(id)) {
        for (ApplicationItem *item : m_appItems) {
            if (item->id == id) {
                item->wids.append(wid);
                // Need to update application active status.
                item->isActive = info.value("active").toBool();
                handleDataChangedFromItem(item);
            }
        }
    }
    // New item needs to be added.
    else {
        beginInsertRows(QModelIndex(), rowCount(), rowCount());
        ApplicationItem *item = new ApplicationItem;
        item->id = id;
        item->iconName = info.value("iconName").toString();
        item->visibleName = info.value("visibleName").toString();
        item->isActive = info.value("active").toBool();
        item->wids.append(wid);

        if (!desktopPath.isEmpty()) {
            QMap<QString, QString> desktopInfo = Utils::instance()->readInfoFromDesktop(desktopPath);
            item->iconName = desktopInfo.value("Icon");
            item->visibleName = desktopInfo.value("Name");
            item->exec = desktopInfo.value("Exec");
            item->desktopPath = desktopPath;
        }

        m_appItems << item;
        endInsertRows();

        emit itemAdded();
        emit countChanged();
    }
}

void ApplicationModel::onWindowRemoved(quint64 wid)
{
    ApplicationItem *item = findItemByWId(wid);

    if (!item)
        return;

    // Remove from wid list.
    item->wids.removeOne(wid);

    if (item->currentActive >= item->wids.size())
        item->currentActive = 0;

    handleDataChangedFromItem(item);

    if (item->wids.isEmpty()) {
        // If it is not fixed to the dock, need to remove it.
        if (!item->isPinned) {
            int index = indexOf(item->id);

            if (index == -1)
                return;

            beginRemoveRows(QModelIndex(), index, index);
            m_appItems.removeAll(item);
            endRemoveRows();

            emit itemRemoved();
            emit countChanged();
        }
    }
}

void ApplicationModel::onActiveChanged(quint64 wid)
{
    // Using this method will cause the listview scrollbar to reset.
    // beginResetModel();

    for (ApplicationItem *item : m_appItems) {
        if (item->isActive != item->wids.contains(wid)) {
            item->isActive = item->wids.contains(wid);

            QModelIndex idx = index(indexOf(item->id), 0, QModelIndex());
            if (idx.isValid()) {
                emit dataChanged(idx, idx);
            }
        }
    }
}

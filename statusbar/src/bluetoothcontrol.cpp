/*
 * Status bar - bluetooth control
 *
 * Talks to org.bluez directly over D-Bus.
 *
 * Why not BluezQt (Cutefish.Bluetooth / KF6 BluezQt): the BlueZ version
 * shipped on this system (5.87, Fedora 44) only exposes the adapter power
 * state as a WRITABLE PROPERTY (org.bluez.Adapter1 "Powered" through
 * org.freedesktop.DBus.Properties.Set). The classic org.bluez.Adapter1
 * "SetPowered" method no longer exists, so BluezQt's setPowered() silently
 * fails and the toggle does nothing. Talking to org.bluez directly fixes
 * powering on/off, the indicator state and device connect/disconnect.
 *
 * SPDX-FileCopyrightText: 2026
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "bluetoothcontrol.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QDBusVariant>
#include <QDebug>

using InterfacesMap = QMap<QString, QVariantMap>;
using ObjectsMap = QMap<QDBusObjectPath, InterfacesMap>;

Q_DECLARE_METATYPE(InterfacesMap)
Q_DECLARE_METATYPE(ObjectsMap)

BluetoothControl::BluetoothControl(QObject *parent)
    : QObject(parent)
{
    qDBusRegisterMetaType<InterfacesMap>();
    qDBusRegisterMetaType<ObjectsMap>();

    m_objectManager = new QDBusInterface(QStringLiteral("org.bluez"),
                                         QStringLiteral("/"),
                                         QStringLiteral("org.freedesktop.DBus.ObjectManager"),
                                         QDBusConnection::systemBus(),
                                         this);

    // Refresh whenever anything on org.bluez changes.
    QDBusConnection::systemBus().connect(QStringLiteral("org.bluez"),
                                         QString(),
                                         QStringLiteral("org.freedesktop.DBus.Properties"),
                                         QStringLiteral("PropertiesChanged"),
                                         this,
                                         SLOT(onPropertiesChanged(QDBusMessage)));

    QDBusConnection::systemBus().connect(QStringLiteral("org.bluez"),
                                         QStringLiteral("/"),
                                         QStringLiteral("org.freedesktop.DBus.ObjectManager"),
                                         QStringLiteral("InterfacesAdded"),
                                         this,
                                         SLOT(onInterfacesAdded(QDBusMessage)));

    QDBusConnection::systemBus().connect(QStringLiteral("org.bluez"),
                                         QStringLiteral("/"),
                                         QStringLiteral("org.freedesktop.DBus.ObjectManager"),
                                         QStringLiteral("InterfacesRemoved"),
                                         this,
                                         SLOT(onInterfacesRemoved(QDBusMessage)));

    refresh();
}

bool BluetoothControl::available() const
{
    return m_available;
}

bool BluetoothControl::powered() const
{
    return m_powered;
}

void BluetoothControl::setPowered(bool powered)
{
    if (m_adapterPath.isEmpty()) {
        return;
    }

    // New BlueZ (>= 5.8x on Fedora 44) has no SetPowered method; the power
    // state is a writable "Powered" property on org.bluez.Adapter1.
    QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("org.bluez"),
                                                      m_adapterPath,
                                                      QStringLiteral("org.freedesktop.DBus.Properties"),
                                                      QStringLiteral("Set"));
    msg << QStringLiteral("org.bluez.Adapter1")
        << QStringLiteral("Powered")
        << QVariant::fromValue(QDBusVariant(powered));

    QDBusMessage reply = QDBusConnection::systemBus().call(msg);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        qWarning() << "BluetoothControl: Set Powered failed:" << reply.errorMessage();
    }

    refresh();
}

QVariantList BluetoothControl::devices() const
{
    return m_devices;
}

void BluetoothControl::connectDevice(const QString &address)
{
    const QString path = devicePath(address);
    if (path.isEmpty()) {
        return;
    }

    QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("org.bluez"),
                                                      path,
                                                      QStringLiteral("org.bluez.Device1"),
                                                      QStringLiteral("Connect"));
    QDBusMessage reply = QDBusConnection::systemBus().call(msg);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        qWarning() << "BluetoothControl: Connect" << address << "failed:" << reply.errorMessage();
    }

    refresh();
}

void BluetoothControl::disconnectDevice(const QString &address)
{
    const QString path = devicePath(address);
    if (path.isEmpty()) {
        return;
    }

    QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("org.bluez"),
                                                      path,
                                                      QStringLiteral("org.bluez.Device1"),
                                                      QStringLiteral("Disconnect"));
    QDBusMessage reply = QDBusConnection::systemBus().call(msg);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        qWarning() << "BluetoothControl: Disconnect" << address << "failed:" << reply.errorMessage();
    }

    refresh();
}

void BluetoothControl::onPropertiesChanged(const QDBusMessage &message)
{
    Q_UNUSED(message)
    refresh();
}

void BluetoothControl::onInterfacesAdded(const QDBusMessage &message)
{
    Q_UNUSED(message)
    refresh();
}

void BluetoothControl::onInterfacesRemoved(const QDBusMessage &message)
{
    Q_UNUSED(message)
    refresh();
}

void BluetoothControl::refresh()
{
    if (!m_objectManager) {
        return;
    }

    QDBusReply<ObjectsMap> reply = m_objectManager->call(QStringLiteral("GetManagedObjects"));
    if (!reply.isValid()) {
        qWarning() << "BluetoothControl: GetManagedObjects failed:" << reply.error().message();
        return;
    }

    // a{oa{sa{sv}}} demarshalling via QDBusReply + registered metatypes.
    const ObjectsMap objects = reply.value();

    QString adapterPath;
    bool powered = false;
    QVariantList devices;
    QHash<QString, QString> devicePaths;

    for (auto it = objects.constBegin(); it != objects.constEnd(); ++it) {
        const QString path = it.key().path();
        const InterfacesMap &interfaces = it.value();

        if (interfaces.contains(QStringLiteral("org.bluez.Adapter1")) && adapterPath.isEmpty()) {
            adapterPath = path;
            powered = interfaces.value(QStringLiteral("org.bluez.Adapter1"))
                          .value(QStringLiteral("Powered"), false)
                          .toBool();
        }

        if (interfaces.contains(QStringLiteral("org.bluez.Device1"))) {
            const QVariantMap props = interfaces.value(QStringLiteral("org.bluez.Device1"));
            QVariantMap device;
            device.insert(QStringLiteral("path"), path);
            device.insert(QStringLiteral("address"), props.value(QStringLiteral("Address")).toString());
            device.insert(QStringLiteral("name"), props.value(QStringLiteral("Alias")).toString());
            device.insert(QStringLiteral("connected"), props.value(QStringLiteral("Connected"), false).toBool());
            device.insert(QStringLiteral("paired"), props.value(QStringLiteral("Paired"), false).toBool());
            devices.append(device);
            const QString address = device.value(QStringLiteral("address")).toString();
            if (!address.isEmpty()) {
                devicePaths.insert(address, path);
            }
        }
    }

    const bool availChanged = (m_available != !adapterPath.isEmpty());
    const bool pwrChanged = (m_powered != powered);

    m_adapterPath = adapterPath;
    m_available = !adapterPath.isEmpty();
    m_powered = powered;
    m_devices = devices;
    m_devicePaths = devicePaths;

    if (availChanged) {
        Q_EMIT availableChanged();
    }
    if (pwrChanged) {
        Q_EMIT poweredChanged();
    }
    Q_EMIT devicesChanged();
}

QString BluetoothControl::devicePath(const QString &address) const
{
    return m_devicePaths.value(address);
}
/*
 * Status bar - bluetooth control
 *
 * SPDX-FileCopyrightText: 2026
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef BLUETOOTHCONTROL_H
#define BLUETOOTHCONTROL_H

#include <QDBusMessage>
#include <QDBusMetaType>
#include <QHash>
#include <QObject>
#include <QVariantList>

class QDBusInterface;

class BluetoothControl : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    Q_PROPERTY(bool powered READ powered WRITE setPowered NOTIFY poweredChanged)
    Q_PROPERTY(QVariantList devices READ devices NOTIFY devicesChanged)

public:
    explicit BluetoothControl(QObject *parent = nullptr);

    bool available() const;
    bool powered() const;
    void setPowered(bool powered);

    QVariantList devices() const;

    Q_INVOKABLE void connectDevice(const QString &address);
    Q_INVOKABLE void disconnectDevice(const QString &address);

Q_SIGNALS:
    void availableChanged();
    void poweredChanged();
    void devicesChanged();

private Q_SLOTS:
    void onPropertiesChanged(const QDBusMessage &message);
    void onInterfacesAdded(const QDBusMessage &message);
    void onInterfacesRemoved(const QDBusMessage &message);

private:
    void refresh();
    QString devicePath(const QString &address) const;

    QString m_adapterPath;
    bool m_available = false;
    bool m_powered = false;
    QVariantList m_devices;
    QHash<QString, QString> m_devicePaths;
    QDBusInterface *m_objectManager = nullptr;
};

#endif // BLUETOOTHCONTROL_H
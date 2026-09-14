/*
    Status bar - system indicators

    SPDX-FileCopyrightText: 2026
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "keyboardlayout.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMetaType>
#include <QDBusReply>
#include <QDebug>

// a(sss) -> array of (layout, variant, display name)
struct LayoutDesc
{
    QString layout;
    QString variant;
    QString displayName;
};

inline QDBusArgument &operator<<(QDBusArgument &argument, const LayoutDesc &desc)
{
    argument.beginStructure();
    argument << desc.layout << desc.variant << desc.displayName;
    argument.endStructure();
    return argument;
}

inline const QDBusArgument &operator>>(const QDBusArgument &argument, LayoutDesc &desc)
{
    argument.beginStructure();
    argument >> desc.layout >> desc.variant >> desc.displayName;
    argument.endStructure();
    return argument;
}

Q_DECLARE_METATYPE(LayoutDesc)
Q_DECLARE_METATYPE(QList<LayoutDesc>)

KeyboardLayout::KeyboardLayout(QObject *parent)
    : QObject(parent)
{
    qDBusRegisterMetaType<LayoutDesc>();
    qDBusRegisterMetaType<QList<LayoutDesc>>();

    // org.kde.keyboard is the kded KeyboardLayouts module (Plasma 6):
    //   getLayout()      -> u        (index of the active layout)
    //   getLayoutsList() -> a(sss)   (id, variant, display name)
    //   layoutChanged(u) -> signal
    //   switchToNextLayout() -> switch to the next configured layout
    m_iface = new QDBusInterface(QStringLiteral("org.kde.keyboard"),
                                 QStringLiteral("/Layouts"),
                                 QStringLiteral("org.kde.KeyboardLayouts"),
                                 QDBusConnection::sessionBus(),
                                 this);

    connect(m_iface, SIGNAL(layoutChanged(uint)),
            this, SLOT(refresh()));

    refresh();
}

QString KeyboardLayout::currentLayout() const
{
    return m_currentLayout;
}

void KeyboardLayout::nextLayout()
{
    if (m_iface && m_iface->isValid()) {
        m_iface->call(QStringLiteral("switchToNextLayout"));
    }
}

void KeyboardLayout::refresh()
{
    if (!m_iface || !m_iface->isValid()) {
        return;
    }

    // a(sss): list of (layout, variant, displayName)
    QDBusReply<QList<LayoutDesc>> reply = m_iface->call(QStringLiteral("getLayoutsList"));
    if (!reply.isValid()) {
        qWarning() << "KeyboardLayout: getLayoutsList failed:" << reply.error().message();
        return;
    }

    const QList<LayoutDesc> layouts = reply.value();
    if (layouts.isEmpty()) {
        return;
    }

    // Index of the current layout is emitted by layoutChanged(); we could also
    // query getLayout() but the signal already fires on every switch.
    QDBusReply<uint> idxReply = m_iface->call(QStringLiteral("getLayout"));
    const int index = idxReply.isValid() ? int(idxReply.value()) : 0;
    if (index < 0 || index >= layouts.size()) {
        return;
    }

    const LayoutDesc layout = layouts.at(index);
    QString label = layout.layout.toUpper();   // "us" -> "US"
    if (label.isEmpty()) {
        label = layout.displayName;            // display name fallback
    }
    if (label != m_currentLayout) {
        m_currentLayout = label;
        Q_EMIT currentLayoutChanged();
    }
}
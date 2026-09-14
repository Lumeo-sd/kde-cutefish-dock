/*
    Status bar - system indicators

    SPDX-FileCopyrightText: 2026
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

#include <QObject>

class QDBusInterface;

/**
 * Exposes the active keyboard layout (from the kded KeyboardLayouts module,
 * org.kde.keyboard /Layouts, org.kde.KeyboardLayouts) to the status bar.
 * Used by the language indicator on the right side of the panel.
 */
class KeyboardLayout : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString currentLayout READ currentLayout NOTIFY currentLayoutChanged)

public:
    explicit KeyboardLayout(QObject *parent = nullptr);

    QString currentLayout() const;

public Q_SLOTS:
    void nextLayout();

Q_SIGNALS:
    void currentLayoutChanged();

private Q_SLOTS:
    void refresh();

private:
    QDBusInterface *m_iface = nullptr;
    QString m_currentLayout;
};
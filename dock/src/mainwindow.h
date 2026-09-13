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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QQuickView>
#include <QTimer>
#include <QVariantAnimation>

#include "activity.h"
#include "docksettings.h"
#include "applicationmodel.h"
#include "trashmanager.h"

namespace LayerShellQt
{
class Window;
}

class MainWindow : public QQuickView
{
    Q_OBJECT
    Q_PROPERTY(QRect primaryGeometry READ primaryGeometry NOTIFY primaryGeometryChanged)
    Q_PROPERTY(int direction READ direction NOTIFY directionChanged)
    Q_PROPERTY(int visibility READ visibility NOTIFY visibilityChanged)
    Q_PROPERTY(int style READ style NOTIFY styleChanged)
    // Auto-hide modes shrink the panel to a thin edge strip instead of
    // unmapping it; QML fades the visuals out while dockHidden is true.
    Q_PROPERTY(bool dockHidden READ dockHidden WRITE setDockHidden NOTIFY dockHiddenChanged)
    // While an internal reorder drag is active the layer surface covers the
    // whole screen area beyond the strip ("drag band"), so the ghost icon and
    // the Unpin hint can follow the cursor anywhere. This reports the current
    // band thickness (perpendicular to the strip) so QML can size the visible
    // strip the same way regardless of the actual band.
    Q_PROPERTY(int dragBandSize READ dragBandSize NOTIFY dragBandSizeChanged)

public:
    explicit MainWindow(QQuickView *parent = nullptr);
    ~MainWindow();

    // DBus interface
    void add(const QString &desktop);
    void remove(const QString &desktop);
    bool pinned(const QString &desktop);

    // Callable from QML (drag & drop of .desktop files onto the dock).
    Q_INVOKABLE bool addDesktopFile(const QString &desktop);
    Q_INVOKABLE bool addDesktopFileAt(const QString &desktop, int index);

    // Smoothly resize the (layer-shell) surface to fit the current model row
    // count. Called when a drop slot is inserted (dock grows by one cell so
    // the icons never overflow behind the trash) or removed (dock shrinks).
    // A negative cellOffset shrinks the surface by one cell so the panel looks
    // like the dragged app already left (used while an internal reorder drag
    // is held outside the dock).
    Q_INVOKABLE void resizeToContent(int cellOffset);

    // While an internal reorder drag is active the layer surface is widened by
    // a transparent "drag band" on the non-edge side, giving the in-window
    // drag ghost and the "Unpin" hint room to follow the cursor outside the
    // visible strip. When on, every resizeToContent() keeps the band; turning
    // it off returns the surface to the plain strip geometry. The auto-hide
    // timers are also blocked during the drag so the panel cannot vanish while
    // an icon is being dragged away.
    Q_INVOKABLE void setDragBand(bool on);

    QRect primaryGeometry() const;
    int direction() const;

    int visibility() const;
    bool dockHidden() const { return m_dockHidden; }
    void setDockHidden(bool hidden);
    int dragBandSize() const { return m_dragBandSize; }

    void setDirection(int direction);
    void setIconSize(int iconSize);
    void setVisibility(int visibility);

    int style() const;
    void setStyle(int style);

    Q_INVOKABLE void updateSize();

signals:
    void resizingFished();
    void iconSizeChanged();
    void directionChanged();
    void primaryGeometryChanged();
    void visibilityChanged();
    void styleChanged();
    void dockHiddenChanged();
    void dragBandSizeChanged();

private:
    QRect windowRect(int cellOffset = 0) const;
    QRect stripRect() const;
    void resizeWindow();
    void initScreens();
    void updateLayerShell();

private slots:
    void onPrimaryScreenChanged(QScreen *screen);
    void onPositionChanged();
    void onIconSizeChanged();
    void onVisibilityChanged();

    void onHideTimeout();
    void onShrinkTimeout();

protected:
    bool eventFilter(QObject *obj, QEvent *e) override;
    void resizeEvent(QResizeEvent *) override;

private:
    Activity *m_activity;
    DockSettings *m_settings;
    ApplicationModel *m_appModel;
    TrashManager *m_trashManager;

    LayerShellQt::Window *m_layerShell;

    bool m_hideBlocked;
    bool m_dockHidden;
    // While an internal reorder drag is active: the layer surface is widened
    // by the transparent drag band on the non-edge side (resizeToContent()
    // keeps it when set) and auto-hide is suspended.
    bool m_dragBand = false;
    // Current band thickness in Qt pixels (0 when no band is armed). QML binds
    // its bandHeight/bandWidth to this so the strip math always matches the
    // real surface.
    int m_dragBandSize = 0;

    QTimer *m_showTimer;
    QTimer *m_hideTimer;
    // Delays the shrink to the edge strip until the QML fade-out finished, so
    // the layer surface is never resized while its content is still visible.
    QTimer *m_shrinkTimer;
    // Animates the surface geometry when a drop slot opens/closes.
    QVariantAnimation *m_resizeAnimation = nullptr;
};

#endif // MAINWINDOW_H

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

import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import Qt5Compat.GraphicalEffects

import Cutefish.Dock 1.0
import FishUI 1.0 as FishUI

Item {
    id: root
    visible: true

    property bool isHorizontal: Settings.direction === DockSettings.Bottom
    property real windowRadius: isHorizontal ? root.height * 0.3 : root.width * 0.3
    property bool compositing: windowHelper.compositing

    // True while an external drag (a .desktop file from a launcher) hovers the
    // dock. The app icons' own drop areas are disabled during it, so the root
    // DropArea below can follow the cursor and show the live insertion gap.
    property bool externalDragActive: false

    // Wayland auto-hide: the panel shrinks to a thin edge strip and the QML
    // layer fades out underneath the mouse cursor.
    opacity: mainWindow.dockHidden ? 0 : 1

    Behavior on opacity {
        NumberAnimation {
            duration: 200
            easing.type: Easing.OutCubic
        }
    }

    onCompositingChanged: {
        mainWindow.updateSize()
    }

    // Convert a drag position (in root coordinates, along the app axis) to a
    // row index in the ListView. Index 0 is the launcher and is protected.
    function gapIndex(pos) {
        const itemSize = isHorizontal ? appItemView.height : appItemView.width
        if (itemSize <= 0 || appItemView.count === 0)
            return 1
        const i = Math.floor(pos / itemSize)
        return Math.max(1, Math.min(i, appItemView.count))
    }

    DropArea {
        id: rootDropArea
        anchors.fill: parent
        enabled: true

        // Pin apps dragged in from a launcher (Kickoff, the Cutefish launcher,
        // a file manager) by dropping their .desktop file onto the dock.
        // While the drag hovers, a temporary drop slot is shown at the cursor
        // position so the existing icons slide apart (leaving a gap) and the
        // drop can be aimed at exactly the wanted spot.
        onEntered: function(drag) {
            if (drag.urls.length
                    && drag.urls[0].toString().toLowerCase().endsWith(".desktop")) {
                root.externalDragActive = true
                appModel.beginDropSlot(root.gapIndex(isHorizontal ? drag.x : drag.y))
            }
        }

        onPositionChanged: function(drag) {
            if (root.externalDragActive)
                appModel.moveDropSlot(root.gapIndex(isHorizontal ? drag.x : drag.y))
        }

        onExited: function(drag) {
            if (root.externalDragActive) {
                root.externalDragActive = false
                appModel.endDropSlot()
            }
        }

        onDropped: function(drop) {
            root.externalDragActive = false

            if (drop.hasUrls) {
                const idx = root.gapIndex(isHorizontal ? drop.x : drop.y)
                let first = true

                for (let i = 0; i < drop.urls.length; ++i) {
                    const url = drop.urls[i].toString()
                    if (!url.toLowerCase().endsWith(".desktop"))
                        continue

                    if (first) {
                        mainWindow.addDesktopFileAt(url, idx)
                        first = false
                    } else {
                        mainWindow.addDesktopFile(url)
                    }
                }
            }

            // No-op if the drop slot was already replaced by the inserted pin.
            appModel.endDropSlot()
        }
    }

    // Background
    Rectangle {
        id: _background

        property var borderColor: root.compositing ? FishUI.Theme.darkMode ? Qt.rgba(255, 255, 255, 0.3)
                                                                           : Qt.rgba(0, 0, 0, 0.2) : FishUI.Theme.darkMode ? Qt.rgba(255, 255, 255, 0.15)
                                                                                                                           : Qt.rgba(0, 0, 0, 0.15)

        anchors.fill: parent
        radius: root.compositing && Settings.style === 0 ? windowRadius : 0
        color: FishUI.Theme.darkMode ? "#666666" : "#E6E6E6"
        opacity: root.compositing ? FishUI.Theme.darkMode ? 0.5 : 0.5 : 0.9
        border.width: 1 / FishUI.Units.devicePixelRatio
        border.pixelAligned: FishUI.Units.devicePixelRatio > 1 ? false : true
        border.color: borderColor

        Behavior on color {
            ColorAnimation {
                duration: 200
                easing.type: Easing.Linear
            }
        }
    }

    FishUI.WindowHelper {
        id: windowHelper
    }

    FishUI.WindowShadow {
        view: mainWindow
        geometry: Qt.rect(root.x, root.y, root.width, root.height)
        strength: 1
        radius: _background.radius
    }

    FishUI.WindowBlur {
        view: mainWindow
        geometry: Qt.rect(root.x, root.y, root.width, root.height)
        windowRadius: _background.radius
        enabled: true
    }

    FishUI.PopupTips {
        id: popupTips
        backgroundColor: _background.color
        blurEnabled: false
    }

    GridLayout {
        id: mainLayout
        anchors.fill: parent
        anchors.topMargin: Settings.style === 1
                           && (Settings.direction === 0 || Settings.direction === 2)
                           ? 28 : 0
        flow: isHorizontal ? Grid.LeftToRight : Grid.TopToBottom
        columnSpacing: 0
        rowSpacing: 0

        ListView {
            id: appItemView
            orientation: isHorizontal ? Qt.Horizontal : Qt.Vertical
            snapMode: ListView.SnapToItem
            interactive: false
            model: appModel
            clip: true

            Layout.fillHeight: true
            Layout.fillWidth: true

            delegate: AppItem {
                id: appItemDelegate
                implicitWidth: isHorizontal ? appItemView.height : appItemView.width
                implicitHeight: isHorizontal ? appItemView.height : appItemView.width
            }

            moveDisplaced: Transition {
                NumberAnimation {
                    properties: "x, y"
                    duration: 300
                    easing.type: Easing.InOutQuad
                }
            }
        }

        DockItem {
            id: trashItem
            implicitWidth: isHorizontal ? root.height : root.width
            implicitHeight: isHorizontal ? root.height : root.width
            popupText: qsTr("Trash")
            enableActivateDot: false
            iconName: trash.count === 0 ? "user-trash-empty" : "user-trash-full"
            onClicked: trash.openTrash()
            onRightClicked: trashMenu.popup()

            dropArea.enabled: !root.externalDragActive

            onDropped: {
                if (drop.hasUrls) {
                    trash.moveToTrash(drop.urls)
                }
            }

            Rectangle {
                anchors.fill: parent
                anchors.margins: FishUI.Units.smallSpacing / 2
                color: "transparent"
                border.color: FishUI.Theme.textColor
                radius: height * 0.3
                border.width: 1 / FishUI.Units.devicePixelRatio
                border.pixelAligned: FishUI.Units.devicePixelRatio > 1 ? false : true
                opacity: trashItem.dropArea.containsDrag ? 0.5 : 0

                Behavior on opacity {
                    NumberAnimation {
                        duration: 200
                    }
                }
            }

            FishUI.DesktopMenu {
                id: trashMenu

                MenuItem {
                    text: qsTr("Open")
                    onTriggered: trash.openTrash()
                }

                MenuItem {
                    text: qsTr("Empty Trash")
                    onTriggered: trash.emptyTrash()
                    visible: trash.count !== 0
                }
            }
        }
    }

    Connections {
        target: Settings

        function onDirectionChanged() {
            popupTips.hide()
        }
    }

    Connections {
        target: mainWindow

        function onVisibleChanged() {
            popupTips.hide()
        }
    }
}

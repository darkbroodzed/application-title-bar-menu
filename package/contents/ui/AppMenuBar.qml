/*
 * SPDX-FileCopyrightText: 2024 Anton Kharuzhy <publicantroids@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Layouts
import org.kde.plasma.components as PlasmaComponents
import org.kde.applicationtitlebar 1.0 as ATB

/**
 * AppMenuBar renders the active window's application menu as a horizontal row
 * of ToolButtons inside the widget.  It is loaded as a WidgetElement of type
 * AppMenu and sits next to the windowTitle element.
 *
 * Visibility is driven by root.titleHoveredForMenu (the sticky hover latch set
 * by the windowTitle component) combined with the windowTitleHideOnHover
 * config flag.  When menuVisible becomes true the row animates open and the
 * title simultaneously collapses; when the mouse leaves the widget both
 * animations reverse.
 */
Item {
    id: appMenuBar

    // Required by the WidgetElement loader infrastructure.
    property var modelData

    // True while the app-menu row should be shown (animated).
    readonly property bool menuVisible: plasmoid.configuration.windowTitleHideOnHover
                                        && root.titleHoveredForMenu

    Layout.fillHeight: true
    Layout.fillWidth: false
    // Grow to the natural width of the button row when visible, collapse to 0
    // when hidden.  Both transitions are animated via the Behaviors below.
    Layout.preferredWidth: menuVisible ? menuRow.implicitWidth : 0
    Layout.maximumWidth: menuVisible ? 99999 : 0
    clip: true

    Behavior on Layout.preferredWidth {
        NumberAnimation {
            duration: 200
            easing.type: Easing.InOutQuad
        }
    }
    Behavior on Layout.maximumWidth {
        NumberAnimation {
            duration: 200
            easing.type: Easing.InOutQuad
        }
    }

    // ── AppMenuModel ──────────────────────────────────────────────────────────
    // The service name and object path are read from the existing ActiveWindow
    // object so we don't need a separate TasksModel.

    ATB.AppMenuModel {
        id: appMenuModel
        serviceName: tasksModel.activeWindow.appMenuServiceName
        menuObjectPath: tasksModel.activeWindow.appMenuObjectPath

        // Keyboard left/right navigation: activate the requested button.
        onRequestActivateIndex: function (idx) {
            const btn = menuRow.itemAtIndex(idx)
            if (btn) {
                appMenuModel.trigger(btn, idx)
            }
        }
    }

    // ── Button row ────────────────────────────────────────────────────────────

    RowLayout {
        id: menuRow

        spacing: 0
        anchors.fill: parent

        Repeater {
            model: appMenuModel.menuAvailable ? appMenuModel : null

            delegate: PlasmaComponents.ToolButton {
                required property int index
                required property string activeMenu
                required property var activeActions

                Layout.fillHeight: true
                text: activeMenu
                // Disable the button when the top-level QAction is disabled.
                enabled: activeActions?.enabled ?? false
                // Separators have an empty text; hide them entirely.
                visible: text !== ""
                // Match the window title font so the bar feels integrated.
                font.pointSize: plasmoid.configuration.windowTitleFontSize
                font.bold: plasmoid.configuration.windowTitleFontBold

                onClicked: appMenuModel.trigger(this, index)
            }
        }
    }
}

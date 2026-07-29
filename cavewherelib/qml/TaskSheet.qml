/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

// The phone's answer to the sidebar's task flyout: a sheet across the bottom of
// the view listing every tracked job, opened by the top bar's status chip.
//
// It is a sheet rather than a hinged card because there is nothing to hinge it
// to — below Medium there is no sidebar — and because a phone-width card off to
// one side would be most of the window anyway.
//
// It carries Automatic Update as well as the list, and the hamburger menu keeps
// its own copy: both write updateCoordinator.automaticUpdate, so there is one
// value with two ways to reach it. This is where you already are when you are
// thinking about background work; the menu is where you look when you are not.
//
// Opening is a latch rather than a bound state so that a tap closes it and it
// stays closed, and it falls away on its own once the last job finishes, since
// an empty list has nothing to say.
QQ.Item {
    id: sheetId

    // QtObject rather than TaskFutureCombineModel so a test can stand in a
    // ListModel; both carry the count and roles this sheet reads.
    property QQ.QtObject model: ActiveTasks.model
    property bool automaticUpdate: false

    property bool opened: false

    readonly property int taskCount: sheetId.model ? sheetId.model.count : 0
    readonly property bool shown: sheetId.opened && sheetId.taskCount > 0

    // Past this the list scrolls. The sheet spans the window, so it can afford a
    // taller list than the card hinged beside the sidebar.
    readonly property int maxListHeight: 320

    signal automaticUpdateToggled(bool enabled)

    visible: sheetId.shown

    // The chip that opens this is a toggle, and there is nothing to open onto
    // with no jobs to list.
    function toggle() {
        if (sheetId.opened) {
            sheetId.opened = false
        } else if (sheetId.taskCount > 0) {
            sheetId.opened = true
        }
    }

    function close() {
        sheetId.opened = false
    }

    // The work it was opened for is finished, so drop the latch rather than
    // leaving it armed for an unrelated job minutes later.
    onTaskCountChanged: {
        if (sheetId.taskCount === 0) {
            sheetId.opened = false
        }
    }

    // Dims the view behind the sheet and takes the taps that land outside it,
    // which is the phone's usual way of closing something like this.
    //
    // It stops at the card's top edge rather than filling the sheet, so a tap on
    // the card cannot reach it. A blocking handler on the card would not do: the
    // card's default property is its body, so a handler written there never
    // covers the header, and a TapHandler's default DragThreshold policy takes
    // only a passive grab, which does not stop delivery either way. The card is
    // opaque, so ending the scrim under it dims nothing less.
    QQ.Rectangle {
        objectName: "taskSheetScrim"
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: cardId.top

        color: Theme.shadow

        QQ.TapHandler {
            onTapped: sheetId.close()
        }
    }

    FlyoutCard {
        id: cardId
        objectName: "taskSheetCard"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        title: sheetId.taskCount === 1
               ? qsTr("1 Task")
               : qsTr("%1 Tasks").arg(sheetId.taskCount)
        iconSource: "qrc:/twbs-icons/icons/list.svg"

        onCloseRequested: sheetId.close()

        TaskListView {
            id: taskListId
            objectName: "taskSheetList"
            Layout.fillWidth: true
            Layout.margins: Theme.toolFlyoutPadding
            Layout.preferredHeight: Math.min(taskListId.contentHeight,
                                             sheetId.maxListHeight)

            model: sheetId.model
            clip: true
        }

        QQ.Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.border
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: Theme.toolFlyoutPadding
            spacing: Theme.flowSpacing

            QC.Switch {
                objectName: "taskSheetAutoUpdateSwitch"
                checked: sheetId.automaticUpdate

                onToggled: sheetId.automaticUpdateToggled(checked)
            }

            QC.Label {
                Layout.fillWidth: true
                text: qsTr("Automatic Update")
                color: Theme.text
                font.pixelSize: Theme.fontSizeSmall
            }
        }
    }
}

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

// What the sidebar's busy footer is busy with: every tracked job, by name and
// progress, in a card off the sidebar's bottom-right corner. Opened by tapping
// the footer's running row.
//
// It lists the whole ActiveTasks model, not just the three pipelines the update
// coordinator drives — a point-cloud load, a picking BVH rebuild, an import or a
// save all reach the future manager and nothing else, and this is the only place
// in the main window that names them since the sidebar's task list was folded
// into the footer.
//
// It opens two ways. Hovering the control that owns it peeks it open, and it
// closes again when the pointer leaves; clicking pins it, and it stays until
// something closes it. Hovering the card itself counts as hovering, and a short
// close delay covers the gap between the sidebar and the card — without both,
// the flyout would vanish while the pointer was on its way over.
//
// Closing therefore has to do more than clear the pin, and that is why the pin
// lives here rather than with the host: at the moment the user presses × or
// clicks to unpin, the pointer performing that click is itself one of the terms
// holding the card open, so clearing the pin alone changes nothing on screen.
// dismiss() drops the pin and suppresses the hover terms together.
//
// It falls away on its own once the last job finishes, since an empty list has
// nothing to say, and drops the pin with it so the next unrelated job doesn't
// bring it back unasked.
QQ.Item {
    id: flyoutId

    property bool hostVisible: true
    // Input: the control that opens this flyout is hovered.
    property bool previewHovered: false
    // QtObject rather than TaskFutureCombineModel so a test can stand in a
    // ListModel; both carry the count and roles this card reads.
    property QQ.QtObject model: ActiveTasks.model

    // Pinned open by a click. Driven through togglePin()/dismiss() rather than
    // assigned from outside, because closing has to suppress the hover terms in
    // the same breath — see `dismissed`.
    property bool pinned: false

    // Suppresses the hover terms after an explicit close, until the pointer has
    // left everything that holds the card open. Without it both exits are inert:
    // the very pointer that presses × or clicks to unpin is itself keeping the
    // card open, so it would linger until the pointer wandered off — exactly
    // what would have happened had the user never pressed anything.
    property bool dismissed: false

    // wantsOpen going false only *starts* the close; the delay is what lets the
    // pointer cross the gap. Latched rather than bound for that reason.
    property bool openLatched: false

    readonly property int taskCount: flyoutId.model ? flyoutId.model.count : 0

    // Everything that would hold the card open, before the dismissal latch.
    readonly property bool openInputs: flyoutId.pinned
                                       || flyoutId.previewHovered
                                       || panelHoverId.hovered

    readonly property bool wantsOpen: flyoutId.openInputs && !flyoutId.dismissed

    readonly property bool shown: flyoutId.openLatched && flyoutId.taskCount > 0

    implicitWidth: Theme.taskFlyoutWidth
    implicitHeight: panelId.height
    width: implicitWidth
    height: implicitHeight
    visible: flyoutId.hostVisible && flyoutId.shown

    // A click on the control that owns the flyout. Pins it open, or closes it
    // when it is already pinned. Nothing to pin open with no jobs to list.
    function togglePin() {
        if (flyoutId.pinned) {
            flyoutId.dismiss()
        } else if (flyoutId.taskCount > 0) {
            flyoutId.pinned = true
        }
    }

    // Close now, rather than waiting out the close delay meant for the gap.
    function dismiss() {
        closeDelayId.stop()
        flyoutId.pinned = false
        flyoutId.dismissed = true
        flyoutId.openLatched = false
    }

    onWantsOpenChanged: {
        if (flyoutId.wantsOpen) {
            closeDelayId.stop()
            flyoutId.openLatched = true
        } else {
            closeDelayId.restart()
        }
    }

    onOpenInputsChanged: {
        // The pointer has left everything, so a later hover peeks it open again.
        if (!flyoutId.openInputs) {
            flyoutId.dismissed = false
        }
    }

    onTaskCountChanged: {
        // The work it was pinned open for is finished. Drop the pin, so an
        // unrelated job minutes later doesn't put the card back over the view
        // with the pointer nowhere near it.
        if (flyoutId.taskCount === 0) {
            flyoutId.pinned = false

            // With nothing listed the card is off screen anyway, so the close
            // delay has no gap left to cover — drop the latch now rather than
            // leaving it armed for a job that arrives inside the delay. Still
            // held while something wants it open, so a job finishing and another
            // starting under the pointer reads as one continuous run.
            if (!flyoutId.wantsOpen) {
                closeDelayId.stop()
                flyoutId.openLatched = false
            }
        }
    }

    QQ.Timer {
        id: closeDelayId
        interval: Theme.taskFlyoutHoverCloseDelay

        onTriggered: flyoutId.openLatched = false
    }

    QQ.Rectangle {
        id: panelId
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        height: contentLayoutId.implicitHeight
        radius: Theme.floatingWidgetRadius
        color: Theme.surfaceRaised
        border.width: 1
        border.color: Theme.border

        // Keeps the card open while the pointer is on it, so a hover-peeked
        // flyout can be read, scrolled, and pinned.
        QQ.HoverHandler {
            id: panelHoverId
        }

        ColumnLayout {
            id: contentLayoutId
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            spacing: 0

            QQ.Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: headerRowId.implicitHeight + Theme.toolFlyoutPadding
                color: Theme.surface

                RowLayout {
                    id: headerRowId
                    anchors.fill: parent
                    anchors.leftMargin: Theme.toolFlyoutPadding
                    anchors.rightMargin: Theme.tightSpacing
                    spacing: Theme.flowSpacing

                    QC.Label {
                        objectName: "taskFlyoutTitle"
                        Layout.fillWidth: true
                        text: flyoutId.taskCount === 1
                              ? qsTr("1 Task")
                              : qsTr("%1 Tasks").arg(flyoutId.taskCount)
                        color: Theme.text
                        font.bold: true
                        elide: QC.Label.ElideRight
                    }

                    QC.ToolButton {
                        objectName: "taskFlyoutCloseButton"
                        text: "×"
                        font.pixelSize: Theme.fontSizeXLarge

                        onClicked: flyoutId.dismiss()
                    }
                }
            }

            QQ.Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Theme.border
            }

            TaskListView {
                id: taskListId
                objectName: "taskFlyoutList"
                Layout.fillWidth: true
                Layout.margins: Theme.toolFlyoutPadding
                // Grows with the list until it would outgrow the card, then
                // scrolls. contentHeight is the laid-out total, so a couple of
                // wrapped job names lift the card rather than clipping them.
                Layout.preferredHeight: Math.min(taskListId.contentHeight,
                                                 Theme.taskFlyoutMaxListHeight)

                model: flyoutId.model
                clip: true
            }
        }
    }
}

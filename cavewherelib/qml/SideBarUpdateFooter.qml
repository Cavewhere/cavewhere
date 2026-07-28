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

// The sidebar's bottom footer, and the sidebar's only report of background
// work: the automatic-update toggle when nothing is happening, "Update needed ·
// Run" when derived data is stale, and a busy row while anything is in flight.
//
// Busy is read before stale, and it covers more than the update coordinator.
// The coordinator only knows about the three derived-data pipelines (line plot,
// scraps, LiDAR); a point-cloud load, a picking BVH rebuild, an import or a save
// register with the future manager and nothing else. Reading only the
// coordinator would leave the footer showing "Automatic Update" through all of
// them — so any tracked job puts the footer in the busy state. Both that state
// and the count in the label default to the same shared source, which is what
// keeps them talking about the same thing.
//
// Busy and the task count both default to the shared ActiveTasks state and both
// stay overridable, so the footer is a view of that state and a test can drive
// it directly. A job whose future is already finished is never admitted, and a
// cascade is briefly running between pipelines with no future yet — so the
// count can be zero while work really is in flight, and the label says
// "Running…" rather than "Running 0 Tasks".
//
// The busy row is a button into the task flyout, which lists what those jobs
// actually are; the host owns the flyout, because it has to composite above the
// page view and a child of the sidebar cannot.
QQ.Rectangle {
    id: footerId

    required property bool needsUpdate
    required property bool automaticUpdate
    property bool compact: false
    property bool busy: ActiveTasks.busy
    property int taskCount: ActiveTasks.count

    // Output: the busy row is under the pointer, so the host can peek the task
    // flyout open. Written by the running row's HoverHandler rather than aliased,
    // because that handler lives inside a Loader component.
    property bool busyRowHovered: false

    // Input: whether the task flyout the busy row opens is currently on screen,
    // so the row's chevron can point at it and light up.
    property bool tasksShown: false

    signal runRequested()
    signal automaticUpdateToggled(bool enabled)
    signal tasksRequested()

    implicitHeight: contentLoaderId.implicitHeight + Theme.updateFooterPadding * 2

    color: Theme.sidebar.panel

    // The row is destroyed the moment work finishes, taking its HoverHandler with
    // it, so leaving busy never reports a hover-out. Clear it here instead.
    onBusyChanged: {
        if (!footerId.busy) {
            footerId.busyRowHovered = false
        }
    }

    QQ.Loader {
        id: contentLoaderId
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: Theme.updateFooterPadding

        sourceComponent: {
            if (footerId.busy) {
                return runningComponent
            }
            if (footerId.needsUpdate) {
                return pendingComponent
            }
            return idleComponent
        }
    }

    QQ.Component {
        id: idleComponent

        ColumnLayout {
            spacing: Theme.updateFooterSpacing

            QC.Label {
                objectName: "autoUpdateLabel"
                visible: !footerId.compact
                text: qsTr("Automatic\nUpdate")
                horizontalAlignment: QC.Label.AlignHCenter
                font.pixelSize: Theme.fontSizeCaption
                Layout.fillWidth: true
            }

            QC.CheckBox {
                objectName: "autoUpdateCheckbox"
                visible: !footerId.compact
                checked: footerId.automaticUpdate
                Layout.alignment: Qt.AlignHCenter

                onToggled: footerId.automaticUpdateToggled(checked)
            }

            RoundButton {
                id: autoUpdateToggleId
                objectName: "autoUpdateToggle"
                visible: footerId.compact
                checkable: true
                checked: footerId.automaticUpdate
                icon.source: "qrc:/twbs-icons/icons/arrow-repeat.svg"
                icon.color: checked ? Theme.accent : Theme.text
                Layout.alignment: Qt.AlignHCenter

                QC.ToolTip.visible: autoUpdateToggleId.hovered
                QC.ToolTip.text: autoUpdateToggleId.checked
                                 ? qsTr("Automatic updates on")
                                 : qsTr("Automatic updates off")

                onToggled: footerId.automaticUpdateToggled(checked)
            }
        }
    }

    QQ.Component {
        id: pendingComponent

        QQ.Rectangle {
            implicitHeight: pendingColumnId.implicitHeight + Theme.updateFooterPadding * 2

            radius: Theme.floatingWidgetRadius
            color: Theme.warningSurface
            border.width: 1
            border.color: Theme.warningBorder

            ColumnLayout {
                id: pendingColumnId
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: Theme.updateFooterPadding
                spacing: Theme.updateFooterSpacing

                QC.Label {
                    objectName: "updateNeededLabel"
                    visible: !footerId.compact
                    text: qsTr("Update needed")
                    color: Theme.warningText
                    font.pixelSize: Theme.fontSizeCaption
                    font.bold: true
                    horizontalAlignment: QC.Label.AlignHCenter
                    wrapMode: QC.Label.WordWrap
                    Layout.fillWidth: true
                }

                QC.Button {
                    id: runButtonId
                    objectName: "updateRunButton"
                    text: qsTr("Run")
                    font.pixelSize: Theme.fontSizeCaption
                    Layout.fillWidth: true

                    QC.ToolTip.visible: runButtonId.hovered && footerId.compact
                    QC.ToolTip.text: qsTr("Update needed — run now")

                    onClicked: footerId.runRequested()
                }
            }
        }
    }

    QQ.Component {
        id: runningComponent

        ColumnLayout {
            spacing: Theme.updateFooterSpacing

            // No tooltip: hovering already opens the task flyout, which says far
            // more than a tooltip would, and both at once is just noise.
            QQ.HoverHandler {
                id: runningHoverId
                cursorShape: Qt.PointingHandCursor

                onHoveredChanged: footerId.busyRowHovered = runningHoverId.hovered
            }

            QQ.TapHandler {
                objectName: "updateRunningTapHandler"
                onTapped: footerId.tasksRequested()
            }

            RowLayout {
                spacing: Theme.tightSpacing
                Layout.alignment: Qt.AlignHCenter

                QC.BusyIndicator {
                    objectName: "updateRunningIndicator"
                    implicitWidth: Theme.iconSizeSmall
                    implicitHeight: Theme.iconSizeSmall
                }

                // Points at the flyout it opens, and lights up while that flyout
                // is on screen — the row's only cue that there is more to see.
                // Hidden with no jobs listed, since the flyout has nothing to
                // show then and the chevron would promise something that can't
                // happen; the footer still reads as busy on the coordinator
                // alone, which is the state that has a zero count.
                Icon {
                    objectName: "updateRunningExpandIcon"
                    visible: footerId.taskCount > 0
                    source: "qrc:/twbs-icons/icons/chevron-right.svg"
                    sourceSize: Qt.size(Theme.updateFooterChevronSize,
                                        Theme.updateFooterChevronSize)
                    colorizationColor: footerId.tasksShown ? Theme.accent : Theme.icon
                }
            }

            QC.Label {
                objectName: "updateRunningLabel"
                visible: !footerId.compact
                text: {
                    if (footerId.taskCount <= 0) {
                        return qsTr("Running…")
                    }
                    if (footerId.taskCount === 1) {
                        return qsTr("Running 1 Task")
                    }
                    return qsTr("Running %1 Tasks").arg(footerId.taskCount)
                }
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSizeCaption
                horizontalAlignment: QC.Label.AlignHCenter
                wrapMode: QC.Label.WordWrap
                Layout.fillWidth: true
            }
        }
    }
}

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
// Which of the three it shows is not this component's call — ActiveTasks ranks
// them, because the top bar chip shows the same three at widths where there is
// no sidebar. The activity, the task count and the progress aggregate all
// default to that shared state and all stay overridable, so the footer is a view
// of it and a test can drive it directly. The Automatic Update setting is read
// and written straight on the update coordinator instead of being injected —
// it is a real persisted setting, so a test has no need of a way in.
//
// A job whose future is already finished is never admitted, and a cascade is
// briefly running between pipelines with no future yet — so the count can be
// zero while work really is in flight, and the label says "Running…" rather than
// "Running 0 Tasks".
//
// The busy row is a button into the task flyout, which lists what those jobs
// actually are; the host owns the flyout, because it has to composite above the
// page view and a child of the sidebar cannot.
QQ.Rectangle {
    id: footerId

    property bool compact: false
    property int activity: ActiveTasks.activity
    property int taskCount: ActiveTasks.count
    property real progress: ActiveTasks.progress

    readonly property bool busy: footerId.activity === ActiveTasks.Activity.Busy

    // Output: the busy row is under the pointer, so the host can peek the task
    // flyout open. Written by the running row's HoverHandler rather than aliased,
    // because that handler lives inside a Loader component.
    property bool busyRowHovered: false

    // Input: whether the task flyout the busy row opens is currently on screen,
    // so the row's chevron can point at it and light up.
    property bool tasksShown: false

    signal runRequested()
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
            switch (footerId.activity) {
            case ActiveTasks.Activity.Busy:
                return runningComponent
            case ActiveTasks.Activity.Stale:
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
                id: autoUpdateCheckboxId
                objectName: "autoUpdateCheckbox"
                visible: !footerId.compact
                Layout.alignment: Qt.AlignHCenter

                onToggled: RootData.updateCoordinator.automaticUpdate = checked

                QQ.Binding {
                    target: autoUpdateCheckboxId
                    property: "checked"
                    value: RootData.updateCoordinator.automaticUpdate
                }
            }

            RoundButton {
                id: autoUpdateToggleId
                objectName: "autoUpdateToggle"
                visible: footerId.compact
                checkable: true
                icon.source: "qrc:/twbs-icons/icons/arrow-repeat.svg"
                icon.color: checked ? Theme.accent : Theme.text
                Layout.alignment: Qt.AlignHCenter

                QC.ToolTip.visible: autoUpdateToggleId.hovered
                QC.ToolTip.text: autoUpdateToggleId.checked
                                 ? qsTr("Automatic updates on")
                                 : qsTr("Automatic updates off")

                onToggled: RootData.updateCoordinator.automaticUpdate = checked

                QQ.Binding {
                    target: autoUpdateToggleId
                    property: "checked"
                    value: RootData.updateCoordinator.automaticUpdate
                }
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

                // Compact hides the label below, so the count moves inside the
                // ring — the only place left that can carry it at 50px.
                TaskProgressRing {
                    objectName: "updateRunningIndicator"
                    progress: footerId.progress
                    count: footerId.taskCount
                    showCount: footerId.compact
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

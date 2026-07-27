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
// them — so any tracked job puts the footer in the busy state, which also keeps
// the state and the count in the label talking about the same thing.
//
// The coordinator's state is injected rather than read here, so the footer is a
// view of it and can be driven directly. The task count is the exception: only
// the task/future models can supply it. A job whose future is already finished
// is never admitted, and a cascade is briefly running between pipelines with no
// future yet — so the count can be zero while work really is in flight, and the
// label says "Running…" rather than "Running 0 Tasks".
QQ.Rectangle {
    id: footerId

    required property bool running
    required property bool needsUpdate
    required property bool automaticUpdate
    property bool compact: false
    property int taskCount: taskModelId.count

    readonly property bool busy: footerId.running || footerId.taskCount > 0

    signal runRequested()
    signal automaticUpdateToggled(bool enabled)

    implicitHeight: contentLoaderId.implicitHeight + Theme.updateFooterPadding * 2

    color: Theme.sidebar.panel

    TaskFutureCombineModel {
        id: taskModelId
        models: [RootData.taskManagerModel, RootData.futureManagerModel]
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

            QC.BusyIndicator {
                objectName: "updateRunningIndicator"
                implicitWidth: Theme.iconSizeSmall
                implicitHeight: Theme.iconSizeSmall
                Layout.alignment: Qt.AlignHCenter
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

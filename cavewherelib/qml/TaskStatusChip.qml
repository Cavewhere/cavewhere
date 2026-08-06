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

// What the sidebar footer says, in the top bar, for the widths that have no
// sidebar. Below Medium MainSideBar does not exist, and with it went the app's
// only report of background work — so a save, a sync, a point-cloud load or an
// import ran on a phone with nothing on screen at all.
//
// It shows the same three states the footer does, ranked by the same
// ActiveTasks.activity, and wears the same ring: busy is the ring plus the count
// beside it (the chip's ring is too small to hold two digits inside), stale is
// an amber Update button, and idle is nothing at all — the hamburger menu
// already carries Automatic Update, so there is nothing for this to say when
// nothing is happening.
//
// The states are loaded rather than merely hidden: the ring is a QQuickRhiItem,
// and one kept alive behind a hidden chip is a render target for a mark nobody
// can see.
QQ.Item {
    id: chipId

    property int activity: ActiveTasks.activity
    property int taskCount: ActiveTasks.count
    property real progress: ActiveTasks.progress

    // Whether the width this chip belongs to is the one on screen. Kept apart
    // from the idle state so the host says where it lives and the chip says
    // whether it has anything to report.
    property bool hostVisible: true

    // Smaller than the footer's ring: the top bar's row is shorter than the
    // sidebar's. The padding is the amber Update chip's alone.
    readonly property int ringSize: 14
    readonly property int pendingPadding: 5

    signal tasksRequested()
    signal runRequested()

    implicitWidth: contentLoaderId.implicitWidth
    implicitHeight: contentLoaderId.implicitHeight

    visible: chipId.hostVisible && chipId.activity !== ActiveTasks.Activity.Idle

    QQ.Loader {
        id: contentLoaderId
        anchors.centerIn: parent

        sourceComponent: {
            if (!chipId.hostVisible) {
                return null
            }
            switch (chipId.activity) {
            case ActiveTasks.Activity.Busy:
                return runningComponent
            case ActiveTasks.Activity.Stale:
                return pendingComponent
            }
            return null
        }
    }

    QQ.Component {
        id: runningComponent

        RowLayout {
            spacing: Theme.tightSpacing

            // A cascade between pipelines is busy with nothing registered yet,
            // and there is no list to open onto until something is. The sidebar
            // footer drops its chevron in the same state.
            QQ.TapHandler {
                objectName: "chipRunningTapHandler"
                enabled: chipId.taskCount > 0

                onTapped: chipId.tasksRequested()
            }

            TaskProgressRing {
                objectName: "chipRunningIndicator"
                implicitWidth: chipId.ringSize
                implicitHeight: chipId.ringSize

                progress: chipId.progress
            }

            // A cascade between pipelines is running with nothing registered
            // yet, and "0" would read as no work rather than as work being
            // counted.
            QC.Label {
                objectName: "chipRunningCount"
                visible: chipId.taskCount > 0
                text: chipId.taskCount
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSizeCaption
            }
        }
    }

    QQ.Component {
        id: pendingComponent

        QQ.Rectangle {
            implicitWidth: pendingRowId.implicitWidth + chipId.pendingPadding * 2
            implicitHeight: pendingRowId.implicitHeight + chipId.pendingPadding

            radius: Theme.floatingWidgetRadius
            color: Theme.warningSurface
            border.width: 1
            border.color: Theme.warningBorder

            QQ.TapHandler {
                objectName: "chipPendingTapHandler"
                onTapped: chipId.runRequested()
            }

            RowLayout {
                id: pendingRowId
                anchors.centerIn: parent
                spacing: Theme.tightSpacing

                Icon {
                    source: "qrc:/twbs-icons/icons/exclamation-triangle.svg"
                    sourceSize: Qt.size(Theme.updateFooterChevronSize,
                                        Theme.updateFooterChevronSize)
                    colorizationColor: Theme.warningText
                }

                QC.Label {
                    objectName: "chipPendingLabel"
                    text: qsTr("Update")
                    color: Theme.warningText
                    font.pixelSize: Theme.fontSizeCaption
                    font.bold: true
                }
            }
        }
    }
}

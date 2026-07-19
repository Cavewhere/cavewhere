import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

StandardPage {
    id: root
    objectName: "cavernOutputPage"

    readonly property LinePlotManager linePlotManager: RootData.linePlotManager
    readonly property ExternalCenterlineManager externalManager: RootData.externalCenterlineManager
    readonly property bool hasError: linePlotManager !== null
                                     && linePlotManager.hasSolveError
    readonly property string cavernLog: linePlotManager !== null
                                        ? linePlotManager.cavernLog : ""
    readonly property string loopClosureStats: linePlotManager !== null
                                               ? linePlotManager.loopClosureStats : ""
    readonly property string solveErrorMessage: linePlotManager !== null
                                                ? linePlotManager.solveErrorMessage : ""

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.pageMargin
        spacing: Theme.sectionSpacing

        // Header: status on the left, primary action after it. A Flow so
        // the Solve button wraps under the status on narrow windows
        // instead of being cut off.
        QQ.Flow {
            Layout.fillWidth: true
            spacing: Theme.sectionSpacing

            QC.Label {
                objectName: "statusLabel"
                // Cap at the Flow's width so long status text word-wraps
                // instead of forcing the row wider than the page.
                width: Math.min(implicitWidth, parent.width)
                font.pixelSize: Theme.fontSizeMedium
                color: root.hasError ? Theme.danger : Theme.text
                wrapMode: QC.Label.WordWrap
                text: {
                    if (root.hasError) {
                        return qsTr("Cavern reported an error during the last solve.")
                    }
                    if (root.linePlotManager !== null
                        && root.linePlotManager.lastSolveDuration >= 0) {
                        return qsTr("Last solve completed successfully in %1 s — %2 stations, %3 warnings.")
                            .arg(root.linePlotManager.lastSolveDuration.toFixed(1))
                            .arg(root.linePlotManager.lastSolveStationCount)
                            .arg(root.linePlotManager.lastSolveWarningCount)
                    }
                    return qsTr("Last solve completed successfully.")
                }
            }

            QC.Button {
                objectName: "rerunSolveButton"
                enabled: root.linePlotManager !== null
                text: qsTr("Solve")
                onClicked: root.linePlotManager.rerunSurvex()
            }
        }

        QC.Label {
            objectName: "errorMessageLabel"
            Layout.fillWidth: true
            visible: root.hasError
            wrapMode: QC.Label.WordWrap
            text: root.solveErrorMessage
        }

        QQ.Rectangle {
            objectName: "missingSourceBanner"
            Layout.fillWidth: true
            implicitHeight: missingSourceLabel.implicitHeight + Theme.sectionSpacing * 2
            visible: root.externalManager !== null
                     && root.externalManager.missingSourceOwners.length > 0
            color: Theme.warning
            radius: 5

            QC.Label {
                id: missingSourceLabel
                anchors.fill: parent
                anchors.margins: Theme.sectionSpacing
                wrapMode: QC.Label.WordWrap
                text: qsTr("Some external centerline source files can no longer be found on disk. "
                           + "The copies inside the project are still used for solving.")
            }
        }

        ColumnLayout {
            objectName: "attachedCenterlinesSection"
            Layout.fillWidth: true
            visible: attachedRepeater.count > 0
            spacing: Theme.tightSpacing

            QC.Label {
                text: qsTr("Attached external centerlines")
                font.bold: true
            }

            QQ.Repeater {
                id: attachedRepeater
                model: root.externalManager !== null
                       ? root.externalManager.attachedCenterlinesModel : null

                delegate: RowLayout {
                    id: attachedRowId
                    objectName: "attachedCenterlineRow" + index

                    required property int index
                    required property string ownerName
                    required property string ownerKind
                    required property string entryFile
                    required property int depCount
                    required property int warningCount

                    Layout.fillWidth: true
                    spacing: Theme.sectionSpacing

                    QC.Label {
                        objectName: "attachedRowLabel"
                        Layout.fillWidth: true
                        elide: QC.Label.ElideMiddle
                        text: qsTr("%1 %2 — %3")
                              .arg(attachedRowId.ownerKind)
                              .arg(attachedRowId.ownerName)
                              .arg(attachedRowId.entryFile)
                    }

                    QC.Label {
                        color: Theme.textSubtle
                        text: attachedRowId.warningCount > 0
                              ? qsTr("%1 files, %2 warnings")
                                    .arg(attachedRowId.depCount)
                                    .arg(attachedRowId.warningCount)
                              : qsTr("%1 files").arg(attachedRowId.depCount)
                    }
                }
            }
        }

        // The three switcher buttons act like tabs — one ButtonGroup keeps
        // them mutually exclusive across the two group boxes.
        QC.ButtonGroup {
            buttons: [cavernInputButton, cavernOutputButton, loopClosureButton]
        }

        // Flow instead of RowLayout so the Output box wraps under the
        // Input box on narrow windows instead of being cut off.
        QQ.Flow {
            Layout.fillWidth: true
            spacing: Theme.sectionSpacing

            QC.GroupBox {
                objectName: "cavernInputGroupBox"
                title: qsTr("Input")

                QC.Button {
                    id: cavernInputButton
                    objectName: "cavernInputButton"
                    text: qsTr("Cavern Input")
                    checkable: true
                }
            }

            QC.GroupBox {
                objectName: "cavernOutputGroupBox"
                title: qsTr("Output")

                RowLayout {
                    spacing: Theme.tightSpacing

                    QC.Button {
                        id: cavernOutputButton
                        objectName: "cavernOutputButton"
                        text: qsTr("Cavern Output")
                        checkable: true
                        checked: true
                    }

                    QC.Button {
                        id: loopClosureButton
                        objectName: "loopClosureButton"
                        text: qsTr("Loop Closure")
                        checkable: true
                        // The .err sidecar is only written when the cave
                        // has loops; disable when there's nothing to show.
                        enabled: root.loopClosureStats.length > 0
                        onEnabledChanged: {
                            if (!enabled && checked) {
                                cavernOutputButton.checked = true
                            }
                        }
                    }
                }
            }
        }

        QC.ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            QC.TextArea {
                objectName: "cavernTextArea"
                readOnly: true
                selectByMouse: true
                font.family: Theme.fontFamilyMono
                font.pixelSize: Theme.fontSizeSmall
                wrapMode: QC.TextArea.NoWrap
                text: {
                    if (cavernInputButton.checked) {
                        return root.linePlotManager !== null
                                ? root.linePlotManager.driverSource : ""
                    }
                    if (loopClosureButton.checked) {
                        return root.loopClosureStats
                    }
                    return root.cavernLog
                }
                placeholderText: {
                    if (cavernInputButton.checked) {
                        return qsTr("No survex input — run a solve first.")
                    }
                    if (loopClosureButton.checked) {
                        return qsTr("No loop closure data — the cave has no loops.")
                    }
                    return qsTr("No cavern output to display.")
                }
            }
        }
    }
}

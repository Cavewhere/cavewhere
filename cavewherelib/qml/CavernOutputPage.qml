import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

StandardPage {
    id: root
    objectName: "cavernOutputPage"

    readonly property LinePlotManager linePlotManager: RootData.linePlotManager
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

        // Header row: status on the left, primary action on the right.
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.sectionSpacing

            QC.Label {
                objectName: "statusLabel"
                Layout.fillWidth: true
                font.pixelSize: Theme.fontSizeMedium
                color: root.hasError ? Theme.danger : Theme.text
                wrapMode: QC.Label.WordWrap
                text: root.hasError
                      ? qsTr("Cavern reported an error during the last solve.")
                      : qsTr("Last solve completed successfully.")
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

        QC.TabBar {
            id: outputTabBar
            objectName: "outputTabBar"
            Layout.fillWidth: true

            QC.TabButton {
                objectName: "cavernLogTab"
                text: qsTr("Cavern log")
            }

            QC.TabButton {
                objectName: "loopClosureTab"
                text: qsTr("Loop closure")
                // The .err sidecar is only written when the cave has loops.
                // Hide the tab when there's nothing to show so users aren't
                // misled into thinking the tab is broken.
                enabled: root.loopClosureStats.length > 0
            }
        }

        QC.Frame {
            Layout.fillWidth: true
            Layout.fillHeight: true

            StackLayout {
                anchors.fill: parent
                currentIndex: outputTabBar.currentIndex

                QC.ScrollView {
                    clip: true

                    QC.TextArea {
                        objectName: "cavernLogTextArea"
                        readOnly: true
                        selectByMouse: true
                        font.family: Theme.fontFamilyMono
                        font.pixelSize: Theme.fontSizeSmall
                        text: root.cavernLog
                        placeholderText: qsTr("No cavern output to display.")
                        wrapMode: QC.TextArea.NoWrap
                    }
                }

                QC.ScrollView {
                    clip: true

                    QC.TextArea {
                        objectName: "loopClosureTextArea"
                        readOnly: true
                        selectByMouse: true
                        font.family: Theme.fontFamilyMono
                        font.pixelSize: Theme.fontSizeSmall
                        text: root.loopClosureStats
                        placeholderText: qsTr("No loop closure data — the cave has no loops.")
                        wrapMode: QC.TextArea.NoWrap
                    }
                }
            }
        }
    }
}

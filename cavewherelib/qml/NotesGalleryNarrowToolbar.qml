pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

QQ.Rectangle {
    id: narrowToolbar

    property bool shown: true
    property int currentNoteIndex: 0
    property int noteCount: 0
    property string galleryState: ""
    property string galleryMode: ""
    property bool hasCurrentNote: false
    property bool hasCurrentSketch: false
    property bool hasSelection: false

    signal navigatePrev()
    signal navigateNext()
    signal stateChangeRequested(string newState)
    signal doneClicked()
    signal rotateRequested()
    signal notePickerRequested()
    signal deleteSelectedRequested()

    visible: shown
    height: shown ? narrowToolbarColumn.implicitHeight + 2 * Theme.flowSpacing : 0
    color: Theme.floatingWidgetColor

    QQ.Behavior on height {
        QQ.NumberAnimation { duration: 120 }
    }

    ColumnLayout {
        id: narrowToolbarColumn

        anchors.top: parent.top
        anchors.topMargin: Theme.flowSpacing
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: Theme.flowSpacing
        anchors.rightMargin: Theme.flowSpacing
        spacing: Theme.flowSpacing

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.flowSpacing

            RoundButton {
                objectName: "prevButton"
                icon.source: "qrc:/twbs-icons/icons/chevron-left.svg"
                enabled: narrowToolbar.currentNoteIndex > 0
                onClicked: narrowToolbar.navigatePrev()
            }

            QC.Label {
                objectName: "counterLabel"

                text: (narrowToolbar.currentNoteIndex + 1) + " / " + narrowToolbar.noteCount
                font.pixelSize: Theme.fontSizeSmall

                QQ.TapHandler {
                    onTapped: narrowToolbar.notePickerRequested()
                }
            }

            RoundButton {
                objectName: "nextButton"
                icon.source: "qrc:/twbs-icons/icons/chevron-right.svg"
                enabled: narrowToolbar.currentNoteIndex < narrowToolbar.noteCount - 1
                onClicked: narrowToolbar.navigateNext()
            }

            QQ.Item { Layout.fillWidth: true }

            RoundButton {
                objectName: "editButton"
                icon.source: "qrc:/twbs-icons/icons/pencil.svg"
                checked: narrowToolbar.galleryMode === "CARPET"
                onClicked: {
                    if (narrowToolbar.galleryMode === "CARPET") {
                        narrowToolbar.doneClicked()
                    } else {
                        narrowToolbar.stateChangeRequested("SELECT")
                    }
                }
            }
        }

        QQ.Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: row2Layout.implicitHeight + 2 * Theme.flowSpacing
            visible: narrowToolbar.galleryMode === "CARPET"
            color: Theme.floatingWidgetRaisedColor
            radius: Theme.floatingWidgetRadius

            RowLayout {
                id: row2Layout
                anchors.fill: parent
                anchors.margins: Theme.flowSpacing
                spacing: Theme.flowSpacing

                RoundButton {
                    objectName: "selectButton"
                    icon.source: "qrc:/twbs-icons/icons/hand-index.svg"
                    checked: narrowToolbar.galleryState === "SELECT"
                    onClicked: narrowToolbar.stateChangeRequested("SELECT")
                }

                RoundButton {
                    objectName: "addScrapButton"
                    icon.source: "qrc:/twbs-icons/icons/bounding-box.svg"
                    visible: narrowToolbar.hasCurrentNote
                    checked: narrowToolbar.galleryState === "ADD-SCRAP"
                    onClicked: narrowToolbar.stateChangeRequested("ADD-SCRAP")
                }

                RoundButton {
                    objectName: "addStationButton"
                    icon.source: "qrc:/twbs-icons/icons/geo-alt.svg"
                    checked: narrowToolbar.galleryState === "ADD-STATION"
                    onClicked: narrowToolbar.stateChangeRequested("ADD-STATION")
                }

                RoundButton {
                    objectName: "addLeadButton"
                    icon.source: "qrc:/twbs-icons/icons/question-circle.svg"
                    visible: narrowToolbar.hasCurrentNote
                    checked: narrowToolbar.galleryState === "ADD-LEAD"
                    onClicked: narrowToolbar.stateChangeRequested("ADD-LEAD")
                }

                RoundButton {
                    objectName: "addSketchWallButton"
                    icon.source: "qrc:/twbs-icons/icons/border.svg"
                    visible: narrowToolbar.hasCurrentSketch
                    checked: narrowToolbar.galleryState === "ADD-SKETCH-WALL"
                    onClicked: narrowToolbar.stateChangeRequested("ADD-SKETCH-WALL")
                }

                RoundButton {
                    objectName: "addSketchFeatureButton"
                    icon.source: "qrc:/twbs-icons/icons/brush.svg"
                    visible: narrowToolbar.hasCurrentSketch
                    checked: narrowToolbar.galleryState === "ADD-SKETCH-FEATURE"
                    onClicked: narrowToolbar.stateChangeRequested("ADD-SKETCH-FEATURE")
                }

                QQ.Rectangle {
                    width: 1
                    Layout.preferredHeight: 20
                    Layout.alignment: Qt.AlignVCenter
                    color: Theme.border
                    visible: narrowToolbar.hasCurrentNote
                }

                RoundButton {
                    objectName: "rotateButton"
                    icon.source: "qrc:/twbs-icons/icons/arrow-repeat.svg"
                    visible: narrowToolbar.hasCurrentNote
                    onClicked: narrowToolbar.rotateRequested()
                }

                QQ.Item { Layout.fillWidth: true }

                RoundButton {
                    objectName: "deleteSelectedButton"
                    icon.source: "qrc:/twbs-icons/icons/trash.svg"
                    visible: narrowToolbar.galleryState === "SELECT" && narrowToolbar.hasSelection
                    onClicked: narrowToolbar.deleteSelectedRequested()
                }
            }
        }
    }
}

/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import cavewherelib
import QtQuick.Controls as QC
import QtQuick.Layouts
import QtQuick.Effects
import "Utils.js" as Utils

QQ.Item {
    id: clipArea

    property Trip currentTrip
    property TripCalibration currentCalibration: currentTrip.calibration
    readonly property alias contentWidth: scrollAreaId.width //For animation
    property bool isNarrow: false
    property bool showNotes: false
    property SurveyNotesConcatModel notesModel: null

    signal collapseClicked();
    signal noteClicked(int noteIndex)

    clip: false

    width: isNarrow ? parent.width : scrollAreaId.width
    height: 100

    TripLengthTask {
        id: tripLengthTask
        trip: clipArea.currentTrip
    }

    RemovePreview {
        id: removePreviewId
    }


    SurveyEditorModel {
        id: editorModel
        trip: clipArea.currentTrip

        onLastChunkAdded: {
            editorModel.focusOnLastChunk()
        }
    }

    QC.ScrollView {
        id: scrollAreaId

        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.margins: 1;

        QC.ScrollBar.vertical.stepSize: 50;
        QC.ScrollBar.vertical.policy: clipArea.isNarrow ? QC.ScrollBar.AlwaysOff : QC.ScrollBar.AsNeeded

        width: clipArea.isNarrow ? clipArea.width : 500
        visible: true
        clip: true

        QQ.ListView {
            id: viewId
            objectName: "view"

            QC.ButtonGroup {
                id: errorButtonGroupId
            }

            StationValidator {
                id: stationValidatorId
            }

            DistanceValidator {
                id: distanceValidatorId
            }

            CompassValidator {
                id: compassValidatorId
            }

            ClinoValidator {
                id: clinoValidatorId
            }

            function syncFocusedCellInView() {
                let row = editorModel.focusedRow
                let role = editorModel.focusedRole
                if(row < 0 || role < 0) {
                    return
                }
                let focusedCell = editorModel.cellIndex(row, role)
                if(!editorModel.isCellValid(focusedCell)) {
                    return
                }

                editorModel.setFocusedChunk(editorModel.chunkForRow(row))

                if(editorModel.isShotRole(role)) {
                    //Since the shot row has a height of zero, this -1 and +1 forces shot to be visible in the list view
                    viewId.positionViewAtIndex(row - 1, QQ.ListView.Contain)
                    viewId.positionViewAtIndex(row + 1, QQ.ListView.Contain)
                } else {
                    viewId.positionViewAtIndex(row, QQ.ListView.Contain)
                }
            }

            QQ.Connections {
                target: editorModel
                function onFocusedRowChanged() {
                    Qt.callLater(viewId.syncFocusedCellInView)
                }
                function onFocusedRoleChanged() {
                    Qt.callLater(viewId.syncFocusedCellInView)
                }
            }

            //reuseItem can cause instablity in testcases and crashes in qml, by default it's false
            // reuseItems: true

            currentIndex: -1 //The currentIndex should change

            //Prevents default keyboard interaction
            keyNavigationEnabled: false

            header: ColumnLayout {
                id: column
                spacing: 5
                width: scrollAreaId.width - viewId.contentMargin

                ColumnLayout {
                    Layout.fillWidth: true


                    QQ.Item {
                        Layout.fillWidth: true
                        implicitHeight: collapseButton.height
                        visible: !clipArea.isNarrow
                        SectionLabel {
                            text: "Trip"
                        }

                        QC.Button {
                            id: collapseButton
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: 3
                            icon.source: "qrc:/twbs-icons/icons/chevron-left.svg"
                            icon.width: 16
                            icon.height: 16
                            onClicked:  clipArea.collapseClicked()
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        DoubleClickTextInput {
                            id: tripNameText
                            objectName: "tripNameText"
                            text: clipArea.currentTrip.name
                            font.bold: true
                            font.pixelSize: Theme.fontSizeTitle
                            autoResize: true
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0

                            onFinishedEditting: (newText) => {
                                                    clipArea.currentTrip.name = newText
                                                }
                        }

                        DoubleClickTextInput {
                            id: tripDate
                            objectName: "tripDate"

                            text: Qt.formatDate(clipArea.currentTrip.date, "yyyy-MM-dd")

                            font.bold: true
                            Layout.alignment: Qt.AlignRight

                            onFinishedEditting: function(newText) {
                                let dateTime = newText + " 00:00:00";
                                clipArea.currentTrip.date = Date.fromLocaleString(Qt.locale(), dateTime, "yyyy-MM-dd HH:mm:ss");
                            }
                        }
                    }
                }

                BreakLine { }

                CalibrationEditor {
                    Layout.fillWidth: true
                    calibration: currentTrip === null ? null : clipArea.currentTrip.calibration
                }

                BreakLine { }

                TeamTable {
                    id: teamTable
                    model: currentTrip !== null ? clipArea.currentTrip.team : null
                    Layout.fillWidth: true
                }

                // Note thumbnails — hidden when full gallery is shown (wide mode)
                ColumnLayout {
                    Layout.fillWidth: true
                    visible: clipArea.showNotes && clipArea.notesModel !== null

                    BreakLine { }

                    SectionHeader {
                        Layout.alignment: Qt.AlignHCenter
                        text: "Notes"
                        showAddButton: true
                        onAddClicked: addNotesFileDialog.open()

                        NotesFileDialog {
                            id: addNotesFileDialog
                            onFilesSelected: (images) => clipArea.notesModel.addFiles(images)
                        }
                    }

                    QQ.Flow {
                        Layout.fillWidth: true
                        spacing: Theme.flowSpacing

                        QQ.Repeater {
                            model: clipArea.notesModel

                            delegate: QQ.Item {
                                id: thumbDelegate
                                required property url iconPath
                                required property QQ.QtObject noteObject
                                required property int index

                                width: thumbSize
                                height: thumbSize

                                readonly property real thumbSize: 80

                                QQ.Image {
                                    id: thumbImage
                                    anchors.fill: parent
                                    anchors.margins: 2
                                    source: thumbDelegate.iconPath
                                    fillMode: QQ.Image.PreserveAspectFit
                                    asynchronous: true
                                    sourceSize: Qt.size(width, height)
                                    rotation: thumbDelegate.noteObject?.rotate ?? 0
                                }

                                QC.BusyIndicator {
                                    anchors.centerIn: parent
                                    running: thumbImage.visible && thumbImage.status === QQ.Image.Loading
                                }

                                QQ.Rectangle {
                                    anchors.fill: parent
                                    anchors.margins: 2
                                    visible: thumbDelegate.iconPath.toString().length === 0
                                    color: Theme.surfaceMuted
                                    radius: 4
                                    QC.Label {
                                        anchors.centerIn: parent
                                        text: "3D"
                                        color: Theme.textSubtle
                                    }
                                }

                                QQ.TapHandler {
                                    gesturePolicy: QQ.TapHandler.ReleaseWithinBounds
                                    onSingleTapped: clipArea.noteClicked(thumbDelegate.index)
                                }
                            }
                        }
                    }

                }

                BreakLine { }

                SectionLabel {
                    text: "Data"
                    Layout.alignment: Qt.AlignHCenter
                }

                SurveyErrorOverview {
                    trip: currentTrip
                }

                AddButton {
                    id: addSurveyData
                    objectName: "addSurveyData"
                    text: "Add Survey Data "
                    Layout.alignment: Qt.AlignHCenter
                    visible: clipArea.currentTrip !== null && clipArea.currentTrip.chunkCount === 0

                    onClicked: {
                        clipArea.currentTrip.addNewChunk()
                    }
                }
            }


            model: editorModel

            readonly property int contentMargin: clipArea.isNarrow ? 10 : 30

            SurveyEditorColumnTitles {
                id: titleTemplate
                width: viewId.width - viewId.contentMargin
                visible: false
                listViewIndex: -1
                chunk: null
            }


            delegate: DrySurveyComponent {
                calibration: currentCalibration
                errorButtonGroup: errorButtonGroupId
                // view: viewId
                model: editorModel
                removePreview: removePreviewId
                stationValidator: stationValidatorId
                distanceValidator: distanceValidatorId
                compassValidator: compassValidatorId
                clinoValidator: clinoValidatorId
                columnTemplate: titleTemplate
            }


            footer: ColumnLayout {
                width: scrollAreaId.width - viewId.contentMargin

                QC.Label {
                    objectName: "totalLengthText"
                    text: {
                        if(clipArea.currentTrip === null) { return "" }
                        var unit = ""
                        switch(clipArea.currentTrip.calibration.distanceUnit) {
                        case Units.Meters:
                            unit = "m"
                            break;
                        case Units.Feet:
                            unit = "ft"
                            break;
                        }

                        return "Total Length: " + Utils.fixed(tripLengthTask.length, 2) + " " + unit;
                    }
                }

                QC.Button {
                    objectName: "spaceAddBar"
                    visible: currentTrip !== null && currentTrip.chunkCount > 0
                    Layout.alignment: Qt.AlignHCenter
                    text: "Press <b>Space</b> to add another data block"
                    onClicked: clipArea.currentTrip.addNewChunk();
                }
            }
        }
    }
}

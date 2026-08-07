import QtQuick
import QtQuick.Controls as QC
import cavewherelib

Item {
    id: itemId

    width: ListView.view ? ListView.view.width : 400
    height: {
        switch(itemId.rowType) {
        case SurveyEditorRowIndex.TitleRow:
            return titleLoaderId.item ? titleLoaderId.item.height + titleOffset : 0
        case SurveyEditorRowIndex.StationRow:
            return 49
        case SurveyEditorRowIndex.ShotRow:
            return itemId.shotBoxShift
        case SurveyEditorRowIndex.SplayRow:
            return itemId.columnTemplate.splayRowHeight
        }
    }

    //Visual properties
    property TripCalibration calibration: null
    property bool canEditTripCalibration: false
    readonly property bool frontSights: calibration !== null && calibration.frontSights
    readonly property bool backSights: calibration !== null && calibration.backSights
    required property int index;
    required property QC.ButtonGroup errorButtonGroup
    required property var removePreview
    required property FixStationPopup fixStationPopup
    required property int rowType
    required property cwSurveyEditorRowIndex rowIndex

    //Data that comes from the model
    required property cwSurveyEditorBoxData stationName;
    required property cwSurveyEditorBoxData stationLeft;
    required property cwSurveyEditorBoxData stationRight;
    required property cwSurveyEditorBoxData stationUp;
    required property cwSurveyEditorBoxData stationDown;
    required property bool stationFixed;
    required property cwSurveyEditorBoxData shotDistance;
    required property bool shotDistanceIncluded;
    required property cwSurveyEditorBoxData shotCompass;
    required property cwSurveyEditorBoxData shotBackCompass;
    required property cwSurveyEditorBoxData shotClino;
    required property cwSurveyEditorBoxData shotBackClino;
    required property int stationSplayCount;
    required property bool stationSplaysExpanded;
    required property string splayDistance;
    required property string splayCompass;
    required property string splayClino;

    //Visualize properties
    required property SurveyEditorColumnTitles columnTemplate
    required property SurveyEditorModel model;

    //Validators
    required property StationValidator stationValidator;
    required property DistanceValidator distanceValidator;
    required property CompassValidator compassValidator;
    required property ClinoValidator clinoValidator;

    //For sizing
    readonly property int titleOffset: index === 0 ? 5 : 25

    //A shot row is normally zero-height, with its boxes reaching up over the
    //boundary from columnTemplate.shotRowY. An open splay cluster stands between
    //the station and its shot, so the row takes on the height the boxes reach up
    //by and drops them below the cluster instead of covering the last splay
    readonly property real shotBoxShift: stationSplaysExpanded ? -columnTemplate.shotRowY : 0

    Loader {
        id: titleLoaderId
        active: itemId.rowType === SurveyEditorRowIndex.TitleRow
        sourceComponent: SurveyEditorColumnTitles {
            id: titleColumnId
            width: itemId.columnTemplate.width
            y: titleOffset
            shotOffset: Math.floor(50.0 / 2.0);
            chunk: itemId.model.chunkForRow(itemId.index)
            dataRowHeight: itemId.columnTemplate.dataRowHeight
            listViewIndex: index
        }
    }

    //Station data loader
    Loader {
        id: stationLoaderId
        active: itemId.rowType === SurveyEditorRowIndex.StationRow

        sourceComponent: Item {
            StationBox {
                id: stationBox
                // width: titleColumnId.stationWidth
                width: itemId.columnTemplate.stationWidth
                height: itemId.columnTemplate.dataRowHeight

                dataValue: itemId.stationName
                listViewIndex: itemId.index
                errorButtonGroup: itemId.errorButtonGroup
                model: itemId.model
                removePreview: itemId.removePreview
                calibration: itemId.calibration
                view: itemId.ListView.view
                dataValidator: stationValidator
                fixStationPopup: itemId.fixStationPopup
                stationIsFixed: itemId.stationFixed
            }


            StationDistanceBox {
                id: leftBox
                width: itemId.columnTemplate.lWidth
                height: itemId.columnTemplate.dataRowHeight
                x: itemId.columnTemplate.leftX
                anchors.top: stationBox.top
                anchors.topMargin: 0

                dataValue: stationLeft
                listViewIndex: itemId.index
                errorButtonGroup: itemId.errorButtonGroup

                model: itemId.model
                removePreview: itemId.removePreview
                calibration: itemId.calibration
                view: itemId.ListView.view
                dataValidator: distanceValidator
            }

            StationDistanceBox {
                id: rightBox
                width: itemId.columnTemplate.rWidth
                height: itemId.columnTemplate.dataRowHeight
                anchors.topMargin: 0
                anchors.top: stationBox.top
                anchors.left: leftBox.right
                anchors.leftMargin: -1

                dataValue: stationRight
                listViewIndex: itemId.index
                errorButtonGroup: itemId.errorButtonGroup
                model: itemId.model
                removePreview: itemId.removePreview
                calibration: itemId.calibration
                view: itemId.ListView.view
                dataValidator: distanceValidator
            }

            StationDistanceBox {
                id: upBox
                width: itemId.columnTemplate.uWidth
                height: itemId.columnTemplate.dataRowHeight
                anchors.topMargin: 0
                anchors.top: stationBox.top
                anchors.left: rightBox.right
                anchors.leftMargin: -1

                dataValue:  stationUp
                listViewIndex: itemId.index
                errorButtonGroup: itemId.errorButtonGroup
                model: itemId.model
                removePreview: itemId.removePreview
                calibration: itemId.calibration
                view: itemId.ListView.view
                dataValidator: distanceValidator
            }

            StationDistanceBox {
                id: downBox
                width: itemId.columnTemplate.dWidth
                height: itemId.columnTemplate.dataRowHeight
                anchors.topMargin: 0
                anchors.top: stationBox.top
                anchors.left: upBox.right
                anchors.leftMargin: -1

                dataValue: stationDown
                listViewIndex: itemId.index
                errorButtonGroup: itemId.errorButtonGroup
                model: itemId.model
                removePreview: itemId.removePreview
                calibration: itemId.calibration
                view: itemId.ListView.view
                dataValidator: distanceValidator
            }

            //Most stations carry no splays, so the chip stays uninstantiated
            //rather than laying out text it will never show
            Loader {
                active: itemId.stationSplayCount > 0
                anchors.verticalCenter: downBox.verticalCenter
                anchors.left: downBox.right
                anchors.leftMargin: Theme.delegatePadding

                sourceComponent: Rectangle {
                    objectName: "splayChip"

                    width: chipRowId.implicitWidth + Theme.delegatePadding * 2
                    height: chipRowId.implicitHeight + Theme.delegatePadding

                    radius: height * 0.5
                    color: Theme.splaySurface
                    border.color: Theme.splayBorder
                    border.width: 1

                    Row {
                        id: chipRowId
                        anchors.centerIn: parent
                        spacing: Theme.flowSpacing

                        //A glyph rather than the chevron SVG the rest of the app
                        //uses: Icon colorizes through a MultiEffect layer, which
                        //costs an offscreen texture in every recycled row and
                        //renders as nothing in offscreen tests
                        QC.Label {
                            anchors.verticalCenter: parent.verticalCenter
                            text: "▶"
                            color: Theme.splayText
                            font.pixelSize: Theme.fontSizeCaption
                            rotation: itemId.stationSplaysExpanded ? 90 : 0

                            Behavior on rotation {
                                NumberAnimation { duration: 120 }
                            }
                        }

                        QC.Label {
                            objectName: "splayChipCount"
                            anchors.verticalCenter: parent.verticalCenter
                            text: itemId.stationSplayCount
                            color: Theme.splayText
                            font.pixelSize: Theme.fontSizeSmall
                        }
                    }

                    TapHandler {
                        onSingleTapped: itemId.model.toggleSplaysExpanded(itemId.rowIndex)
                    }
                }
            }
        }

    }

    //Splay data loader. Splays are read-only here — editing them is a later
    //feature, so the row shows the readings as they were written
    Loader {
        id: splayLoaderId
        active: itemId.rowType === SurveyEditorRowIndex.SplayRow

        sourceComponent: Rectangle {
            width: itemId.columnTemplate.width
            height: itemId.columnTemplate.splayRowHeight
            color: Theme.splaySurface

            QC.Label {
                objectName: "splayRowLabel"

                readonly property real splayNameIndent: 20

                x: itemId.columnTemplate.stationX + splayNameIndent
                anchors.verticalCenter: parent.verticalCenter
                text: "splay"
                color: Theme.splayText
                font.pixelSize: Theme.fontSizeCaption
            }

            QC.Label {
                objectName: "splayDistanceLabel"
                x: itemId.columnTemplate.distanceX
                width: itemId.columnTemplate.distanceWidth
                anchors.verticalCenter: parent.verticalCenter
                horizontalAlignment: Text.AlignHCenter
                text: itemId.splayDistance
                font.pixelSize: Theme.fontSizeSmall
            }

            QC.Label {
                objectName: "splayCompassLabel"
                x: itemId.columnTemplate.compassX
                width: itemId.columnTemplate.compassWidth
                anchors.verticalCenter: parent.verticalCenter
                horizontalAlignment: Text.AlignHCenter
                text: itemId.splayCompass
                font.pixelSize: Theme.fontSizeSmall
            }

            QC.Label {
                objectName: "splayClinoLabel"
                x: itemId.columnTemplate.clinoX
                width: itemId.columnTemplate.clinoWidth
                anchors.verticalCenter: parent.verticalCenter
                horizontalAlignment: Text.AlignHCenter
                text: itemId.splayClino
                font.pixelSize: Theme.fontSizeSmall
            }
        }
    }

    Loader {
        id: shotLoaderId
        active: itemId.rowType === SurveyEditorRowIndex.ShotRow


        sourceComponent: Item {
            ShotDistanceDataBox {
                id: shotDistanceDataBox
                width: itemId.columnTemplate.distanceWidth
                height: itemId.columnTemplate.dataRowHeight
                x: itemId.columnTemplate.distanceX
                y: itemId.columnTemplate.shotRowY + itemId.shotBoxShift
                anchors.topMargin: 0

                dataValue: shotDistance
                listViewIndex: itemId.index
                errorButtonGroup: itemId.errorButtonGroup
                model: itemId.model
                removePreview: itemId.removePreview
                calibration: itemId.calibration
                view: itemId.ListView.view
                dataValidator: distanceValidator
                distanceIncluded: shotDistanceIncluded
            }

            CompassReadBox {
                id: compassFrontReadBox
                width: itemId.columnTemplate.compassWidth
                height: itemId.backSights ? itemId.columnTemplate.dataRowHalfHeight : itemId.columnTemplate.dataRowHeight
                anchors.left: shotDistanceDataBox.right
                anchors.leftMargin: -1
                anchors.top: shotDistanceDataBox.top
                anchors.topMargin: 0

                visible: itemId.frontSights
                dataValue: shotCompass
                listViewIndex: itemId.index
                errorButtonGroup: itemId.errorButtonGroup
                model: itemId.model
                removePreview: itemId.removePreview
                calibration: itemId.calibration
                view: itemId.ListView.view
                readingText: "fs"
                dataValidator: compassValidator
            }

            CompassReadBox {
                id: compassBackReadBox
                width: itemId.columnTemplate.compassWidth
                height: itemId.frontSights ? itemId.columnTemplate.dataRowHalfHeight + 1 : itemId.columnTemplate.dataRowHeight
                anchors.left: compassFrontReadBox.left
                anchors.top: itemId.frontSights ? shotDistanceDataBox.verticalCenter : shotDistanceDataBox.top
                anchors.topMargin: itemId.frontSights ? -1 : 0

                visible: itemId.backSights
                dataValue: shotBackCompass
                listViewIndex: itemId.index
                errorButtonGroup: itemId.errorButtonGroup
                model: itemId.model
                removePreview: itemId.removePreview
                calibration: itemId.calibration
                view: itemId.ListView.view
                readingText: "bs"
                dataValidator: compassValidator
            }

            ClinoReadBox {
                id: clinoFrontReadBox
                width: itemId.columnTemplate.clinoWidth
                height: itemId.backSights ? itemId.columnTemplate.dataRowHalfHeight : itemId.columnTemplate.dataRowHeight
                anchors.top: shotDistanceDataBox.top
                anchors.topMargin: 0
                anchors.left: compassFrontReadBox.right
                anchors.leftMargin: -1
                visible: itemId.frontSights

                dataValue: shotClino
                listViewIndex: itemId.index
                errorButtonGroup: itemId.errorButtonGroup
                model: itemId.model
                removePreview: itemId.removePreview
                calibration: itemId.calibration
                view: itemId.ListView.view
                readingText: "fs"
                dataValidator: clinoValidator
            }

            ClinoReadBox {
                id: clinoBackReadBox
                width: itemId.columnTemplate.clinoWidth
                height: itemId.frontSights ? itemId.columnTemplate.dataRowHalfHeight + 1 : itemId.columnTemplate.dataRowHeight
                anchors.topMargin: itemId.frontSights ? -1 : 0
                anchors.top: itemId.frontSights ? shotDistanceDataBox.verticalCenter : shotDistanceDataBox.top
                anchors.left: compassFrontReadBox.right
                anchors.leftMargin: -1
                visible: itemId.backSights

                dataValue: shotBackClino
                listViewIndex: itemId.index
                errorButtonGroup: itemId.errorButtonGroup
                model: itemId.model
                removePreview: itemId.removePreview
                calibration: itemId.calibration
                view: itemId.ListView.view
                readingText: "bs"
                dataValidator: clinoValidator
            }

        }
    }
}

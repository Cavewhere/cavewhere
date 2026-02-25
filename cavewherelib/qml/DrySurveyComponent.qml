import QtQuick
import QtQuick.Controls as QC
import cavewherelib

Item {
    id: itemId

    width: 400
    height: {
        switch(itemId.rowType) {
        case SurveyEditorRowIndex.TitleRow:
            return titleLoaderId.item ? titleLoaderId.item.height + titleOffset : 0
        case SurveyEditorRowIndex.StationRow:
            return 49
        case SurveyEditorRowIndex.ShotRow:
            return 0
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
    required property int rowType

    //Data that comes from the model
    required property cwSurveyEditorBoxData stationName;
    required property cwSurveyEditorBoxData stationLeft;
    required property cwSurveyEditorBoxData stationRight;
    required property cwSurveyEditorBoxData stationUp;
    required property cwSurveyEditorBoxData stationDown;
    required property cwSurveyEditorBoxData shotDistance;
    required property cwSurveyEditorBoxData shotCompass;
    required property cwSurveyEditorBoxData shotBackCompass;
    required property cwSurveyEditorBoxData shotClino;
    required property cwSurveyEditorBoxData shotBackClino;

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

    Loader {
        id: titleLoaderId
        active: itemId.rowType === SurveyEditorRowIndex.TitleRow
        sourceComponent: SurveyEditorColumnTitles {
            id: titleColumnId
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
                view: itemId.ListView.view
                dataValidator: stationValidator
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
                view: itemId.ListView.view
                dataValidator: distanceValidator
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
                y: itemId.columnTemplate.shotRowY
                anchors.topMargin: 0

                dataValue: shotDistance
                listViewIndex: itemId.index
                errorButtonGroup: itemId.errorButtonGroup
                model: itemId.model
                removePreview: itemId.removePreview
                view: itemId.ListView.view
                dataValidator: distanceValidator
                distanceIncluded: itemId.model.shotDistanceIncludedAt(itemId.model.cellIndex(itemId.index, SurveyChunk.ShotDistanceRole))
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
                view: itemId.ListView.view
                readingText: "bs"
                dataValidator: clinoValidator
            }

        }
    }

    Text {
        x: 20
        text: itemId.rowType + " " + index
    }
}

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
    required property RemoveAskBox splayRemoveChallenge
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
    required property string splayStationName;

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

            SplaysBox {
                id: splaysBox
                width: itemId.columnTemplate.splaysWidth
                height: itemId.columnTemplate.dataRowHeight
                anchors.top: stationBox.top
                anchors.left: downBox.right
                anchors.leftMargin: -1

                model: itemId.model
                rowIndex: itemId.rowIndex
                stationData: itemId.stationName
                listViewIndex: itemId.index
                splayCount: itemId.stationSplayCount
                splaysExpanded: itemId.stationSplaysExpanded
                splayRemoveChallenge: itemId.splayRemoveChallenge
                removePreview: itemId.removePreview
                calibration: itemId.calibration
                view: itemId.ListView.view
            }
        }

    }

    //Splay data loader. The readings are read-only here — editing them is a
    //later feature, so the row shows them as they were written, and offers the
    //one action it has through its menu. The rail down the station column ties
    //the cluster back to the station it hangs from, which a long cluster can
    //push off-screen
    Loader {
        id: splayLoaderId
        active: itemId.rowType === SurveyEditorRowIndex.SplayRow

        sourceComponent: Item {
            id: splayRowId

            width: itemId.columnTemplate.width
            height: itemId.columnTemplate.splayRowHeight

            readonly property bool lastInCluster:
                itemId.rowIndex.splayIndex + 1 === itemId.stationSplayCount

            //Removing the station takes its splays with it
            readonly property bool removePreviewActive:
                itemId.removePreview !== null
                && itemId.removePreview.chunk === itemId.rowIndex.chunk
                && (itemId.removePreview.previewChunkRemoval
                    || itemId.removePreview.stationIndex === itemId.rowIndex.indexInChunk)

            Rectangle {
                objectName: "splayRail"
                x: itemId.columnTemplate.stationX + itemId.columnTemplate.splayRailX
                width: itemId.columnTemplate.splayRailWidth
                //Every row but the last bridges into the row below so the
                //cluster draws one unbroken rail. The last stops at its own
                //edge, since the shot row beneath it has nothing to bridge to
                height: splayRowId.height
                        + (splayRowId.lastInCluster ? 0 : itemId.columnTemplate.columnOffset)
                color: Theme.splayBorder
            }

            QC.Label {
                objectName: "splayRowLabel"

                x: itemId.columnTemplate.stationX + itemId.columnTemplate.splayTagIndent
                width: itemId.columnTemplate.stationWidth
                       - itemId.columnTemplate.splayTagIndent
                       - itemId.columnTemplate.splayTagRightMargin
                anchors.verticalCenter: parent.verticalCenter
                horizontalAlignment: Text.AlignRight
                elide: Text.ElideLeft
                text: itemId.splayStationName + " · s" + (itemId.rowIndex.splayIndex + 1)
                color: Theme.splayText
                font.pixelSize: Theme.fontSizeCaption
            }

            SplayReadingCell {
                objectName: "splayDistanceLabel"
                x: itemId.columnTemplate.distanceX
                width: itemId.columnTemplate.distanceWidth
                height: itemId.columnTemplate.splayCellHeight
                text: itemId.splayDistance
            }

            SplayReadingCell {
                objectName: "splayCompassLabel"
                x: itemId.columnTemplate.compassX
                width: itemId.columnTemplate.compassWidth
                height: itemId.columnTemplate.splayCellHeight
                text: itemId.splayCompass
            }

            SplayReadingCell {
                objectName: "splayClinoLabel"
                x: itemId.columnTemplate.clinoX
                width: itemId.columnTemplate.clinoWidth
                height: itemId.columnTemplate.splayCellHeight
                text: itemId.splayClino
            }

            SplayRowMenu {
                id: splayRowMenuId
                anchors.fill: parent

                model: itemId.model
                rowIndex: itemId.rowIndex
            }

            SplayMenuButton {
                id: splayRowMenuButtonId
                objectName: "splayRowMenuButton"

                x: itemId.columnTemplate.clinoX
                   + itemId.columnTemplate.clinoWidth
                   + itemId.columnTemplate.splayMenuIndent
                anchors.verticalCenter: parent.verticalCenter

                onTapped: (position) => {
                              const menuPoint = splayRowMenuButtonId.mapToItem(splayRowMenuId,
                                                                               position.x,
                                                                               position.y)
                              splayRowMenuId.showMenu(menuPoint.x, menuPoint.y)
                          }
            }

            Rectangle {
                objectName: "splayRemovePreviewLine"
                x: itemId.columnTemplate.stationX + itemId.columnTemplate.splayTagIndent
                width: itemId.columnTemplate.clinoX
                       + itemId.columnTemplate.clinoWidth
                       - x
                anchors.verticalCenter: parent.verticalCenter
                height: 2
                color: Theme.text
                visible: splayRowId.removePreviewActive
                z: 2
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

import QtQuick
import QtQuick.Controls as QC
import cavewherelib

Item {
    id: itemId

    width: 400
    height: {
        switch(rowIndex.rowType) {
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
    required property int index;
    required property QC.ButtonGroup errorButtonGroup
    required property SurveyChunkTrimmer surveyChunkTrimmer;

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

    //For indexing
    required property cwSurveyEditorRowIndex rowIndex


    //Visualize properties
    required property SurveyEditorColumnTitles columnTemplate
    required property SurveyEditorModel model;

    //Validators
    required property StationValidator stationValidator;
    required property DistanceValidator distanceValidator;
    required property CompassValidator compassValidator;
    required property ClinoValidator clinoValidator;

    //For focus
    required property SurveyEditorFocus editorFocus

    //For sizing
    readonly property int titleOffset: index === 0 ? 5 : 25

    function nextLeftFromClino(rowIndex) {
        if(rowIndex.indexInChunk === 0) {
            return itemId.model.boxIndex(rowIndex.chunk, rowIndex.indexInChunk, SurveyEditorRowIndex.StationRow, SurveyChunk.StationLeftRole)
        } else {
            return itemId.model.boxIndex(rowIndex.chunk, rowIndex.indexInChunk + 1, SurveyEditorRowIndex.StationRow, SurveyChunk.StationLeftRole)
        }
    }

    Loader {
        id: titleLoaderId
        active: itemId.rowIndex.rowType === SurveyEditorRowIndex.TitleRow
        sourceComponent: SurveyEditorColumnTitles {
            id: titleColumnId
            y: titleOffset
            shotOffset: Math.floor(50.0 / 2.0);
            chunk: itemId.rowIndex.chunk
            dataRowHeight: itemId.columnTemplate.dataRowHeight
            listViewIndex: index
        }
    }

    //Station data loader
    Loader {
        id: stationLoaderId
        active: itemId.rowIndex.rowType === SurveyEditorRowIndex.StationRow

        sourceComponent: Item {
            StationBox {
                id: stationBox
                // width: titleColumnId.stationWidth
                width: itemId.columnTemplate.stationWidth
                height: itemId.columnTemplate.dataRowHeight

                navigation.tabPrevious: () => {
                    let rowIndex = stationBox.dataValue.rowIndex
                    if(rowIndex.indexInChunk === 1) {
                        let boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.StationName)
                        return itemId.model.offsetBoxIndex(boxIndex, -1);
                    } else {
                        let boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.StationDownRole)
                        return itemId.model.offsetBoxIndex(boxIndex, -1);
                    }
                }

                navigation.tabNext: () => {
                        let rowIndex = stationBox.dataValue.rowIndex
                        if(rowIndex.indexInChunk === 0) {
                            let boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.StationName)
                            return itemId.model.offsetBoxIndex(boxIndex, 1);
                        } else {
                            let boxIndex = itemId.model.boxIndex(rowIndex.chunk, rowIndex.indexInChunk - 1, SurveyEditorRowIndex.ShotRow, SurveyChunk.ShotDistanceRole)
                            return itemId.model.offsetBoxIndex(boxIndex, 0);
                        }
                    }

                navigation.arrowRight:
                    () => {
                        let rowIndex = stationBox.dataValue.rowIndex
                        let offset = rowIndex.indexInChunk === 0 ? 0 : -1
                        let boxIndex = boxIndex = itemId.model.boxIndex(rowIndex.chunk, rowIndex.indexInChunk + offset, SurveyEditorRowIndex.ShotRow, SurveyChunk.ShotDistanceRole)
                        return itemId.model.offsetBoxIndex(boxIndex, 0);
                    }

                navigation.arrowLeft:
                    () => {
                        return itemId.model.boxIndex() //Null
                    }
                navigation.arrowDown:
                    () => {
                        let rowIndex = stationBox.dataValue.rowIndex
                        let boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.StationName)
                        return itemId.model.offsetBoxIndex(boxIndex, 1);
                    }
                navigation.arrowUp:
                    () => {
                        let rowIndex = stationBox.dataValue.rowIndex
                        let boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.StationName)
                        return itemId.model.offsetBoxIndex(boxIndex, -1);
                    }

                dataValue: itemId.stationName
                listViewIndex: itemId.index
                errorButtonGroup: itemId.errorButtonGroup
                surveyChunkTrimmer: itemId.surveyChunkTrimmer
                view: itemId.ListView.view
                dataValidator: stationValidator
                editorFocus: itemId.editorFocus
            }


            StationDistanceBox {
                id: leftBox
                width: itemId.columnTemplate.lWidth
                height: itemId.columnTemplate.dataRowHeight
                x: itemId.columnTemplate.leftX
                anchors.top: stationBox.top
                anchors.topMargin: 0

                navigation.tabNext: () => {
                    let rowIndex = leftBox.dataValue.rowIndex
                    let boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.StationRightRole)
                    return itemId.model.offsetBoxIndex(boxIndex, 0);
                }
                navigation.tabPrevious:
                    () => {
                        let rowIndex = leftBox.dataValue.rowIndex
                        if(rowIndex.indexInChunk === 1) {
                            let boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.StationDownRole)
                            return itemId.model.offsetBoxIndex(boxIndex, -1);
                        }

                        let shotOffset = rowIndex.indexInChunk === 0 ? 0 : -1

                        let boxIndex;
                        if(itemId.calibration.backSights) {
                            boxIndex = itemId.model.boxIndex(rowIndex.chunk, rowIndex.indexInChunk + shotOffset, SurveyEditorRowIndex.ShotRow, SurveyChunk.ShotBackClinoRole)
                        } else if(itemId.calibration.frontSights) {
                            boxIndex = itemId.model.boxIndex(rowIndex.chunk, rowIndex.indexInChunk + shotOffset, SurveyEditorRowIndex.ShotRow, SurveyChunk.ShotClinoRole)
                        } else {
                            boxIndex = itemId.model.boxIndex(rowIndex.chunk, rowIndex.indexInChunk + shotOffset, SurveyEditorRowIndex.ShotRow, SurveyChunk.ShotDistanceRole)
                        }

                        return itemId.model.offsetBoxIndex(boxIndex, 0);
                    }
                navigation.arrowRight:
                    () => {
                        let rowIndex = leftBox.dataValue.rowIndex
                        let boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.StationRightRole)
                        return itemId.model.offsetBoxIndex(boxIndex, 0);
                    }

                navigation.arrowLeft: navigation.tabPrevious
                navigation.arrowDown:
                    () => {
                        return itemId.model.offsetBoxIndex(leftBox.dataValue.boxIndex, 1);
                    }
                navigation.arrowUp:
                    () => {
                        return itemId.model.offsetBoxIndex(leftBox.dataValue.boxIndex, -1);
                    }


                dataValue: stationLeft
                listViewIndex: itemId.index
                errorButtonGroup: itemId.errorButtonGroup

                surveyChunkTrimmer: itemId.surveyChunkTrimmer
                view: itemId.ListView.view
                dataValidator: distanceValidator
                editorFocus: itemId.editorFocus
            }

            StationDistanceBox {
                id: rightBox
                width: itemId.columnTemplate.rWidth
                height: itemId.columnTemplate.dataRowHeight
                anchors.topMargin: 0
                anchors.top: stationBox.top
                anchors.left: leftBox.right
                anchors.leftMargin: -1

                navigation.tabPrevious:
                    () => {
                        let rowIndex = rightBox.dataValue.rowIndex
                        let boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.StationLeftRole)
                        return itemId.model.offsetBoxIndex(boxIndex, 0)
                    }

                navigation.tabNext:
                    () => {
                        let rowIndex = rightBox.dataValue.rowIndex
                        let boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.StationUpRole)
                        return itemId.model.offsetBoxIndex(boxIndex, 0);
                    }
                navigation.arrowRight:
                    () => {
                        let rowIndex = rightBox.dataValue.rowIndex
                        let boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.StationUpRole)
                        return itemId.model.offsetBoxIndex(boxIndex, 0);
                    }
                navigation.arrowLeft:
                    () => {
                        let rowIndex = leftBox.dataValue.rowIndex
                        let boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.StationLeftRole)
                        return itemId.model.offsetBoxIndex(boxIndex, 0);
                    }
                navigation.arrowDown:
                    () => {
                        return itemId.model.offsetBoxIndex(rightBox.dataValue.boxIndex, 1);
                    }
                navigation.arrowUp:
                    () => {
                        return itemId.model.offsetBoxIndex(rightBox.dataValue.boxIndex, -1);
                    }


                dataValue: stationRight
                listViewIndex: itemId.index
                errorButtonGroup: itemId.errorButtonGroup
                surveyChunkTrimmer: itemId.surveyChunkTrimmer
                view: itemId.ListView.view
                dataValidator: distanceValidator
                editorFocus: itemId.editorFocus
            }

            StationDistanceBox {
                id: upBox
                width: itemId.columnTemplate.uWidth
                height: itemId.columnTemplate.dataRowHeight
                anchors.topMargin: 0
                anchors.top: stationBox.top
                anchors.left: rightBox.right
                anchors.leftMargin: -1

                navigation.tabPrevious:
                    () => {
                        let rowIndex = upBox.dataValue.rowIndex
                        let boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.StationRightRole)
                        return itemId.model.offsetBoxIndex(boxIndex, 0)
                    }

                navigation.tabNext:
                    () => {
                        let rowIndex = upBox.dataValue.rowIndex
                        let boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.StationDownRole)
                        return itemId.model.offsetBoxIndex(boxIndex, 0);
                    }
                navigation.arrowRight:
                    () => {
                        let rowIndex = upBox.dataValue.rowIndex
                        let boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.StationDownRole)
                        return itemId.model.offsetBoxIndex(boxIndex, 0);
                    }
                navigation.arrowLeft:
                    () => {
                        let rowIndex = leftBox.dataValue.rowIndex
                        let boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.StationRightRole)
                        return itemId.model.offsetBoxIndex(boxIndex, 0);
                    }
                navigation.arrowDown:
                    () => {
                        return itemId.model.offsetBoxIndex(upBox.dataValue.boxIndex, 1);
                    }

                navigation.arrowUp:
                    () => {
                        return itemId.model.offsetBoxIndex(upBox.dataValue.boxIndex, -1);
                    }



                dataValue:  stationUp
                listViewIndex: itemId.index
                errorButtonGroup: itemId.errorButtonGroup
                surveyChunkTrimmer: itemId.surveyChunkTrimmer
                view: itemId.ListView.view
                dataValidator: distanceValidator
                editorFocus: itemId.editorFocus
            }

            StationDistanceBox {
                id: downBox
                width: itemId.columnTemplate.dWidth
                height: itemId.columnTemplate.dataRowHeight
                anchors.topMargin: 0
                anchors.top: stationBox.top
                anchors.left: upBox.right
                anchors.leftMargin: -1

                navigation.tabPrevious:
                    () => {
                        let rowIndex = downBox.dataValue.rowIndex
                        let boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.StationUpRole)
                        return itemId.model.offsetBoxIndex(boxIndex, 0)
                    }

                navigation.tabNext: () => {
                    let rowIndex = downBox.dataValue.rowIndex
                    if(rowIndex.indexInChunk === 0) {
                        let boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.StationLeftRole)
                        return itemId.model.offsetBoxIndex(boxIndex, 1);
                    } else {
                        let boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.StationNameRole)
                        return itemId.model.offsetBoxIndex(boxIndex, 1);
                    }
                }
                navigation.arrowRight:
                    () => {
                        return itemId.model.boxIndex() //Null
                    }
                navigation.arrowLeft:
                    () => {
                        let rowIndex = leftBox.dataValue.rowIndex
                        let boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.StationUpRole)
                        return itemId.model.offsetBoxIndex(boxIndex, 0);
                    }
                navigation.arrowDown:
                    () => {
                        return itemId.model.offsetBoxIndex(downBox.dataValue.boxIndex, 1);
                    }
                navigation.arrowUp:
                    () => {
                        return itemId.model.offsetBoxIndex(downBox.dataValue.boxIndex, -1);
                    }


                dataValue: stationDown
                listViewIndex: itemId.index
                errorButtonGroup: itemId.errorButtonGroup
                surveyChunkTrimmer: itemId.surveyChunkTrimmer
                view: itemId.ListView.view
                dataValidator: distanceValidator
                editorFocus: itemId.editorFocus
            }
        }

    }

    Loader {
        id: shotLoaderId
        active: itemId.rowIndex.rowType === SurveyEditorRowIndex.ShotRow

        sourceComponent: Item {
            ShotDistanceDataBox {
                id: shotDistanceDataBox
                width: itemId.columnTemplate.distanceWidth
                height: itemId.columnTemplate.dataRowHeight
                x: itemId.columnTemplate.distanceX
                y: itemId.columnTemplate.shotRowY
                anchors.topMargin: 0

                navigation.tabPrevious:
                    () => {
                        let rowIndex = shotDistanceDataBox.dataValue.rowIndex
                        let boxIndex = itemId.model.boxIndex(rowIndex.chunk, rowIndex.indexInChunk + 1, SurveyEditorRowIndex.StationRow, SurveyChunk.StationNameRole)
                        return itemId.model.offsetBoxIndex(boxIndex, 0);
                    }

                navigation.tabNext:
                    () => {
                        let rowIndex = shotDistanceDataBox.dataValue.rowIndex
                        let boxIndex;

                        if(itemId.calibration.frontSights) {
                            boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.ShotCompassRole)
                        } else if(itemId.calibration.backSights) {
                            boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.ShotBackCompassRole)
                        } else {
                            boxIndex = itemId.model.boxIndex(rowIndex.chunk, rowIndex.indexInChunk + 1, SurveyEditorRowIndex.StationRow, SurveyChunk.StationLeftRole)
                        }
                        return itemId.model.offsetBoxIndex(boxIndex, 0);
                    }
                navigation.arrowRight:
                    () => {
                        let rowIndex = shotDistanceDataBox.dataValue.rowIndex
                        let boxIndex;

                        if(itemId.calibration.backSights) {
                            boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.ShotBackCompassRole)
                        } else if(itemId.calibration.frontSights) {
                            boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.ShotCompassRole)
                        } else {
                            boxIndex = itemId.model.boxIndex(rowIndex.chunk, rowIndex.indexInChunk + 1, SurveyEditorRowIndex.StationRow, SurveyChunk.StationLeftRole)
                        }
                        return itemId.model.offsetBoxIndex(boxIndex, 0);
                    }

                navigation.arrowLeft:
                    () => {
                        let rowIndex = shotDistanceDataBox.dataValue.rowIndex
                        let boxIndex = itemId.model.boxIndex(rowIndex.chunk, rowIndex.indexInChunk + 1, SurveyEditorRowIndex.StationRow, SurveyChunk.StationNameRole)
                        return itemId.model.offsetBoxIndex(boxIndex, 0);
                    }
                navigation.arrowDown:
                    () => {
                        return itemId.model.offsetBoxIndex(shotDistanceDataBox.dataValue.boxIndex, 1);
                    }
                navigation.arrowUp:
                    () => {
                        return itemId.model.offsetBoxIndex(shotDistanceDataBox.dataValue.boxIndex, -1);
                    }

                dataValue: shotDistance
                listViewIndex: itemId.index
                errorButtonGroup: itemId.errorButtonGroup
                surveyChunkTrimmer: itemId.surveyChunkTrimmer
                view: itemId.ListView.view
                dataValidator: distanceValidator
                editorFocus: itemId.editorFocus
                distanceIncluded: dataValue.chunk.data(SurveyChunk.ShotDistanceIncludedRole, dataValue.indexInChunk)
            }

            CompassReadBox {
                id: compassFrontReadBox
                width: itemId.columnTemplate.compassWidth
                height: calibration.backSights ? itemId.columnTemplate.dataRowHalfHeight : itemId.columnTemplate.dataRowHeight
                anchors.left: shotDistanceDataBox.right
                anchors.leftMargin: -1
                anchors.top: shotDistanceDataBox.top
                anchors.topMargin: 0

                navigation.tabPrevious:
                    () => {
                        let rowIndex = compassFrontReadBox.dataValue.rowIndex
                        let boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.ShotDistanceRole)
                        return itemId.model.offsetBoxIndex(boxIndex, 0);
                    }

                navigation.tabNext: () => {
                    let rowIndex = compassFrontReadBox.dataValue.rowIndex
                    let boxIndex;
                    if(itemId.calibration.backSights) {
                        boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.ShotBackCompassRole)
                    } else if(itemId.calibration.frontSights) {
                        boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.ShotClinoRole)
                    } else {
                        boxIndex = itemId.model.boxIndex(rowIndex.chunk, rowIndex.indexInChunk + 1, SurveyEditorRowIndex.StationRow, SurveyChunk.StationLeftRole)
                    }
                    return itemId.model.offsetBoxIndex(boxIndex, 0);
                }
                navigation.arrowRight:
                    () => {
                        let rowIndex = compassFrontReadBox.dataValue.rowIndex
                        let boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.ShotClinoRole)
                        return itemId.model.offsetBoxIndex(boxIndex, 0);
                    }
                navigation.arrowLeft: navigation.tabPrevious
                navigation.arrowDown:
                    () => {
                        let rowIndex = compassFrontReadBox.dataValue.rowIndex
                        if(itemId.calibration.backSights) {
                            let boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.ShotBackCompassRole)
                            return itemId.model.offsetBoxIndex(boxIndex, 0);
                        } else {
                            let boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.ShotCompassRole)
                            return itemId.model.offsetBoxIndex(boxIndex, 1);
                        }
                    }
                navigation.arrowUp:
                    () => {
                        let rowIndex = compassFrontReadBox.dataValue.rowIndex
                        if(itemId.calibration.backSights) {
                            let boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.ShotBackCompassRole)
                            return itemId.model.offsetBoxIndex(boxIndex, -1);
                        } else {
                            //front sights
                            let boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.ShotCompassRole)
                            return itemId.model.offsetBoxIndex(boxIndex, -1);
                        }
                    }

                visible: itemId.calibration.frontSights
                dataValue: shotCompass
                listViewIndex: itemId.index
                errorButtonGroup: itemId.errorButtonGroup
                surveyChunkTrimmer: itemId.surveyChunkTrimmer
                view: itemId.ListView.view
                readingText: "fs"
                dataValidator: compassValidator
                editorFocus: itemId.editorFocus
            }

            CompassReadBox {
                id: compassBackReadBox
                width: itemId.columnTemplate.compassWidth
                height: calibration.frontSights ? itemId.columnTemplate.dataRowHalfHeight + 1 : itemId.columnTemplate.dataRowHeight
                anchors.left: compassFrontReadBox.left
                anchors.top: calibration.frontSights ? shotDistanceDataBox.verticalCenter : shotDistanceDataBox.top
                anchors.topMargin: calibration.frontSights ? -1 : 0

                navigation.tabPrevious:
                    () => {
                        let rowIndex = compassBackReadBox.dataValue.rowIndex
                        let boxIndex;
                        if(itemId.calibration.frontSights) {
                            boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.ShotCompassRole)
                        } else {
                            boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.ShotDistanceRole)
                        }
                        return itemId.model.offsetBoxIndex(boxIndex, 0);
                    }

                navigation.tabNext: () => {
                    let rowIndex = compassBackReadBox.dataValue.rowIndex
                    let boxIndex;
                    if(itemId.calibration.frontSights) {
                        boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.ShotClinoRole)
                    } else if(itemId.calibration.backSights) {
                        boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.ShotBackClinoRole)
                    } else {
                        boxIndex = itemId.model.boxIndex(rowIndex.chunk, rowIndex.indexInChunk + 1, SurveyEditorRowIndex.StationRow, SurveyChunk.StationLeftRole)
                    }
                    return itemId.model.offsetBoxIndex(boxIndex, 0);
                }

                navigation.arrowRight:
                    () => {
                        let rowIndex = compassBackReadBox.dataValue.rowIndex
                        let boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.ShotBackClinoRole)
                        return itemId.model.offsetBoxIndex(boxIndex, 0);
                    }
                navigation.arrowLeft:
                    () => {
                        let rowIndex = compassBackReadBox.dataValue.rowIndex
                        let boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.ShotDistanceRole)
                        return itemId.model.offsetBoxIndex(boxIndex, 0);
                    }
                navigation.arrowDown:
                    () => {
                        let rowIndex = compassBackReadBox.dataValue.rowIndex
                        if(itemId.calibration.frontSights) {
                            let boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.ShotCompassRole)
                            return itemId.model.offsetBoxIndex(boxIndex, 1);
                        } else {
                            let boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.ShotBackCompassRole)
                            return itemId.model.offsetBoxIndex(boxIndex, 1);
                        }
                    }
                navigation.arrowUp:
                    () => {
                        let rowIndex = compassFrontReadBox.dataValue.rowIndex
                        if(itemId.calibration.frontSights) {
                            let boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.ShotCompassRole)
                            return itemId.model.offsetBoxIndex(boxIndex, 0);
                        } else {
                            //Backsights
                            let boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.ShotBackCompassRole)
                            return itemId.model.offsetBoxIndex(boxIndex, -1);
                        }
                    }

                visible: itemId.calibration.backSights
                dataValue: shotBackCompass
                listViewIndex: itemId.index
                errorButtonGroup: itemId.errorButtonGroup
                surveyChunkTrimmer: itemId.surveyChunkTrimmer
                view: itemId.ListView.view
                readingText: "bs"
                dataValidator: compassValidator
                editorFocus: itemId.editorFocus
            }

            ClinoReadBox {
                id: clinoFrontReadBox
                width: itemId.columnTemplate.clinoWidth
                height: itemId.calibration.backSights ? itemId.columnTemplate.dataRowHalfHeight : itemId.columnTemplate.dataRowHeight
                anchors.top: shotDistanceDataBox.top
                anchors.topMargin: 0
                anchors.left: compassFrontReadBox.right
                anchors.leftMargin: -1

                navigation.tabPrevious:
                    () => {
                        let rowIndex = clinoFrontReadBox.dataValue.rowIndex
                        let boxIndex;
                        if(itemId.calibration.backSights) {
                            boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.ShotBackCompassRole)
                        } else if(itemId.calibration.frontSights) {
                            boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.ShotCompassRole)
                        } else {
                            boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.ShotDistanceRole)
                        }
                        return itemId.model.offsetBoxIndex(boxIndex, 0);
                    }

                navigation.tabNext:
                    () => {
                        let rowIndex = clinoFrontReadBox.dataValue.rowIndex
                        let boxIndex;
                        if(itemId.calibration.backSights) {
                            boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.ShotBackClinoRole)
                        } else {
                            boxIndex = itemId.nextLeftFromClino(rowIndex);
                        }
                        return itemId.model.offsetBoxIndex(boxIndex, 0);
                    }
                navigation.arrowRight:
                    () => {
                        let rowIndex = clinoFrontReadBox.dataValue.rowIndex
                        let boxIndex = itemId.model.boxIndex(rowIndex.chunk, rowIndex.indexInChunk + 1, SurveyEditorRowIndex.StationRow, SurveyChunk.StationLeftRole)
                        return itemId.model.offsetBoxIndex(boxIndex, 0);
                    }
                navigation.arrowLeft:
                    () => {
                        let rowIndex = clinoFrontReadBox.dataValue.rowIndex
                        let boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.ShotCompassRole)
                        return itemId.model.offsetBoxIndex(boxIndex, 0);
                    }
                navigation.arrowDown:
                    () => {
                        let rowIndex = clinoFrontReadBox.dataValue.rowIndex
                        if(itemId.calibration.backSights) {
                            let boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.ShotBackClinoRole)
                            return itemId.model.offsetBoxIndex(boxIndex, 0);
                        } else {
                            let boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.ShotClinoRole)
                            return itemId.model.offsetBoxIndex(boxIndex, 1);
                        }
                    }
                navigation.arrowUp:
                    () => {
                        let rowIndex = clinoFrontReadBox.dataValue.rowIndex
                        if(itemId.calibration.backSights) {
                            let boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.ShotBackClinoRole)
                            return itemId.model.offsetBoxIndex(boxIndex, -1);
                        } else {
                            //front sights
                            let boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.ShotClinoRole)
                            return itemId.model.offsetBoxIndex(boxIndex, -1);
                        }
                    }

                visible: itemId.calibration.frontSights
                dataValue: shotClino
                listViewIndex: itemId.index
                errorButtonGroup: itemId.errorButtonGroup
                surveyChunkTrimmer: itemId.surveyChunkTrimmer
                view: itemId.ListView.view
                readingText: "fs"
                dataValidator: clinoValidator
                editorFocus: itemId.editorFocus
            }

            ClinoReadBox {
                id: clinoBackReadBox
                width: itemId.columnTemplate.clinoWidth
                height: itemId.calibration.frontSights ? itemId.columnTemplate.dataRowHalfHeight + 1 : itemId.columnTemplate.dataRowHeight
                anchors.topMargin: itemId.calibration.frontSights ? -1 : 0
                anchors.top: itemId.calibration.frontSights ? shotDistanceDataBox.verticalCenter : shotDistanceDataBox.top
                anchors.left: compassFrontReadBox.right
                anchors.leftMargin: -1
                visible: itemId.calibration.backSights

                navigation.tabPrevious:
                    () => {
                        let rowIndex = clinoBackReadBox.dataValue.rowIndex
                        let boxIndex;
                        if(itemId.calibration.frontSights) {
                            boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.ShotClinoRole)
                        } else if(itemId.calibration.backSights) {
                            boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.ShotBackCompassRole)
                        } else {
                            boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.ShotDistanceRole)
                        }
                        return itemId.model.offsetBoxIndex(boxIndex, 0);
                    }

                navigation.tabNext:
                    () => {
                        let rowIndex = clinoBackReadBox.dataValue.rowIndex
                        let boxIndex = itemId.nextLeftFromClino(rowIndex);
                        return itemId.model.offsetBoxIndex(boxIndex, 0);
                    }


                navigation.arrowRight: clinoFrontReadBox.navigation.arrowRight
                navigation.arrowLeft:
                    () => {
                        let rowIndex = clinoBackReadBox.dataValue.rowIndex
                        let boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.ShotBackCompassRole)
                        return itemId.model.offsetBoxIndex(boxIndex, 0);
                    }
                navigation.arrowDown:
                    () => {
                        let rowIndex = clinoBackReadBox.dataValue.rowIndex
                        if(itemId.calibration.frontSights) {
                            let boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.ShotClinoRole)
                            return itemId.model.offsetBoxIndex(boxIndex, 1);
                        } else {
                            let boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.ShotBackClinoRole)
                            return itemId.model.offsetBoxIndex(boxIndex, 1);
                        }
                    }
                navigation.arrowUp:
                    () => {
                        let rowIndex = clinoBackReadBox.dataValue.rowIndex
                        if(itemId.calibration.frontSights) {
                            let boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.ShotClinoRole)
                            return itemId.model.offsetBoxIndex(boxIndex, 0);
                        } else {
                            //Backsights
                            let boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.ShotBackClinoRole)
                            return itemId.model.offsetBoxIndex(boxIndex, -1);
                        }
                    }

                dataValue: shotBackClino
                listViewIndex: itemId.index
                errorButtonGroup: itemId.errorButtonGroup
                surveyChunkTrimmer: itemId.surveyChunkTrimmer
                view: itemId.ListView.view
                readingText: "bs"
                dataValidator: clinoValidator
                editorFocus: itemId.editorFocus
            }

        }
    }
}

import QtQuick
import QtQuick.Controls as QC
import cavewherelib

Item {
    id: itemId
    // objectName: "dryComponent." + index + "." + rowType
    width: 400
    height: {
        switch(rowIndex.rowType) {
        case SurveyEditorRowIndex.TitleRow:
            return titleLoaderId.item ? titleLoaderId.item.height + titleOffset : 0
        case SurveyEditorRowIndex.StationRow:
            return lastElement ? 75 : 50 - 1
        case SurveyEditorRowIndex.ShotRow:
            return 0
        }
    }

    //Visual properties
    // property SurveyEditorColumnTitles titleItem

    // height: stationVisible ? (lastElement ? 75 : 50 - 1) : titleColumnId.height + titleOffset

    readonly property bool lastElement: index == view.count - 1
    // readonly property bool skippedElement: stationName == undefined
    property TripCalibration calibration: null
    property bool canEditTripCalibration: false
    required property int index;
    required property QC.ButtonGroup errorButtonGroup
    required property SurveyChunkTrimmer surveyChunkTrimmer;
    // required property SurveyChunk chunk;
    // required property int indexInChunk;

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
    // readonly property bool stationVisible: itemId.rowIndex.rowType === SurveyEditorModel.StationRow
    // readonly property bool shotVisible: itemId.rowIndex.rowType === SurveyEditorModel.ShotRow
    required property ListView view;
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


    function errorModel(dataRole) {
        if(chunk) {
            console.log("Chunk:" + chunk + "error:" + indexInChunk + " " + dataRole + " " + chunk.errorsAt(indexInChunk, dataRole))
            return chunk.errorsAt(indexInChunk, dataRole)
        }
        return null;
    }

    // function firstVisible(items) {
    //     for(let i in items) {
    //         if(items[i].used) {
    //             return items[i].item;
    //         }
    //     }
    //     return null;
    // }

    // function boxIndex(chunkRole) {
    //     return model.boxIndex(rowType, chunk, indexInChunk, chunkRole);
    // }

    // Connections {
    //     target: itemId.chunk
    //     function onErrorsChanged(mainRole, index) {
    //         if(index === indexInChunk) {
    //             switch(mainRole) {
    //             case SurveyChunk.StationNameRole:
    //                 stationBox.errorModel = errorModel(SurveyChunk.StationNameRole);
    //                 break;
    //             case SurveyChunk.StationLeftRole:
    //                 leftBox.errorModel = errorModel(SurveyChunk.StationLeftRole);
    //                 break;
    //             case SurveyChunk.StationRightRole:
    //                 rightBox.errorModel = errorModel(SurveyChunk.StationRightRole);
    //                 break;
    //             case SurveyChunk.StationUpRole:
    //                 upBox.errorModel = errorModel(SurveyChunk.StationUpRole);
    //                 break;
    //             case SurveyChunk.StationDownRole:
    //                 downBox.errorModel = errorModel(SurveyChunk.StationDownRole);
    //                 break;
    //             case SurveyChunk.ShotDistanceRole:
    //                 distanceBox.errorModel = errorModel(SurveyChunk.ShotDistanceRole);
    //                 break;
    //             case SurveyChunk.ShotCompassRole:
    //                 stationBox.errorModel = errorModel(SurveyChunk.ShotCompassRole);
    //                 break;
    //             case SurveyChunk.ShotBackCompassRole:
    //                 stationBox.errorModel = errorModel(SurveyChunk.ShotBackCompassRole);
    //                 break;
    //             case SurveyChunk.ShotClinoRole:
    //                 stationBox.errorModel = errorModel(SurveyChunk.ShotClinoRole);
    //                 break;
    //             case SurveyChunk.ShotBackClinoRole:
    //                 stationBox.errorModel = errorModel(SurveyChunk.ShotBackClinoRole);
    //                 break;
    //             }

    //             console.log("onErrorChanged:" + mainRole + " " + index)
    //         }


    //     }
    // }

    // function focusOnStationName() {
    //     console.log("Focus on station:" + stationBox + " stationRow:" + rowType + " " +  (rowType === SurveyEditorModel.StationRow) + " " + stationBox.visible)
    //     stationBox.forceActiveFocus()
    // }

    // function hasFocus(box) {
    //     return editorFocus.chunk === box.chunk
    //     && editorFocus.indexInChunk === box.indexInChunk
    //     && editorFocus.chunkRole === box.dataRole
    // }

    // Text {
    //     z: 1
    //     text: indexInChunk + " " + stationBox.hasEditorFocus + "\n" + stationBox.objectName + "\n" + stationBox.focus
    // }

    // Component {

    // }

    // Text {
    //     color: "red"
    //     font.pixelSize: 10
    //     // text: dataBox.objectName + "\nF:" + dataBox.focus + "\nEF:" + hasEditorFocus
    //     text: itemId.index + " type:" + itemId.rowIndex.rowType + " test:" + (itemId.rowIndex.rowType === SurveyEditorRowIndex.StationRow)
    //     z: 1
    // }


    Loader {
        id: titleLoaderId
        active: itemId.rowIndex.rowType === SurveyEditorRowIndex.TitleRow
        sourceComponent: SurveyEditorColumnTitles {
            id: titleColumnId
            y: titleOffset
            // shotOffset: Math.floor(stationBox.height / 2.0);
            shotOffset: Math.floor(50.0 / 2.0);
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

                navigation.tabPrevious: {
                    let rowIndex = stationBox.dataValue.rowIndex
                    if(itemId.indexInChunk === 1) {
                        let boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.StationName)
                        return itemId.model.offsetBoxIndex(boxIndex, -1);
                    } else {
                        let boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.StationDownRole)
                        return itemId.model.offsetBoxIndex(boxIndex, -1);
                    }
                }

                    // NavigationItem {

                //     // index: {
                //     //     let current = model.

                //     //     if(itemId.dataValue.rowIndex.indexInChunk === 1) {
                //     //         current.
                //     //     }

                //     //     current.
                //     // }

                //     item: {


                //         if(itemId.indexInChunk === 1) {
                //             return stationBox
                //         }
                //         return downBox
                //     }

                //     // indexOffset: -1
                // }
                // navigation.tabPrevious: NavigationItem {

                // }

                navigation.tabNext: () => {
                        let rowIndex = stationBox.dataValue.rowIndex
                        if(rowIndex.indexInChunk === 0) {
                            let boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.StationName)
                            // console.log("BoxIndex statinoName:" + boxIndex + rowIndex)
                            return itemId.model.offsetBoxIndex(boxIndex, 1);
                        } else {
                            let boxIndex = itemId.model.boxIndex(rowIndex.chunk, rowIndex.indexInChunk - 1, SurveyEditorRowIndex.ShotRow, SurveyChunk.ShotDistanceRole)
                            // console.log("BoxIndex statinoName2:" + stationBox.objectName + " " + boxIndex + rowIndex + " " + SurveyEditorRowIndex.ShotRow)
                            return itemId.model.offsetBoxIndex(boxIndex, 0);
                        }
                    }

                    // item: {
                    //     if(itemId.indexInChunk === 0) {
                    //         //Go to the text station name
                    //         return stationBox
                    //     }
                    //     return shotDistanceDataBox;
                    // }
                    // indexOffset: {
                    //     if(itemId.indexInChunk === 0) {
                    //         return 1;
                    //     }
                    //     return -1;
                    // }
                // }
                // navigation.arrowRight: NavigationItem { item: shotDistanceDataBox1 }
                // navigation.arrowUp: NavigationItem { item: stationBox1; indexOffset: -1 }
                // navigation.arrowDown: NavigationItem { item: stationBox1; indexOffset: 1 }

                dataValue: itemId.stationName
                // visible: stationVisible
                listViewIndex: itemId.index
                // index: itemId.boxIndex(SurveyChunk.StationNameRole)
                // indexInChunk: itemId.indexInChunk
                errorButtonGroup: itemId.errorButtonGroup
                // errorModel: itemId.errorModel(SurveyChunk.StationNameRole)
                surveyChunkTrimmer: itemId.surveyChunkTrimmer
                // surveyChunk: itemId.chunk
                // rowType: itemId.rowType
                // dataRole: SurveyChunk.StationNameRole
                view: itemId.view
                dataValidator: stationValidator
                // focus: hasEditorFocus
                editorFocus: itemId.editorFocus
            }


            StationDistanceBox {
                id: leftBox
                width: itemId.columnTemplate.lWidth
                height: itemId.columnTemplate.dataRowHeight
                // anchors.left: clinoFrontReadBox.right
                // anchors.leftMargin: -1
                x: itemId.columnTemplate.leftX
                anchors.top: stationBox.top
                anchors.topMargin: 0

                navigation.tabNext: () => {
                    let rowIndex = stationBox.dataValue.rowIndex
                    let boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.StationRightRole)
                    return itemId.model.offsetBoxIndex(boxIndex, 0);
                }

                // navigation.tabPrevious: NavigationItem {
                //     item: {
                //         if(itemId.indexInChunk === 1) {
                //             return downBox
                //         } else {
                //             // return clinoFrontReadBox
                //             return firstVisible([{item:clinoBackReadBox, used: itemId.calibration.backSights},
                //                                  {item:clinoFrontReadBox, used: itemId.calibration.frontSights},
                //                                  {item:shotDistanceDataBox, used: true}
                //                                 ])
                //         }
                //     }

                //     indexOffset: {
                //         if(itemId.indexInChunk === 0) {
                //             return 1;
                //         } else {
                //             return -1;
                //         }
                //     }
                // }
                // navigation.tabNext: NavigationItem {
                //     item: rightBox
                //     indexOffset: 0
                // }

                dataValue: stationLeft
                // visible: itemId.stationVisible
                listViewIndex: itemId.index
                // index: itemId.boxIndex(SurveyChunk.StationLeftRole)
                // indexInChunk: itemId.indexInChunk
                errorButtonGroup: itemId.errorButtonGroup
                // errorModel: itemId.errorModel(SurveyChunk.StationLeftRole)

                onErrorModelChanged: {
                    console.log("Error model changed for left:" + errorModel + " " + this + " " + rowIndex)
                }

                surveyChunkTrimmer: itemId.surveyChunkTrimmer
                // surveyChunk: itemId.chunk
                // dataRole: SurveyChunk.StationLeftRole
                view: itemId.view
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

                // navigation.tabPrevious: NavigationItem {
                //     item: leftBox
                //     indexOffset: 0
                // }
                // navigation.tabNext: NavigationItem {
                //     item: upBox
                //     indexOffset: 0
                // }

                navigation.tabNext: () => {
                    let rowIndex = stationBox.dataValue.rowIndex
                    let boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.StationUpRole)
                    return itemId.model.offsetBoxIndex(boxIndex, 0);
                }


                dataValue: stationRight
                // visible: itemId.stationVisible
                listViewIndex: itemId.index
                // index: itemId.boxIndex(SurveyChunk.StationRightRole)
                // indexInChunk: itemId.indexInChunk
                errorButtonGroup: itemId.errorButtonGroup
                // errorModel: itemId.errorModel(SurveyChunk.StationRightRole)
                surveyChunkTrimmer: itemId.surveyChunkTrimmer
                // surveyChunk: itemId.chunk
                // dataRole: SurveyChunk.StationRightRole
                view: itemId.view
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

                // navigation.tabPrevious: NavigationItem {
                //     item: rightBox
                //     indexOffset: 0
                // }
                // navigation.tabNext: NavigationItem {
                //     item: downBox
                //     indexOffset: 0
                // }

                navigation.tabNext: () => {
                    let rowIndex = stationBox.dataValue.rowIndex
                    let boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.StationDownRole)
                    return itemId.model.offsetBoxIndex(boxIndex, 0);
                }

                dataValue:  stationUp
                // visible: itemId.stationVisible
                listViewIndex: itemId.index
                // index: itemId.boxIndex(SurveyChunk.StationUpRole)
                // indexInChunk: itemId.indexInChunk
                errorButtonGroup: itemId.errorButtonGroup
                // errorModel: itemId.errorModel(SurveyChunk.StationUpRole)
                surveyChunkTrimmer: itemId.surveyChunkTrimmer
                // surveyChunk: itemId.chunk
                // dataRole: SurveyChunk.StationUpRole
                view: itemId.view
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

                // navigation.tabPrevious: NavigationItem {
                //     item: upBox
                //     indexOffset: 0
                // }
                // navigation.tabNext: NavigationItem {
                //     item: {
                //         if(itemId.indexInChunk === 0) {
                //             return leftBox
                //         } else {
                //             return stationBox
                //         }
                //     }
                //     indexOffset: 1
                // }

                navigation.tabNext: () => {
                    let rowIndex = stationBox.dataValue.rowIndex
                    if(rowIndex.indexInChunk === 0) {
                        let boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.StationLeftRole)
                        return itemId.model.offsetBoxIndex(boxIndex, 1);
                    } else {
                        let boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.StationNameRole)
                        console.log("Down next tab:" + boxIndex + " " + itemId.model.offsetBoxIndex(boxIndex, 1))
                        return itemId.model.offsetBoxIndex(boxIndex, 1);
                    }
                }

                dataValue: stationDown
                // visible: itemId.stationVisible
                listViewIndex: itemId.index
                // index: itemId.boxIndex(SurveyChunk.StationDownRole)
                // indexInChunk: itemId.indexInChunk
                errorButtonGroup: itemId.errorButtonGroup
                // errorModel: itemId.errorModel(SurveyChunk.StationDownRole)
                surveyChunkTrimmer: itemId.surveyChunkTrimmer
                // surveyChunk: itemId.chunk
                // dataRole: SurveyChunk.StationDownRole
                view: itemId.view
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
                // anchors.left: stationBox.right
                // anchors.leftMargin: -1
                // anchors.verticalCenter: stationBox.top
                y: itemId.columnTemplate.shotRowY
                anchors.topMargin: 0

                // navigation.tabPrevious: NavigationItem { item: stationBox; indexOffset: 1 }
                // navigation.tabNext: NavigationItem {
                //     item: itemId.firstVisible([{item:(compassFrontReadBox), used: itemId.calibration.frontSights},
                //                                {item:(compassBackReadBox), used: itemId.calibration.backSights},
                //                                {item:(leftBox), used: true}
                //                               ])
                //     indexOffset: 0
                // }
                navigation.tabNext: () => {
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


                // navigation.arrowLeft: NavigationItem { item: stationBox1 }
                // navigation.arrowRight: NavigationItem { item: compassFrontReadBox }
                // navigation.arrowUp: NavigationItem { item: shotDistanceDataBox1; indexOffset: -1 }
                // navigation.arrowDown: NavigationItem { item: shotDistanceDataBox1; indexOffset: 1 }

                dataValue: shotDistance
                listViewIndex: itemId.index
                // index: itemId.boxIndex(SurveyChunk.ShotDistanceRole)
                // indexInChunk: itemId.indexInChunk
                errorButtonGroup: itemId.errorButtonGroup
                // errorModel: itemId.errorModel(SurveyChunk.ShotDistanceRole)
                surveyChunkTrimmer: itemId.surveyChunkTrimmer
                // surveyChunk: itemId.chunk
                // dataRole: SurveyChunk.ShotDistanceRole
                view: itemId.view
                dataValidator: distanceValidator
                editorFocus: itemId.editorFocus
            }

            CompassReadBox {
                id: compassFrontReadBox
                width: itemId.columnTemplate.compassWidth
                height: calibration.backSights ? itemId.columnTemplate.dataRowHalfHeight : itemId.columnTemplate.dataRowHeight
                anchors.left: shotDistanceDataBox.right
                anchors.leftMargin: -1
                anchors.top: shotDistanceDataBox.top
                anchors.topMargin: 0

                // navigation.tabPrevious: NavigationItem { item: shotDistanceDataBox; indexOffset: 0 }
                // navigation.tabNext: NavigationItem {
                //     item: itemId.firstVisible([
                //                                   {item:(compassBackReadBox), used: itemId.calibration.backSights},
                //                                   {item:(clinoFrontReadBox), used: itemId.calibration.frontSights},
                //                                   {item:(leftBox), used: true},
                //                               ])
                //     indexOffset: 0
                // }

                navigation.tabNext: () => {
                    let rowIndex = shotDistanceDataBox.dataValue.rowIndex
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

                visible: itemId.calibration.frontSights
                dataValue: shotCompass
                listViewIndex: itemId.index
                // index: itemId.boxIndex(SurveyChunk.ShotCompassRole)
                // indexInChunk: itemId.indexInChunk
                errorButtonGroup: itemId.errorButtonGroup
                // errorModel: itemId.errorModel(SurveyChunk.ShotCompassRole)
                surveyChunkTrimmer: itemId.surveyChunkTrimmer
                // surveyChunk: itemId.chunk
                // dataRole: SurveyChunk.ShotCompassRole
                view: itemId.view
                readingText: "fs"
                dataValidator: compassValidator
                editorFocus: itemId.editorFocus
            }

            CompassReadBox {
                id: compassBackReadBox
                width: itemId.columnTemplate.compassWidth
                height: calibration.frontSights ? itemId.columnTemplate.dataRowHalfHeight + 1 : itemId.columnTemplate.dataRowHeight
                anchors.left: compassFrontReadBox.left
                // anchors.leftMargin: -1
                anchors.top: calibration.frontSights ? shotDistanceDataBox.verticalCenter : shotDistanceDataBox.top
                anchors.topMargin: calibration.frontSights ? -1 : 0

                // navigation.tabPrevious: NavigationItem {
                //     item: itemId.firstVisible([{item:(compassFrontReadBox), used: itemId.calibration.frontSights},
                //                                {item:(shotDistanceDataBox), used: true},
                //                               ])
                //     indexOffset: 0
                // }
                // navigation.tabNext: NavigationItem {
                //     item: firstVisible([
                //                         {item:(clinoFrontReadBox), used: itemId.calibration.frontSights},
                //                         {item:(clinoBackReadBox), used: itemId.calibration.backSights},
                //                         {item:(leftBox), used: true},
                //                        ])
                //     indexOffset: 0
                // }

                navigation.tabNext: () => {
                    let rowIndex = shotDistanceDataBox.dataValue.rowIndex
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


                visible: itemId.calibration.backSights
                dataValue: shotBackCompass
                listViewIndex: itemId.index
                // index: itemId.boxIndex(SurveyChunk.ShotBackCompassRole)
                // indexInChunk: itemId.indexInChunk
                errorButtonGroup: itemId.errorButtonGroup
                // errorModel: itemId.errorModel(SurveyChunk.ShotBackCompassRole)
                surveyChunkTrimmer: itemId.surveyChunkTrimmer
                // surveyChunk: itemId.chunk
                // dataRole: SurveyChunk.ShotBackCompassRole
                view: itemId.view
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

                // navigation.tabPrevious: NavigationItem {
                //     item: itemId.firstVisible([
                //                                   {item:(compassBackReadBox), used: itemId.calibration.backSights},
                //                                {item:(compassFrontReadBox), used: itemId.calibration.frontSights},
                //                                {item:(shotDistanceDataBox), used: true},
                //                               ])
                //     indexOffset: {
                //         return 0;
                //     }
                // }
                // navigation.tabNext: NavigationItem {
                //     item: itemId.firstVisible([
                //                                   {item:(clinoBackReadBox), used: itemId.calibration.backSights},
                //                                   {item:(leftBox), used: true}
                //                               ])
                //     indexOffset: {
                //         if(itemId.indexInChunk === 0 && !itemId.calibration.backSights) {
                //             return -1;
                //         } else if (itemId.calibration.backSights) {
                //             return 0;
                //         } else {
                //             return 1;
                //         }
                //     }
                // }

                navigation.tabNext: () => {
                    let rowIndex = shotDistanceDataBox.dataValue.rowIndex
                    let boxIndex;
                    if(itemId.calibration.backSights) {
                        boxIndex = itemId.model.boxIndex(rowIndex, SurveyChunk.ShotBackClinoRole)
                    } else {
                        boxIndex = itemId.model.boxIndex(rowIndex.chunk, rowIndex.indexInChunk + 1, SurveyEditorRowIndex.StationRow, SurveyChunk.StationLeftRole)
                    }
                    return itemId.model.offsetBoxIndex(boxIndex, 0);
                }

                visible: itemId.calibration.frontSights
                dataValue: shotClino
                listViewIndex: itemId.index
                // index: itemId.boxIndex(SurveyChunk.ShotClinoRole)
                // indexInChunk: itemId.indexInChunk
                errorButtonGroup: itemId.errorButtonGroup
                // errorModel: itemId.errorModel(SurveyChunk.ShotClinoRole)
                surveyChunkTrimmer: itemId.surveyChunkTrimmer
                // surveyChunk: itemId.chunk
                // dataRole: SurveyChunk.ShotClinoRole
                view: itemId.view
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

                // navigation.tabPrevious: NavigationItem {
                //     item: itemId.firstVisible([{item:clinoFrontReadBox, used: itemId.calibration.frontSights},
                //                                {item:compassBackReadBox, used: itemId.calibration.backSights},
                //                                {item:compassFrontReadBox, used: itemId.calibration.frontSights},
                //                                {item:shotDistanceDataBox, used: true}
                //                               ])
                //     indexOffset: 0
                // }
                // navigation.tabNext: NavigationItem {
                //     item: leftBox
                //     indexOffset: {
                //         if(itemId.indexInChunk === 0) {
                //             return -1;
                //         } else {
                //             return 1;
                //         }
                //     }
                // }

                navigation.tabNext: () => {
                    let rowIndex = shotDistanceDataBox.dataValue.rowIndex
                    let boxIndex;
                    if(rowIndex.indexInChunk === 0) {
                        boxIndex = itemId.model.boxIndex(rowIndex.chunk, rowIndex.indexInChunk, SurveyEditorRowIndex.StationRow, SurveyChunk.StationLeftRole)
                    } else {
                        boxIndex = itemId.model.boxIndex(rowIndex.chunk, rowIndex.indexInChunk + 1, SurveyEditorRowIndex.StationRow, SurveyChunk.StationLeftRole)
                    }
                    return itemId.model.offsetBoxIndex(boxIndex, 0);
                }

                dataValue: shotBackClino
                listViewIndex: itemId.index
                // index: itemId.boxIndex(SurveyChunk.ShotBackClinoRole)
                // indexInChunk: itemId.indexInChunk
                errorButtonGroup: itemId.errorButtonGroup
                // errorModel: itemId.errorModel(SurveyChunk.ShotBackClinoRole)
                surveyChunkTrimmer: itemId.surveyChunkTrimmer
                // surveyChunk: itemId.chunk
                // dataRole: SurveyChunk.ShotBackClinoRole
                view: itemId.view
                readingText: "bs"
                dataValidator: clinoValidator
                editorFocus: itemId.editorFocus
            }

        }
    }


    // StationBox {
    //     id: stationBox
    //     width: titleColumnId.stationWidth
    //     height: 50

    //     navigation.tabPrevious: NavigationItem {
    //         item: {
    //             if(itemId.indexInChunk === 1) {
    //                 return stationBox
    //             }
    //             return downBox
    //         }

    //         indexOffset: -1
    //     }
    //     navigation.tabNext: NavigationItem {
    //         item: {
    //             if(itemId.indexInChunk === 0) {
    //                 //Go to the text station name
    //                 return stationBox
    //             }
    //             return shotDistanceDataBox;
    //         }
    //         indexOffset: {
    //             if(itemId.indexInChunk === 0) {
    //                 return 1;
    //             }
    //             return -1;
    //         }
    //     }
    //     // navigation.arrowRight: NavigationItem { item: shotDistanceDataBox1 }
    //     // navigation.arrowUp: NavigationItem { item: stationBox1; indexOffset: -1 }
    //     // navigation.arrowDown: NavigationItem { item: stationBox1; indexOffset: 1 }

    //     dataValue: stationName
    //     visible: stationVisible
    //     listViewIndex: itemId.index
    //     // index: itemId.boxIndex(SurveyChunk.StationNameRole)
    //     // indexInChunk: itemId.indexInChunk
    //     errorButtonGroup: itemId.errorButtonGroup
    //     // errorModel: itemId.errorModel(SurveyChunk.StationNameRole)
    //     surveyChunkTrimmer: itemId.surveyChunkTrimmer
    //     // surveyChunk: itemId.chunk
    //     // rowType: itemId.rowType
    //     // dataRole: SurveyChunk.StationNameRole
    //     view: itemId.view
    //     dataValidator: stationValidator
    //     // focus: hasEditorFocus
    //     editorFocus: itemId.editorFocus
    // }

    // ShotDistanceDataBox {
    //     id: shotDistanceDataBox
    //     width: titleColumnId.distanceWidth
    //     height: 50
    //     anchors.left: stationBox.right
    //     anchors.leftMargin: -1
    //     anchors.verticalCenter: stationBox.top
    //     anchors.topMargin: 0

    //     navigation.tabPrevious: NavigationItem { item: stationBox; indexOffset: 1 }
    //     navigation.tabNext: NavigationItem {
    //         item: itemId.firstVisible([{item:(compassFrontReadBox), used: itemId.calibration.frontSights},
    //                                    {item:(compassBackReadBox), used: itemId.calibration.backSights},
    //                                    {item:(leftBox), used: true}
    //                                   ])
    //         indexOffset: 0
    //     }

    //     // navigation.arrowLeft: NavigationItem { item: stationBox1 }
    //     // navigation.arrowRight: NavigationItem { item: compassFrontReadBox }
    //     // navigation.arrowUp: NavigationItem { item: shotDistanceDataBox1; indexOffset: -1 }
    //     // navigation.arrowDown: NavigationItem { item: shotDistanceDataBox1; indexOffset: 1 }

    //     dataValue: shotDistance
    //     visible: shotVisible
    //     listViewIndex: itemId.index
    //     // index: itemId.boxIndex(SurveyChunk.ShotDistanceRole)
    //     // indexInChunk: itemId.indexInChunk
    //     errorButtonGroup: itemId.errorButtonGroup
    //     // errorModel: itemId.errorModel(SurveyChunk.ShotDistanceRole)
    //     surveyChunkTrimmer: itemId.surveyChunkTrimmer
    //     // surveyChunk: itemId.chunk
    //     // dataRole: SurveyChunk.ShotDistanceRole
    //     view: itemId.view
    //     dataValidator: distanceValidator
    //     editorFocus: itemId.editorFocus
    // }

    // CompassReadBox {
    //     id: compassFrontReadBox
    //     width: titleColumnId.compassWidth
    //     height: calibration.backSights ? 25 : 50
    //     anchors.left: shotDistanceDataBox.right
    //     anchors.leftMargin: -1
    //     anchors.top: shotDistanceDataBox.top
    //     anchors.topMargin: 0

    //     navigation.tabPrevious: NavigationItem { item: shotDistanceDataBox; indexOffset: 0 }
    //     navigation.tabNext: NavigationItem {
    //         item: itemId.firstVisible([
    //                                       {item:(compassBackReadBox), used: itemId.calibration.backSights},
    //                                       {item:(clinoFrontReadBox), used: itemId.calibration.frontSights},
    //                                       {item:(leftBox), used: true},
    //                                   ])
    //         indexOffset: 0
    //     }

    //     visible: calibration.frontSights && itemId.shotVisible
    //     dataValue: shotCompass
    //     listViewIndex: itemId.index
    //     // index: itemId.boxIndex(SurveyChunk.ShotCompassRole)
    //     // indexInChunk: itemId.indexInChunk
    //     errorButtonGroup: itemId.errorButtonGroup
    //     // errorModel: itemId.errorModel(SurveyChunk.ShotCompassRole)
    //     surveyChunkTrimmer: itemId.surveyChunkTrimmer
    //     // surveyChunk: itemId.chunk
    //     // dataRole: SurveyChunk.ShotCompassRole
    //     view: itemId.view
    //     readingText: "fs"
    //     dataValidator: compassValidator
    //     editorFocus: itemId.editorFocus
    // }

    // CompassReadBox {
    //     id: compassBackReadBox
    //     width: titleColumnId.compassWidth
    //     height: calibration.frontSights ? 26 : 50
    //     anchors.left: compassFrontReadBox.left
    //     // anchors.leftMargin: -1
    //     anchors.top: calibration.frontSights ? shotDistanceDataBox.verticalCenter : shotDistanceDataBox.top
    //     anchors.topMargin: calibration.frontSights ? -1 : 0

    //     navigation.tabPrevious: NavigationItem {
    //         item: itemId.firstVisible([{item:(compassFrontReadBox), used: itemId.calibration.frontSights},
    //                                    {item:(shotDistanceDataBox), used: true},
    //                                   ])
    //         indexOffset: 0
    //     }
    //     navigation.tabNext: NavigationItem {
    //         item: firstVisible([
    //                             {item:(clinoFrontReadBox), used: itemId.calibration.frontSights},
    //                             {item:(clinoBackReadBox), used: itemId.calibration.backSights},
    //                             {item:(leftBox), used: true},
    //                            ])
    //         indexOffset: 0
    //     }


    //     visible: calibration.backSights && itemId.shotVisible
    //     dataValue: shotBackCompass
    //     listViewIndex: itemId.index
    //     // index: itemId.boxIndex(SurveyChunk.ShotBackCompassRole)
    //     // indexInChunk: itemId.indexInChunk
    //     errorButtonGroup: itemId.errorButtonGroup
    //     // errorModel: itemId.errorModel(SurveyChunk.ShotBackCompassRole)
    //     surveyChunkTrimmer: itemId.surveyChunkTrimmer
    //     // surveyChunk: itemId.chunk
    //     // dataRole: SurveyChunk.ShotBackCompassRole
    //     view: itemId.view
    //     readingText: "bs"
    //     dataValidator: compassValidator
    //     editorFocus: itemId.editorFocus
    // }

    // ClinoReadBox {
    //     id: clinoFrontReadBox
    //     width: titleColumnId.clinoWidth
    //     height: calibration.backSights ? 25 : 50
    //     anchors.top: shotDistanceDataBox.top
    //     anchors.topMargin: 0
    //     anchors.left: compassFrontReadBox.right
    //     anchors.leftMargin: -1

    //     navigation.tabPrevious: NavigationItem {
    //         item: itemId.firstVisible([
    //                                       {item:(compassBackReadBox), used: itemId.calibration.backSights},
    //                                    {item:(compassFrontReadBox), used: itemId.calibration.frontSights},
    //                                    {item:(shotDistanceDataBox), used: true},
    //                                   ])
    //         indexOffset: {
    //             return 0;
    //         }
    //     }
    //     navigation.tabNext: NavigationItem {
    //         item: itemId.firstVisible([
    //                                       {item:(clinoBackReadBox), used: itemId.calibration.backSights},
    //                                       {item:(leftBox), used: true}
    //                                   ])
    //         indexOffset: {
    //             if(itemId.indexInChunk === 0 && !itemId.calibration.backSights) {
    //                 return -1;
    //             } else if (itemId.calibration.backSights) {
    //                 return 0;
    //             } else {
    //                 return 1;
    //             }
    //         }
    //     }

    //     visible: calibration.frontSights && itemId.shotVisible
    //     dataValue: shotClino.value
    //     listViewIndex: itemId.index
    //     // index: itemId.boxIndex(SurveyChunk.ShotClinoRole)
    //     // indexInChunk: itemId.indexInChunk
    //     errorButtonGroup: itemId.errorButtonGroup
    //     // errorModel: itemId.errorModel(SurveyChunk.ShotClinoRole)
    //     surveyChunkTrimmer: itemId.surveyChunkTrimmer
    //     // surveyChunk: itemId.chunk
    //     // dataRole: SurveyChunk.ShotClinoRole
    //     view: itemId.view
    //     readingText: "fs"
    //     dataValidator: clinoValidator
    //     editorFocus: itemId.editorFocus
    // }

    // ClinoReadBox {
    //     id: clinoBackReadBox
    //     width: titleColumnId.clinoWidth
    //     height: calibration.frontSights ? 26 : 50
    //     anchors.topMargin: calibration.frontSights ? -1 : 0
    //     anchors.top: calibration.frontSights ? shotDistanceDataBox.verticalCenter : shotDistanceDataBox.top
    //     anchors.left: compassFrontReadBox.right
    //     anchors.leftMargin: -1
    //     visible: calibration.backSights && itemId.shotVisible

    //     navigation.tabPrevious: NavigationItem {
    //         item: itemId.firstVisible([{item:clinoFrontReadBox, used: itemId.calibration.frontSights},
    //                                    {item:compassBackReadBox, used: itemId.calibration.backSights},
    //                                    {item:compassFrontReadBox, used: itemId.calibration.frontSights},
    //                                    {item:shotDistanceDataBox, used: true}
    //                                   ])
    //         indexOffset: 0
    //     }
    //     navigation.tabNext: NavigationItem {
    //         item: leftBox
    //         indexOffset: {
    //             if(itemId.indexInChunk === 0) {
    //                 return -1;
    //             } else {
    //                 return 1;
    //             }
    //         }
    //     }

    //     dataValue: shotBackClino.value
    //     listViewIndex: itemId.index
    //     // index: itemId.boxIndex(SurveyChunk.ShotBackClinoRole)
    //     // indexInChunk: itemId.indexInChunk
    //     errorButtonGroup: itemId.errorButtonGroup
    //     // errorModel: itemId.errorModel(SurveyChunk.ShotBackClinoRole)
    //     surveyChunkTrimmer: itemId.surveyChunkTrimmer
    //     // surveyChunk: itemId.chunk
    //     // dataRole: SurveyChunk.ShotBackClinoRole
    //     view: itemId.view
    //     readingText: "bs"
    //     dataValidator: clinoValidator
    //     editorFocus: itemId.editorFocus
    // }

    // StationDistanceBox {
    //     id: leftBox
    //     width: titleColumnId.lWidth
    //     height: 50
    //     anchors.left: clinoFrontReadBox.right
    //     anchors.leftMargin: -1
    //     anchors.top: stationBox.top
    //     anchors.topMargin: 0

    //     navigation.tabPrevious: NavigationItem {
    //         item: {
    //             if(itemId.indexInChunk === 1) {
    //                 return downBox
    //             } else {
    //                 // return clinoFrontReadBox
    //                 return firstVisible([{item:clinoBackReadBox, used: itemId.calibration.backSights},
    //                                      {item:clinoFrontReadBox, used: itemId.calibration.frontSights},
    //                                      {item:shotDistanceDataBox, used: true}
    //                                    ])
    //             }
    //         }

    //         indexOffset: {
    //             if(itemId.indexInChunk === 0) {
    //                 return 1;
    //             } else {
    //                 return -1;
    //             }
    //         }
    //     }
    //     navigation.tabNext: NavigationItem {
    //         item: rightBox
    //         indexOffset: 0
    //     }

    //     dataValue: stationLeft.value
    //     visible: itemId.stationVisible
    //     listViewIndex: itemId.index
    //     // index: itemId.boxIndex(SurveyChunk.StationLeftRole)
    //     // indexInChunk: itemId.indexInChunk
    //     errorButtonGroup: itemId.errorButtonGroup
    //     // errorModel: itemId.errorModel(SurveyChunk.StationLeftRole)

    //     onErrorModelChanged: {
    //         console.log("Error model changed for left:" + errorModel + " " + this + " " + rowIndex)
    //     }

    //     surveyChunkTrimmer: itemId.surveyChunkTrimmer
    //     // surveyChunk: itemId.chunk
    //     // dataRole: SurveyChunk.StationLeftRole
    //     view: itemId.view
    //     dataValidator: distanceValidator
    //     editorFocus: itemId.editorFocus
    // }

    // StationDistanceBox {
    //     id: rightBox
    //     width: titleColumnId.rWidth
    //     height: 50
    //     anchors.topMargin: 0
    //     anchors.top: stationBox.top
    //     anchors.left: leftBox.right
    //     anchors.leftMargin: -1

    //     navigation.tabPrevious: NavigationItem {
    //         item: leftBox
    //         indexOffset: 0
    //     }
    //     navigation.tabNext: NavigationItem {
    //         item: upBox
    //         indexOffset: 0
    //     }

    //     dataValue: stationRight.value
    //     visible: itemId.stationVisible
    //     listViewIndex: itemId.index
    //     // index: itemId.boxIndex(SurveyChunk.StationRightRole)
    //     // indexInChunk: itemId.indexInChunk
    //     errorButtonGroup: itemId.errorButtonGroup
    //     // errorModel: itemId.errorModel(SurveyChunk.StationRightRole)
    //     surveyChunkTrimmer: itemId.surveyChunkTrimmer
    //     // surveyChunk: itemId.chunk
    //     // dataRole: SurveyChunk.StationRightRole
    //     view: itemId.view
    //     dataValidator: distanceValidator
    //     editorFocus: itemId.editorFocus
    // }

    // StationDistanceBox {
    //     id: upBox
    //     width: titleColumnId.uWidth
    //     height: 50
    //     anchors.topMargin: 0
    //     anchors.top: stationBox.top
    //     anchors.left: rightBox.right
    //     anchors.leftMargin: -1

    //     navigation.tabPrevious: NavigationItem {
    //         item: rightBox
    //         indexOffset: 0
    //     }
    //     navigation.tabNext: NavigationItem {
    //         item: downBox
    //         indexOffset: 0
    //     }

    //     dataValue:  stationUp.value
    //     visible: itemId.stationVisible
    //     listViewIndex: itemId.index
    //     // index: itemId.boxIndex(SurveyChunk.StationUpRole)
    //     // indexInChunk: itemId.indexInChunk
    //     errorButtonGroup: itemId.errorButtonGroup
    //     // errorModel: itemId.errorModel(SurveyChunk.StationUpRole)
    //     surveyChunkTrimmer: itemId.surveyChunkTrimmer
    //     // surveyChunk: itemId.chunk
    //     // dataRole: SurveyChunk.StationUpRole
    //     view: itemId.view
    //     dataValidator: distanceValidator
    //     editorFocus: itemId.editorFocus
    // }

    // StationDistanceBox {
    //     id: downBox
    //     width: titleColumnId.dWidth
    //     height: 50
    //     anchors.topMargin: 0
    //     anchors.top: stationBox.top
    //     anchors.left: upBox.right
    //     anchors.leftMargin: -1

    //     navigation.tabPrevious: NavigationItem {
    //         item: upBox
    //         indexOffset: 0
    //     }
    //     navigation.tabNext: NavigationItem {
    //         item: {
    //             if(itemId.indexInChunk === 0) {
    //                 return leftBox
    //             } else {
    //                 return stationBox
    //             }
    //         }
    //         indexOffset: 1
    //     }

    //     dataValue: stationDown.value
    //     visible: itemId.stationVisible
    //     listViewIndex: itemId.index
    //     // index: itemId.boxIndex(SurveyChunk.StationDownRole)
    //     // indexInChunk: itemId.indexInChunk
    //     errorButtonGroup: itemId.errorButtonGroup
    //     // errorModel: itemId.errorModel(SurveyChunk.StationDownRole)
    //     surveyChunkTrimmer: itemId.surveyChunkTrimmer
    //     // surveyChunk: itemId.chunk
    //     // dataRole: SurveyChunk.StationDownRole
    //     view: itemId.view
    //     dataValidator: distanceValidator
    //     editorFocus: itemId.editorFocus
    // }

}

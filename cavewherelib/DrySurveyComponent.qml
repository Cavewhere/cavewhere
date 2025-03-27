import QtQuick
import QtQuick.Controls as QC
import cavewherelib

Item {
    id: itemId
    // objectName: "dryComponent." + index + "." + rowType
    width: 400
    height: {
        switch(rowType) {
        case SurveyEditorModel.TitleRow:
            return titleColumnId.height + titleOffset
        case SurveyEditorModel.StationRow:
            return lastElement ? 75 : 50 - 1
        case SurveyEditorModel.ShotRow:
            return 0
        }
    }

    // height: stationVisible ? (lastElement ? 75 : 50 - 1) : titleColumnId.height + titleOffset

    readonly property bool lastElement: index == view.count - 1
    // readonly property bool skippedElement: stationName == undefined
    property TripCalibration calibration: null
    property bool canEditTripCalibration: false
    required property int index;
    required property QC.ButtonGroup errorButtonGroup
    required property SurveyChunkTrimmer surveyChunkTrimmer;
    required property SurveyChunk chunk;
    required property int indexInChunk;

    //Data that comes from the model
    required property string stationName;
    required property cwDistanceReading stationLeft;
    required property cwDistanceReading stationRight;
    required property cwDistanceReading stationUp;
    required property cwDistanceReading stationDown;
    required property cwDistanceReading shotDistance;
    required property cwCompassReading shotCompass;
    required property cwCompassReading shotBackCompass;
    required property cwClinoReading shotClino;
    required property cwClinoReading shotBackClino;
    required property int rowType;
    // required property bool titleVisible:
    readonly property bool stationVisible: itemId.rowType === SurveyEditorModel.StationRow
    readonly property bool shotVisible: itemId.rowType === SurveyEditorModel.ShotRow
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

    function firstVisible(items) {
        for(let i in items) {
            if(items[i].used) {
                return items[i].item;
            }
        }
        return null;
    }

    function boxIndex(chunkRole) {
        return model.boxIndex(rowType, chunk, indexInChunk, chunkRole);
    }

    Connections {
        target: itemId.chunk
        function onErrorsChanged(mainRole, index) {
            if(index === indexInChunk) {
                switch(mainRole) {
                case SurveyChunk.StationNameRole:
                    stationBox.errorModel = errorModel(SurveyChunk.StationNameRole);
                    break;
                case SurveyChunk.StationLeftRole:
                    leftBox.errorModel = errorModel(SurveyChunk.StationLeftRole);
                    break;
                case SurveyChunk.StationRightRole:
                    rightBox.errorModel = errorModel(SurveyChunk.StationRightRole);
                    break;
                case SurveyChunk.StationUpRole:
                    upBox.errorModel = errorModel(SurveyChunk.StationUpRole);
                    break;
                case SurveyChunk.StationDownRole:
                    downBox.errorModel = errorModel(SurveyChunk.StationDownRole);
                    break;
                case SurveyChunk.ShotDistanceRole:
                    distanceBox.errorModel = errorModel(SurveyChunk.ShotDistanceRole);
                    break;
                case SurveyChunk.ShotCompassRole:
                    stationBox.errorModel = errorModel(SurveyChunk.ShotCompassRole);
                    break;
                case SurveyChunk.ShotBackCompassRole:
                    stationBox.errorModel = errorModel(SurveyChunk.ShotBackCompassRole);
                    break;
                case SurveyChunk.ShotClinoRole:
                    stationBox.errorModel = errorModel(SurveyChunk.ShotClinoRole);
                    break;
                case SurveyChunk.ShotBackClinoRole:
                    stationBox.errorModel = errorModel(SurveyChunk.ShotBackClinoRole);
                    break;
                }

                console.log("onErrorChanged:" + mainRole + " " + index)
            }


        }
    }

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

    SurveyEditorColumnTitles {
        id: titleColumnId
        visible: rowType == SurveyEditorModel.TitleRow
        y: titleOffset
        shotOffset: Math.floor(stationBox.height / 2.0);
    }

    StationBox {
        id: stationBox
        width: titleColumnId.stationWidth
        height: 50

        navigation.tabPrevious: NavigationItem {
            item: {
                if(itemId.indexInChunk === 1) {
                    return stationBox
                }
                return downBox
            }

            indexOffset: -1
        }
        navigation.tabNext: NavigationItem {
            item: {
                if(itemId.indexInChunk === 0) {
                    //Go to the text station name
                    return stationBox
                }
                return shotDistanceDataBox;
            }
            indexOffset: {
                if(itemId.indexInChunk === 0) {
                    return 1;
                }
                return -1;
            }
        }
        // navigation.arrowRight: NavigationItem { item: shotDistanceDataBox1 }
        // navigation.arrowUp: NavigationItem { item: stationBox1; indexOffset: -1 }
        // navigation.arrowDown: NavigationItem { item: stationBox1; indexOffset: 1 }

        dataValue: stationVisible ? stationName : ""
        visible: stationVisible
        rowIndex: itemId.index
        index: itemId.boxIndex(SurveyChunk.StationNameRole)
        // indexInChunk: itemId.indexInChunk
        errorButtonGroup: itemId.errorButtonGroup
        errorModel: itemId.errorModel(SurveyChunk.StationNameRole)
        surveyChunkTrimmer: itemId.surveyChunkTrimmer
        // surveyChunk: itemId.chunk
        // rowType: itemId.rowType
        // dataRole: SurveyChunk.StationNameRole
        view: itemId.view
        dataValidator: stationValidator
        // focus: hasEditorFocus
        editorFocus: itemId.editorFocus
    }

    ShotDistanceDataBox {
        id: shotDistanceDataBox
        width: titleColumnId.distanceWidth
        height: 50
        anchors.left: stationBox.right
        anchors.leftMargin: -1
        anchors.verticalCenter: stationBox.top
        anchors.topMargin: 0

        navigation.tabPrevious: NavigationItem { item: stationBox; indexOffset: 1 }
        navigation.tabNext: NavigationItem {
            item: itemId.firstVisible([{item:(compassFrontReadBox), used: itemId.calibration.frontSights},
                                       {item:(compassBackReadBox), used: itemId.calibration.backSights},
                                       {item:(leftBox), used: true}
                                      ])
            indexOffset: 0
        }

        // navigation.arrowLeft: NavigationItem { item: stationBox1 }
        // navigation.arrowRight: NavigationItem { item: compassFrontReadBox }
        // navigation.arrowUp: NavigationItem { item: shotDistanceDataBox1; indexOffset: -1 }
        // navigation.arrowDown: NavigationItem { item: shotDistanceDataBox1; indexOffset: 1 }

        dataValue: shotVisible ? shotDistance.value : ""
        visible: shotVisible
        rowIndex: itemId.index
        index: itemId.boxIndex(SurveyChunk.ShotDistanceRole)
        // indexInChunk: itemId.indexInChunk
        errorButtonGroup: itemId.errorButtonGroup
        errorModel: itemId.errorModel(SurveyChunk.ShotDistanceRole)
        surveyChunkTrimmer: itemId.surveyChunkTrimmer
        // surveyChunk: itemId.chunk
        // dataRole: SurveyChunk.ShotDistanceRole
        view: itemId.view
        dataValidator: distanceValidator
        editorFocus: itemId.editorFocus
    }

    CompassReadBox {
        id: compassFrontReadBox
        width: titleColumnId.compassWidth
        height: calibration.backSights ? 25 : 50
        anchors.left: shotDistanceDataBox.right
        anchors.leftMargin: -1
        anchors.top: shotDistanceDataBox.top
        anchors.topMargin: 0

        navigation.tabPrevious: NavigationItem { item: shotDistanceDataBox; indexOffset: 0 }
        navigation.tabNext: NavigationItem {
            item: itemId.firstVisible([
                                          {item:(compassBackReadBox), used: itemId.calibration.backSights},
                                          {item:(clinoFrontReadBox), used: itemId.calibration.frontSights},
                                          {item:(leftBox), used: true},
                                      ])
            indexOffset: 0
        }

        visible: calibration.frontSights && itemId.shotVisible
        dataValue: itemId.shotVisible ? shotCompass.value : ""
        rowIndex: itemId.index
        index: itemId.boxIndex(SurveyChunk.ShotCompassRole)
        // indexInChunk: itemId.indexInChunk
        errorButtonGroup: itemId.errorButtonGroup
        errorModel: itemId.errorModel(SurveyChunk.ShotCompassRole)
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
        width: titleColumnId.compassWidth
        height: calibration.frontSights ? 26 : 50
        anchors.left: compassFrontReadBox.left
        // anchors.leftMargin: -1
        anchors.top: calibration.frontSights ? shotDistanceDataBox.verticalCenter : shotDistanceDataBox.top
        anchors.topMargin: calibration.frontSights ? -1 : 0

        navigation.tabPrevious: NavigationItem {
            item: itemId.firstVisible([{item:(compassFrontReadBox), used: itemId.calibration.frontSights},
                                       {item:(shotDistanceDataBox), used: true},
                                      ])
            indexOffset: 0
        }
        navigation.tabNext: NavigationItem {
            item: firstVisible([
                                {item:(clinoFrontReadBox), used: itemId.calibration.frontSights},
                                {item:(clinoBackReadBox), used: itemId.calibration.backSights},
                                {item:(leftBox), used: true},
                               ])
            indexOffset: 0
        }


        visible: calibration.backSights && itemId.shotVisible
        dataValue: itemId.shotVisible ? shotBackCompass.value : ""
        rowIndex: itemId.index
        index: itemId.boxIndex(SurveyChunk.ShotBackCompassRole)
        // indexInChunk: itemId.indexInChunk
        errorButtonGroup: itemId.errorButtonGroup
        errorModel: itemId.errorModel(SurveyChunk.ShotBackCompassRole)
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
        width: titleColumnId.clinoWidth
        height: calibration.backSights ? 25 : 50
        anchors.top: shotDistanceDataBox.top
        anchors.topMargin: 0
        anchors.left: compassFrontReadBox.right
        anchors.leftMargin: -1

        navigation.tabPrevious: NavigationItem {
            item: itemId.firstVisible([
                                          {item:(compassBackReadBox), used: itemId.calibration.backSights},
                                       {item:(compassFrontReadBox), used: itemId.calibration.frontSights},
                                       {item:(shotDistanceDataBox), used: true},
                                      ])
            indexOffset: {
                return 0;
            }
        }
        navigation.tabNext: NavigationItem {
            item: itemId.firstVisible([
                                          {item:(clinoBackReadBox), used: itemId.calibration.backSights},
                                          {item:(leftBox), used: true}
                                      ])
            indexOffset: {
                if(itemId.indexInChunk === 0 && !itemId.calibration.backSights) {
                    return -1;
                } else if (itemId.calibration.backSights) {
                    return 0;
                } else {
                    return 1;
                }
            }
        }

        visible: calibration.frontSights && itemId.shotVisible
        dataValue: itemId.shotVisible ? shotClino.value : ""
        rowIndex: itemId.index
        index: itemId.boxIndex(SurveyChunk.ShotClinoRole)
        // indexInChunk: itemId.indexInChunk
        errorButtonGroup: itemId.errorButtonGroup
        errorModel: itemId.errorModel(SurveyChunk.ShotClinoRole)
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
        width: titleColumnId.clinoWidth
        height: calibration.frontSights ? 26 : 50
        anchors.topMargin: calibration.frontSights ? -1 : 0
        anchors.top: calibration.frontSights ? shotDistanceDataBox.verticalCenter : shotDistanceDataBox.top
        anchors.left: compassFrontReadBox.right
        anchors.leftMargin: -1
        visible: calibration.backSights && itemId.shotVisible

        navigation.tabPrevious: NavigationItem {
            item: itemId.firstVisible([{item:clinoFrontReadBox, used: itemId.calibration.frontSights},
                                       {item:compassBackReadBox, used: itemId.calibration.backSights},
                                       {item:compassFrontReadBox, used: itemId.calibration.frontSights},
                                       {item:shotDistanceDataBox, used: true}
                                      ])
            indexOffset: 0
        }
        navigation.tabNext: NavigationItem {
            item: leftBox
            indexOffset: {
                if(itemId.indexInChunk === 0) {
                    return -1;
                } else {
                    return 1;
                }
            }
        }

        dataValue: itemId.shotVisible ? shotBackClino.value : ""
        rowIndex: itemId.index
        index: itemId.boxIndex(SurveyChunk.ShotBackClinoRole)
        // indexInChunk: itemId.indexInChunk
        errorButtonGroup: itemId.errorButtonGroup
        errorModel: itemId.errorModel(SurveyChunk.ShotBackClinoRole)
        surveyChunkTrimmer: itemId.surveyChunkTrimmer
        // surveyChunk: itemId.chunk
        // dataRole: SurveyChunk.ShotBackClinoRole
        view: itemId.view
        readingText: "bs"
        dataValidator: clinoValidator
        editorFocus: itemId.editorFocus
    }

    StationDistanceBox {
        id: leftBox
        width: titleColumnId.lWidth
        height: 50
        anchors.left: clinoFrontReadBox.right
        anchors.leftMargin: -1
        anchors.top: stationBox.top
        anchors.topMargin: 0

        navigation.tabPrevious: NavigationItem {
            item: {
                if(itemId.indexInChunk === 1) {
                    return downBox
                } else {
                    // return clinoFrontReadBox
                    return firstVisible([{item:clinoBackReadBox, used: itemId.calibration.backSights},
                                         {item:clinoFrontReadBox, used: itemId.calibration.frontSights},
                                         {item:shotDistanceDataBox, used: true}
                                       ])
                }
            }

            indexOffset: {
                if(itemId.indexInChunk === 0) {
                    return 1;
                } else {
                    return -1;
                }
            }
        }
        navigation.tabNext: NavigationItem {
            item: rightBox
            indexOffset: 0
        }

        dataValue: itemId.stationVisible ? stationLeft.value : ""
        visible: itemId.stationVisible
        rowIndex: itemId.index
        index: itemId.boxIndex(SurveyChunk.StationLeftRole)
        // indexInChunk: itemId.indexInChunk
        errorButtonGroup: itemId.errorButtonGroup
        errorModel: itemId.errorModel(SurveyChunk.StationLeftRole)

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
        width: titleColumnId.rWidth
        height: 50
        anchors.topMargin: 0
        anchors.top: stationBox.top
        anchors.left: leftBox.right
        anchors.leftMargin: -1

        navigation.tabPrevious: NavigationItem {
            item: leftBox
            indexOffset: 0
        }
        navigation.tabNext: NavigationItem {
            item: upBox
            indexOffset: 0
        }

        dataValue: itemId.stationVisible ? stationRight.value : ""
        visible: itemId.stationVisible
        rowIndex: itemId.index
        index: itemId.boxIndex(SurveyChunk.StationRightRole)
        // indexInChunk: itemId.indexInChunk
        errorButtonGroup: itemId.errorButtonGroup
        errorModel: itemId.errorModel(SurveyChunk.StationRightRole)
        surveyChunkTrimmer: itemId.surveyChunkTrimmer
        // surveyChunk: itemId.chunk
        // dataRole: SurveyChunk.StationRightRole
        view: itemId.view
        dataValidator: distanceValidator
        editorFocus: itemId.editorFocus
    }

    StationDistanceBox {
        id: upBox
        width: titleColumnId.uWidth
        height: 50
        anchors.topMargin: 0
        anchors.top: stationBox.top
        anchors.left: rightBox.right
        anchors.leftMargin: -1

        navigation.tabPrevious: NavigationItem {
            item: rightBox
            indexOffset: 0
        }
        navigation.tabNext: NavigationItem {
            item: downBox
            indexOffset: 0
        }

        dataValue: itemId.stationVisible ? stationUp.value : ""
        visible: itemId.stationVisible
        rowIndex: itemId.index
        index: itemId.boxIndex(SurveyChunk.StationUpRole)
        // indexInChunk: itemId.indexInChunk
        errorButtonGroup: itemId.errorButtonGroup
        errorModel: itemId.errorModel(SurveyChunk.StationUpRole)
        surveyChunkTrimmer: itemId.surveyChunkTrimmer
        // surveyChunk: itemId.chunk
        // dataRole: SurveyChunk.StationUpRole
        view: itemId.view
        dataValidator: distanceValidator
        editorFocus: itemId.editorFocus
    }

    StationDistanceBox {
        id: downBox
        width: titleColumnId.dWidth
        height: 50
        anchors.topMargin: 0
        anchors.top: stationBox.top
        anchors.left: upBox.right
        anchors.leftMargin: -1

        navigation.tabPrevious: NavigationItem {
            item: upBox
            indexOffset: 0
        }
        navigation.tabNext: NavigationItem {
            item: {
                if(itemId.indexInChunk === 0) {
                    return leftBox
                } else {
                    return stationBox
                }
            }
            indexOffset: 1
        }

        dataValue: itemId.stationVisible ? stationDown.value : ""
        visible: itemId.stationVisible
        rowIndex: itemId.index
        index: itemId.boxIndex(SurveyChunk.StationDownRole)
        // indexInChunk: itemId.indexInChunk
        errorButtonGroup: itemId.errorButtonGroup
        errorModel: itemId.errorModel(SurveyChunk.StationDownRole)
        surveyChunkTrimmer: itemId.surveyChunkTrimmer
        // surveyChunk: itemId.chunk
        // dataRole: SurveyChunk.StationDownRole
        view: itemId.view
        dataValidator: distanceValidator
        editorFocus: itemId.editorFocus
    }

}

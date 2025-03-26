import QtQuick
import QtQuick.Controls as QC
import cavewherelib

Item {
    id: itemId
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

    readonly property int titleOffset: index === 0 ? 5 : 25


    function errorModel(dataRole) {
        if(chunk) {
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
        indexInChunk: itemId.indexInChunk
        errorButtonGroup: itemId.errorButtonGroup
        errorModel: itemId.errorModel(SurveyChunk.StationNameRole)
        surveyChunkTrimmer: itemId.surveyChunkTrimmer
        surveyChunk: itemId.chunk
        dataRole: SurveyChunk.StationNameRole
        view: itemId.view
//        onTabPressed: {
//            console.log("Tab Pressed!")
//            view.currentIndex = index + 1;
//            view.currentItem.children[1].forceActiveFocus()
////            shotDistanceDataBox1.forceActiveFocus()



//        }
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
        indexInChunk: itemId.indexInChunk
        errorButtonGroup: itemId.errorButtonGroup
        errorModel: itemId.errorModel(SurveyChunk.ShotDistanceRole)
        surveyChunkTrimmer: itemId.surveyChunkTrimmer
        surveyChunk: itemId.chunk
        dataRole: SurveyChunk.ShotDistanceRole
        view: itemId.view
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
        indexInChunk: itemId.indexInChunk
        errorButtonGroup: itemId.errorButtonGroup
        errorModel: itemId.errorModel(SurveyChunk.ShotCompassRole)
        surveyChunkTrimmer: itemId.surveyChunkTrimmer
        surveyChunk: itemId.chunk
        dataRole: SurveyChunk.ShotCompassRole
        view: itemId.view
        readingText: "fs"
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
        indexInChunk: itemId.indexInChunk
        errorButtonGroup: itemId.errorButtonGroup
        errorModel: itemId.errorModel(SurveyChunk.ShotBackCompassRole)
        surveyChunkTrimmer: itemId.surveyChunkTrimmer
        surveyChunk: itemId.chunk
        dataRole: SurveyChunk.ShotBackCompassRole
        view: itemId.view
        readingText: "bs"
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
        indexInChunk: itemId.indexInChunk
        errorButtonGroup: itemId.errorButtonGroup
        errorModel: itemId.errorModel(SurveyChunk.ShotClinoRole)
        surveyChunkTrimmer: itemId.surveyChunkTrimmer
        surveyChunk: itemId.chunk
        dataRole: SurveyChunk.ShotClinoRole
        view: itemId.view
        readingText: "fs"
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
        indexInChunk: itemId.indexInChunk
        errorButtonGroup: itemId.errorButtonGroup
        errorModel: itemId.errorModel(SurveyChunk.ShotBackClinoRole)
        surveyChunkTrimmer: itemId.surveyChunkTrimmer
        surveyChunk: itemId.chunk
        dataRole: SurveyChunk.ShotBackClinoRole
        view: itemId.view
        readingText: "bs"
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
        indexInChunk: itemId.indexInChunk
        errorButtonGroup: itemId.errorButtonGroup
        errorModel: itemId.errorModel(SurveyChunk.StationLeftRole)
        surveyChunkTrimmer: itemId.surveyChunkTrimmer
        surveyChunk: itemId.chunk
        dataRole: SurveyChunk.StationLeftRole
        view: itemId.view
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
        indexInChunk: itemId.indexInChunk
        errorButtonGroup: itemId.errorButtonGroup
        errorModel: itemId.errorModel(SurveyChunk.StationLeftRole)
        surveyChunkTrimmer: itemId.surveyChunkTrimmer
        surveyChunk: itemId.chunk
        dataRole: SurveyChunk.StationRightRole
        view: itemId.view
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
        indexInChunk: itemId.indexInChunk
        errorButtonGroup: itemId.errorButtonGroup
        errorModel: itemId.errorModel(SurveyChunk.StationLeftRole)
        surveyChunkTrimmer: itemId.surveyChunkTrimmer
        surveyChunk: itemId.chunk
        dataRole: SurveyChunk.StationUpRole
        view: itemId.view
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
        indexInChunk: itemId.indexInChunk
        errorButtonGroup: itemId.errorButtonGroup
        errorModel: itemId.errorModel(SurveyChunk.StationLeftRole)
        surveyChunkTrimmer: itemId.surveyChunkTrimmer
        surveyChunk: itemId.chunk
        dataRole: SurveyChunk.StationDownRole
        view: itemId.view
    }

}

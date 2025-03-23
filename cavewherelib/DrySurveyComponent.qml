import QtQuick
import QtQuick.Controls as QC
import cavewherelib

Item {
    id: itemId
    width: 400
    height: stationVisible ? (lastElement ? 75 : 50 - 1) : titleColumnId.height + titleOffset

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
    required property bool stationVisible;
    required property bool shotVisible;

    readonly property int titleOffset: index === 0 ? 5 : 25

    function errorModel(dataRole) {
        if(chunk) {
            return chunk.errorsAt(indexInChunk, dataRole)
        }
        return null;
    }

    SurveyEditorColumnTitles {
        id: titleColumnId
        visible: !stationVisible
        y: titleOffset
        shotOffset: Math.floor(stationBox1.height / 2.0);
    }

    StationBox {
        id: stationBox1
        width: titleColumnId.stationWidth
        height: 50
        dataValue: itemId.stationVisible ? stationName : ""
        visible: itemId.stationVisible

        navigation.tabPrevious: NavigationItem { item: downBox; indexOffset: -1 }
        navigation.tabNext: NavigationItem { item: shotDistanceDataBox1; indexOffset: 1}
        navigation.arrowRight: NavigationItem { item: shotDistanceDataBox1 }
        navigation.arrowUp: NavigationItem { item: stationBox1; indexOffset: -1 }
        navigation.arrowDown: NavigationItem { item: stationBox1; indexOffset: 1 }

        rowIndex: itemId.index
        errorButtonGroup: itemId.errorButtonGroup
        errorModel: itemId.errorModel(SurveyChunk.StationNameRole)
        surveyChunkTrimmer: itemId.surveyChunkTrimmer
        surveyChunk: itemId.chunk
        dataRole: SurveyChunk.StationNameRole
//        onTabPressed: {
//            console.log("Tab Pressed!")
//            view.currentIndex = index + 1;
//            view.currentItem.children[1].forceActiveFocus()
////            shotDistanceDataBox1.forceActiveFocus()



//        }
    }

    ShotDistanceDataBox {
        id: shotDistanceDataBox1
        width: titleColumnId.distanceWidth
        height: 50
        anchors.left: stationBox1.right
        anchors.leftMargin: -1
        anchors.top: stationBox1.verticalCenter
        anchors.topMargin: 0
        dataValue: itemId.shotVisible ? shotDistance.value : ""
        visible: itemId.shotVisible
        rowIndex: itemId.index
        errorButtonGroup: itemId.errorButtonGroup
        errorModel: itemId.errorModel(SurveyChunk.ShotDistanceRole)
        surveyChunkTrimmer: itemId.surveyChunkTrimmer
        surveyChunk: itemId.chunk
        dataRole: SurveyChunk.ShotDistanceRole
    }

    CompassReadBox {
        id: compassFrontReadBox
        width: titleColumnId.compassWidth
        height: calibration.backSights ? 25 : 50
        anchors.left: shotDistanceDataBox1.right
        anchors.leftMargin: -1
        anchors.top: shotDistanceDataBox1.top
        anchors.topMargin: 0
        visible: calibration.frontSights && itemId.shotVisible
        dataValue: itemId.shotVisible ? shotCompass.value : ""
        rowIndex: itemId.index
        errorButtonGroup: itemId.errorButtonGroup
        errorModel: itemId.errorModel(SurveyChunk.ShotCompassRole)
        surveyChunkTrimmer: itemId.surveyChunkTrimmer
        surveyChunk: itemId.chunk
        dataRole: SurveyChunk.ShotCompassRole
    }

    CompassReadBox {
        id: compassBackReadBox
        width: titleColumnId.compassWidth
        height: calibration.frontSights ? 26 : 50
        anchors.left: compassFrontReadBox.left
        // anchors.leftMargin: -1
        anchors.top: calibration.frontSights ? shotDistanceDataBox1.verticalCenter : shotDistanceDataBox1.top
        anchors.topMargin: calibration.frontSights ? -1 : 0
        visible: calibration.backSights && itemId.shotVisible
        dataValue: itemId.shotVisible ? shotBackCompass.value : ""
        rowIndex: itemId.index
        errorButtonGroup: itemId.errorButtonGroup
        errorModel: itemId.errorModel(SurveyChunk.ShotBackCompassRole)
        surveyChunkTrimmer: itemId.surveyChunkTrimmer
        surveyChunk: itemId.chunk
        dataRole: SurveyChunk.ShotBackCompassRole
    }

    ClinoReadBox {
        id: clinoFrontReadBox
        width: titleColumnId.clinoWidth
        height: calibration.backSights ? 25 : 50
        anchors.top: shotDistanceDataBox1.top
        anchors.topMargin: 0
        anchors.left: compassFrontReadBox.right
        anchors.leftMargin: -1
        visible: calibration.frontSights && itemId.shotVisible
        dataValue: itemId.shotVisible ? shotClino.value : ""
        rowIndex: itemId.index
        errorButtonGroup: itemId.errorButtonGroup
        errorModel: itemId.errorModel(SurveyChunk.ShotClinoRole)
        surveyChunkTrimmer: itemId.surveyChunkTrimmer
        surveyChunk: itemId.chunk
        dataRole: SurveyChunk.ShotClinoRole
    }

    ClinoReadBox {
        id: clinoBackReadBox
        width: titleColumnId.clinoWidth
        height: calibration.frontSights ? 26 : 50
        anchors.topMargin: calibration.frontSights ? -1 : 0
        anchors.top: calibration.frontSights ? shotDistanceDataBox1.verticalCenter : shotDistanceDataBox1.top
        anchors.left: compassFrontReadBox.right
        anchors.leftMargin: -1
        visible: calibration.backSights && itemId.shotVisible
        dataValue: itemId.shotVisible ? shotBackClino.value : ""
        rowIndex: itemId.index
        errorButtonGroup: itemId.errorButtonGroup
        errorModel: itemId.errorModel(SurveyChunk.ShotBackClinoRole)
        surveyChunkTrimmer: itemId.surveyChunkTrimmer
        surveyChunk: itemId.chunk
        dataRole: SurveyChunk.ShotBackClinoRole
    }

    StationDistanceBox {
        id: leftBox
        width: titleColumnId.lWidth
        height: 50
        anchors.left: clinoFrontReadBox.right
        anchors.leftMargin: -1
        anchors.top: stationBox1.top
        anchors.topMargin: 0
        dataValue: itemId.stationVisible ? stationLeft.value : ""
        visible: itemId.stationVisible
        rowIndex: itemId.index
        errorButtonGroup: itemId.errorButtonGroup
        errorModel: itemId.errorModel(SurveyChunk.StationLeftRole)
        surveyChunkTrimmer: itemId.surveyChunkTrimmer
        surveyChunk: itemId.chunk
        dataRole: SurveyChunk.StationLeftRole
    }

    StationDistanceBox {
        id: rightBox
        width: titleColumnId.rWidth
        height: 50
        anchors.topMargin: 0
        anchors.top: stationBox1.top
        anchors.left: leftBox.right
        anchors.leftMargin: -1
        dataValue: itemId.stationVisible ? stationRight.value : ""
        visible: itemId.stationVisible
        rowIndex: itemId.index
        errorButtonGroup: itemId.errorButtonGroup
        errorModel: itemId.errorModel(SurveyChunk.StationLeftRole)
        surveyChunkTrimmer: itemId.surveyChunkTrimmer
        surveyChunk: itemId.chunk
        dataRole: SurveyChunk.StationLeftRole
    }

    StationDistanceBox {
        id: upBox
        width: titleColumnId.uWidth
        height: 50
        anchors.topMargin: 0
        anchors.top: stationBox1.top
        anchors.left: rightBox.right
        anchors.leftMargin: -1
        dataValue: itemId.stationVisible ? stationUp.value : ""
        visible: itemId.stationVisible
        rowIndex: itemId.index
        errorButtonGroup: itemId.errorButtonGroup
        errorModel: itemId.errorModel(SurveyChunk.StationLeftRole)
        surveyChunkTrimmer: itemId.surveyChunkTrimmer
        surveyChunk: itemId.chunk
        dataRole: SurveyChunk.StationLeftRole
    }

    StationDistanceBox {
        id: downBox
        width: titleColumnId.dWidth
        height: 50
        anchors.topMargin: 0
        anchors.top: stationBox1.top
        anchors.left: upBox.right
        anchors.leftMargin: -1
        dataValue: itemId.stationVisible ? stationDown.value : ""
        visible: itemId.stationVisible
        rowIndex: itemId.index
        errorButtonGroup: itemId.errorButtonGroup
        errorModel: itemId.errorModel(SurveyChunk.StationLeftRole)
        surveyChunkTrimmer: itemId.surveyChunkTrimmer
        surveyChunk: itemId.chunk
        dataRole: SurveyChunk.StationLeftRole
    }

}

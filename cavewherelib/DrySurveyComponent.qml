import QtQuick
import QtQuick.Controls as QC
import cavewherelib

Item {
    id: item1
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
    required property string stationName;
    required property double stationLeft;
    required property double stationRight;
    required property double stationUp;
    required property double stationDown;
    required property double shotDistance;
    required property double shotCompass;
    required property double shotBackCompass;
    required property double shotClino;
    required property double shotBackClino;
    required property bool stationVisible;
    required property bool shotVisible;

    property int titleOffset: 25

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
        dataValue: item1.stationVisible ? stationName : ""
        visible: item1.stationVisible

        navigation.tabPrevious: NavigationItem { item: downBox; indexOffset: -1 }
        navigation.tabNext: NavigationItem { item: shotDistanceDataBox1; indexOffset: 1}
        navigation.arrowRight: NavigationItem { item: shotDistanceDataBox1 }
        navigation.arrowUp: NavigationItem { item: stationBox1; indexOffset: -1 }
        navigation.arrowDown: NavigationItem { item: stationBox1; indexOffset: 1 }

        rowIndex: item1.index
        errorButtonGroup: item1.errorButtonGroup
        surveyChunkTrimmer: item1.surveyChunkTrimmer
        surveyChunk: item1.chunk
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
        dataValue: item1.shotVisible ? shotDistance : ""
        visible: item1.shotVisible
        rowIndex: item1.index
        errorButtonGroup: item1.errorButtonGroup
        surveyChunkTrimmer: item1.surveyChunkTrimmer
        surveyChunk: item1.chunk
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
        visible: calibration.frontSights && item1.shotVisible
        dataValue: item1.shotVisible ? shotCompass : ""
        rowIndex: item1.index
        errorButtonGroup: item1.errorButtonGroup
        surveyChunkTrimmer: item1.surveyChunkTrimmer
        surveyChunk: item1.chunk
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
        visible: calibration.backSights && item1.shotVisible
        dataValue: item1.shotVisible ? shotBackCompass : ""
        rowIndex: item1.index
        errorButtonGroup: item1.errorButtonGroup
        surveyChunkTrimmer: item1.surveyChunkTrimmer
        surveyChunk: item1.chunk
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
        visible: calibration.frontSights && item1.shotVisible
        dataValue: item1.shotVisible ? shotClino : ""
        rowIndex: item1.index
        errorButtonGroup: item1.errorButtonGroup
        surveyChunkTrimmer: item1.surveyChunkTrimmer
        surveyChunk: item1.chunk
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
        visible: calibration.backSights && item1.shotVisible
        dataValue: item1.shotVisible ? shotBackClino : ""
        rowIndex: item1.index
        errorButtonGroup: item1.errorButtonGroup
        surveyChunkTrimmer: item1.surveyChunkTrimmer
        surveyChunk: item1.chunk
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
        dataValue: item1.stationVisible ? stationLeft : ""
        visible: item1.stationVisible
        rowIndex: item1.index
        errorButtonGroup: item1.errorButtonGroup
        surveyChunkTrimmer: item1.surveyChunkTrimmer
        surveyChunk: item1.chunk
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
        dataValue: item1.stationVisible ? stationRight : ""
        visible: item1.stationVisible
        rowIndex: item1.index
        errorButtonGroup: item1.errorButtonGroup
        surveyChunkTrimmer: item1.surveyChunkTrimmer
        surveyChunk: item1.chunk
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
        dataValue: item1.stationVisible ? stationUp : ""
        visible: item1.stationVisible
        rowIndex: item1.index
        errorButtonGroup: item1.errorButtonGroup
        surveyChunkTrimmer: item1.surveyChunkTrimmer
        surveyChunk: item1.chunk
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
        dataValue: item1.stationVisible ? stationDown : ""
        visible: item1.stationVisible
        rowIndex: item1.index
        errorButtonGroup: item1.errorButtonGroup
        surveyChunkTrimmer: item1.surveyChunkTrimmer
        surveyChunk: item1.chunk
        dataRole: SurveyChunk.StationLeftRole
    }

}

import QtQuick 2.4
import Cavewhere 1.0

Item {
    id: item1
    width: 400
    height: skippedElement ? 25 : (lastElement ? 75 : 50 - 1)

    readonly property bool lastElement: index == view.count - 1
    readonly property bool skippedElement: stationName == undefined
    property Calibration calibration: null
    property bool canEditTripCalibration: false

    StationBox {
        id: stationBox1
        width: 50
        height: 50
        dataValue: stationName != undefined ? stationName : ""
        visible: stationName != undefined

        navigation.tabPrevious: NavigationItem { item: downBox; indexOffset: -1 }
        navigation.tabNext: NavigationItem { item: shotDistanceDataBox1; indexOffset: 1}
        navigation.arrowRight: NavigationItem { item: shotDistanceDataBox1 }
        navigation.arrowUp: NavigationItem { item: stationBox1; indexOffset: -1 }
        navigation.arrowDown: NavigationItem { item: stationBox1; indexOffset: 1 }

//        onTabPressed: {
//            console.log("Tab Pressed!")
//            view.currentIndex = index + 1;
//            view.currentItem.children[1].forceActiveFocus()
////            shotDistanceDataBox1.forceActiveFocus()



//        }
    }

    ShotDistanceDataBox {
        id: shotDistanceDataBox1
        width: 50
        height: 50
        anchors.left: stationBox1.right
        anchors.leftMargin: -1
        anchors.top: stationBox1.verticalCenter
        anchors.topMargin: 0
        dataValue: shotDistance != undefined ? shotDistance : ""
        visible: shotDistance != undefined
    }

    CompassReadBox {
        id: compassFrontReadBox
        width: 50
        height: calibration.backSigthts ? 25 : 50
        anchors.left: shotDistanceDataBox1.right
        anchors.leftMargin: -1
        anchors.top: shotDistanceDataBox1.top
        anchors.topMargin: 0
        visible: calibration.frontSights && shotCompass != undefined
        dataValue: shotCompass != undefined ? shotCompass : ""
    }

    CompassReadBox {
        id: compassBackReadBox
        width: 50
        height: 25
        anchors.left: compassFrontReadBox.left
        anchors.leftMargin: -1
        anchors.top: shotDistanceDataBox1.verticalCenter
        anchors.topMargin: 0
        visible: calibration.backSights && shotBackCompass != undefined
        dataValue: shotBackCompass != undefined ? shotBackCompass : ""
    }

    ClinoReadBox {
        id: clinoFrontReadBox
        width: 50
        height: calibration.backSigthts ? 25 : 50
        anchors.top: shotDistanceDataBox1.top
        anchors.topMargin: 0
        anchors.left: compassFrontReadBox.right
        anchors.leftMargin: -1
        visible: calibration.frontSights && shotCompass != undefined
        dataValue: shotClino != undefined ? shotClino : ""
    }

    ClinoReadBox {
        id: clinoBackReadBox
        width: 50
        height: 25
        anchors.topMargin: 0
        anchors.top: shotDistanceDataBox1.verticalCenter
        anchors.left: compassFrontReadBox.right
        anchors.leftMargin: -1
        visible: calibration.backSights && shotBackClino != undefined
        dataValue: shotBackClino != undefined ? shotBackClino : ""
    }

    StationDistanceBox {
        id: leftBox
        width: 50
        height: 50
        anchors.left: clinoFrontReadBox.right
        anchors.leftMargin: -1
        anchors.top: stationBox1.top
        anchors.topMargin: 0
        dataValue: stationLeft != undefined ? stationLeft : ""
        visible: stationLeft != undefined
    }

    StationDistanceBox {
        id: rightBox
        width: 50
        height: 50
        anchors.topMargin: 0
        anchors.top: stationBox1.top
        anchors.left: leftBox.right
        anchors.leftMargin: -1
        dataValue: stationRight != undefined ? stationRight : ""
        visible: stationLeft != undefined
    }

    StationDistanceBox {
        id: upBox
        width: 50
        height: 50
        anchors.topMargin: 0
        anchors.top: stationBox1.top
        anchors.left: rightBox.right
        anchors.leftMargin: -1
        dataValue: stationUp != undefined ? stationUp : ""
        visible: stationLeft != undefined
    }

    StationDistanceBox {
        id: downBox
        width: 50
        height: 50
        anchors.topMargin: 0
        anchors.top: stationBox1.top
        anchors.left: upBox.right
        anchors.leftMargin: -1
        dataValue: stationDown != undefined ? stationDown : ""
        visible: stationLeft != undefined
    }

}

import QtQuick
import QtQuick.Layouts

Item {
//    width: 400
    height: visible ? stationLabelId.height - 1 : 0

    property alias stationWidth: stationLabelId.width
    property alias distanceWidth: distanceLabelId.width
    property alias compassWidth: compassLabelId.width
    property alias clinoWidth: clinoLabelId.width
    property alias lWidth: lLabelId.width
    property alias rWidth: rLabelId.width
    property alias uWidth: uLabelId.width
    property alias dWidth: dLabelId.width
    property int shotOffset: 0

    property real columnOffset: 1

    property real stationX: 0
    property real distanceX: stationWidth - columnOffset
    property real compassX: distanceX + distanceWidth - columnOffset
    property real clinoX: compassX + compassWidth - columnOffset
    property real leftX: clinoX + clinoWidth - columnOffset

    property real dataRowHeight: 50
    property real dataRowHalfHeight: dataRowHeight * 0.5
    property real shotRowY: -dataRowHeight * 0.5

    TitleLabel {
        id: stationLabelId
        text: "Station"
    }

    TitleLabel {
        id: distanceLabelId
        anchors.left: stationLabelId.right
        anchors.leftMargin: -1
        y: shotOffset
        text: "Distance"
    }

    TitleLabel {
        id: compassLabelId
        anchors.left: distanceLabelId.right
        anchors.leftMargin: -1
        y: shotOffset
        text: "Compass"
    }

    TitleLabel {
        id: clinoLabelId
        anchors.left: compassLabelId.right
        anchors.leftMargin: -1
        y: shotOffset
        text: "Vertical\nAngle"
    }

    TitleLabel {
        id: lLabelId
        anchors.left: clinoLabelId.right
        anchors.leftMargin: -1
        text: "L"
    }

    TitleLabel {
        id: rLabelId
        anchors.left: lLabelId.right
        anchors.leftMargin: -1
        text: "R"
    }

    TitleLabel {
        id: uLabelId
        anchors.left: rLabelId.right
        anchors.leftMargin: -1
        text: "U"
    }

    TitleLabel {
        id: dLabelId
        anchors.left: uLabelId.right
        anchors.leftMargin: -1
        text: "D"
    }
}

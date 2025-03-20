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

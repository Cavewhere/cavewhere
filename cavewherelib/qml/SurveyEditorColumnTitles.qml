import QtQuick
import QtQuick.Layouts
import cavewherelib

Item {
    id: titleId
    width: 0
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

    //For displaying chunk error message correctly
    property alias chunk: chunkErrorId.chunk
    required property int listViewIndex


    RowLayout {
        id: titleRowLayoutId
        spacing: -1
        width: titleId.width > 0 ? titleId.width : implicitWidth

        TitleLabel {
            id: stationLabelId
            text: "Station"
            Layout.fillWidth: true
        }

        Item {
            Layout.fillWidth: true
            implicitWidth: distanceLabelId.implicitWidth
            height: distanceLabelId.height
            TitleLabel {
                id: distanceLabelId
                y: shotOffset
                text: "Distance"
                width: parent.width
            }
        }

        Item {
            Layout.fillWidth: true
            implicitWidth: compassLabelId.implicitWidth
            height: compassLabelId.height
            TitleLabel {
                id: compassLabelId
                y: shotOffset
                text: "Compass"
                width: parent.width
            }
        }

        Item {
            Layout.fillWidth: true
            implicitWidth: clinoLabelId.implicitWidth
            height: clinoLabelId.height
            TitleLabel {
                id: clinoLabelId
                y: shotOffset
                text: "Vertical\nAngle"
                width: parent.width
            }
        }

        TitleLabel {
            id: lLabelId
            text: "L"
            Layout.fillWidth: true
        }

        TitleLabel {
            id: rLabelId
            text: "R"
            Layout.fillWidth: true
        }

        TitleLabel {
            id: uLabelId
            text: "U"
            Layout.fillWidth: true
        }

        TitleLabel {
            id: dLabelId
            text: "D"
            Layout.fillWidth: true
        }
    }

    SurveyChunkErrorDelegate {
        id: chunkErrorId
        objectName: "chunkErrorDelegate." + titleId.listViewIndex

        y: -chunkErrorId.errorMessageHeight
        width: titleRowLayoutId.implicitWidth
        height: titleId.chunk === null ? 0 :
         titleRowLayoutId.implicitHeight
        + chunkErrorId.errorMessageHeight
    }
}

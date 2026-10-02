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
    property alias splaysWidth: splaysLabelId.width
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

    //Splay rows are read-only, so they sit tighter than the boxes above them.
    //Their cells are a pixel taller so the cluster shares the grid's overlapping
    //1 px borders the way the boxes above it do
    property real splayRowHeight: 27
    property real splayCellHeight: splayRowHeight + columnOffset

    //Where a splay row's rail and its "A1 · s1" tag sit in the station column
    property real splayRailX: 12
    property real splayRailWidth: 2
    property real splayTagIndent: 20
    property real splayTagRightMargin: 8

    //The gap between the last reading of a splay row and the ⋯ that acts on it
    property real splayMenuIndent: 6

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

        TitleLabel {
            id: splaysLabelId
            text: "Splays"
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

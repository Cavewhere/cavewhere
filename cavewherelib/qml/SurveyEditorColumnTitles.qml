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
    //The trip's Columns setting. A hidden column takes no width, so the
    //columns after it close up
    required property bool lrudVisible
    required property bool splaysVisible
    //The invisible template whose widths the data boxes use. A title row copies
    //them rather than laying out on its own, so a hidden column can't reflow
    //the titles away from the cells under them
    property SurveyEditorColumnTitles columnTemplate: null
    readonly property bool _ownsWidths: columnTemplate === null

    readonly property real lWidth: lrudVisible ? lLabelId.width : 0
    readonly property real rWidth: lrudVisible ? rLabelId.width : 0
    readonly property real uWidth: lrudVisible ? uLabelId.width : 0
    readonly property real dWidth: lrudVisible ? dLabelId.width : 0
    readonly property real splaysWidth: splaysVisible ? splaysLabelId.width : 0
    property int shotOffset: 0

    property real columnOffset: 1

    property real stationX: 0
    property real distanceX: stationWidth - columnOffset
    property real compassX: distanceX + distanceWidth - columnOffset
    property real clinoX: compassX + compassWidth - columnOffset
    property real leftX: clinoX + clinoWidth - columnOffset
    property real splaysX: lrudVisible
                           ? leftX + lWidth + rWidth + uWidth + dWidth - 4 * columnOffset
                           : leftX

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
        width: titleId.width > 0 && titleId._ownsWidths ? titleId.width : implicitWidth

        TitleLabel {
            id: stationLabelId
            objectName: "stationTitle"
            text: "Station"
            Layout.fillWidth: titleId._ownsWidths
            Layout.preferredWidth: titleId._ownsWidths ? -1 : titleId.columnTemplate.stationWidth
        }

        Item {
            Layout.fillWidth: titleId._ownsWidths
            Layout.preferredWidth: titleId._ownsWidths ? -1 : titleId.columnTemplate.distanceWidth
            implicitWidth: distanceLabelId.implicitWidth
            height: distanceLabelId.height
            TitleLabel {
                id: distanceLabelId
                objectName: "distanceTitle"
                y: shotOffset
                text: "Distance"
                width: parent.width
            }
        }

        Item {
            Layout.fillWidth: titleId._ownsWidths
            Layout.preferredWidth: titleId._ownsWidths ? -1 : titleId.columnTemplate.compassWidth
            implicitWidth: compassLabelId.implicitWidth
            height: compassLabelId.height
            TitleLabel {
                id: compassLabelId
                objectName: "compassTitle"
                y: shotOffset
                text: "Compass"
                width: parent.width
            }
        }

        Item {
            Layout.fillWidth: titleId._ownsWidths
            Layout.preferredWidth: titleId._ownsWidths ? -1 : titleId.columnTemplate.clinoWidth
            implicitWidth: clinoLabelId.implicitWidth
            height: clinoLabelId.height
            TitleLabel {
                id: clinoLabelId
                objectName: "clinoTitle"
                y: shotOffset
                text: "Vertical\nAngle"
                width: parent.width
            }
        }

        TitleLabel {
            id: lLabelId
            objectName: "lTitle"
            visible: titleId.lrudVisible
            text: "L"
            Layout.fillWidth: titleId._ownsWidths
            Layout.preferredWidth: titleId._ownsWidths ? -1 : titleId.columnTemplate.lWidth
        }

        TitleLabel {
            id: rLabelId
            objectName: "rTitle"
            visible: titleId.lrudVisible
            text: "R"
            Layout.fillWidth: titleId._ownsWidths
            Layout.preferredWidth: titleId._ownsWidths ? -1 : titleId.columnTemplate.rWidth
        }

        TitleLabel {
            id: uLabelId
            objectName: "uTitle"
            visible: titleId.lrudVisible
            text: "U"
            Layout.fillWidth: titleId._ownsWidths
            Layout.preferredWidth: titleId._ownsWidths ? -1 : titleId.columnTemplate.uWidth
        }

        TitleLabel {
            id: dLabelId
            objectName: "dTitle"
            visible: titleId.lrudVisible
            text: "D"
            Layout.fillWidth: titleId._ownsWidths
            Layout.preferredWidth: titleId._ownsWidths ? -1 : titleId.columnTemplate.dWidth
        }

        TitleLabel {
            id: splaysLabelId
            objectName: "splaysTitle"
            visible: titleId.splaysVisible
            text: "Splays"
            Layout.fillWidth: titleId._ownsWidths
            Layout.preferredWidth: titleId._ownsWidths ? -1 : titleId.columnTemplate.splaysWidth
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

import QtQuick as QQ
import QtQuick.Layouts
import cavewherelib
import "Utils.js" as Utils
import QtQuick.Controls as QC
RowLayout {
    id: itemId

    implicitHeight: rowId.height
    implicitWidth: rowId.width

    property alias helpArea: scaleLabelId.helpArea

    property alias onPaperLabel: onPaperId.text
    property alias inCaveLabel: inCaveId.text

    property alias onPaperValue: onPaperLengthInput.unitValue
    property alias inCaveValue: inCaveLengthInput.unitValue

    property bool readOnly: false
    property bool valueVisible: false
    property alias errorVisible: errorText.visible

    property double scaleValue

    // Fallback units when no scale value is bound yet, matching the project's
    // paper/cave unit pairing (Imperial → in / ft, Metric → cm / m). A bound
    // sketch scale shows its own stored units; these only surface for an unbound
    // input. cwSurveyNoteSketchModel seeds the same pairing into new sketches.
    readonly property int onPaperDefaultUnit: ProjectUnits.unitSystem === Units.Imperial
        ? Units.Inches : Units.Centimeters
    readonly property int inCaveDefaultUnit: ProjectUnits.unitSystem === Units.Imperial
        ? Units.Feet : Units.Meters

    RowLayout {
        id: rowId
        spacing: 3
        Layout.alignment: Qt.AlignHCenter

        LabelWithHelp {
            id: scaleLabelId
            text: "Scale"
            Layout.alignment: Qt.AlignVCenter
        }

        QQ.Rectangle {

            Layout.alignment: Qt.AlignVCenter
            radius: 5

            implicitWidth: childrenRect.width + columnOnPaper.x * 2.0
            implicitHeight: childrenRect.height

            color: Theme.surfaceRaised

            QQ.Column {
                id: columnOnPaper
                x: 3

                QC.Label {
                    id: onPaperId
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "On Paper"
                }

                UnitValueInput {
                    id: onPaperLengthInput
                    objectName: "onPaperLengthInput"
                    anchors.horizontalCenter: parent.horizontalCenter
                    unitValue: null
                    valueVisible: itemId.valueVisible
                    valueReadOnly: itemId.readOnly
                    defaultUnit: itemId.onPaperDefaultUnit
                }
            }
        }

        QC.Label {
            Layout.alignment: Qt.AlignVCenter
            text: "="
        }

        QQ.Rectangle {
            Layout.alignment: Qt.AlignVCenter
            radius: 5

            implicitWidth: childrenRect.width + columnInCave.x * 2.0
            implicitHeight: childrenRect.height

            color: Theme.surfaceRaised

            QQ.Column {
                id: columnInCave
                x: 3

                QC.Label {
                    id: inCaveId
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "In Cave"
                }

                UnitValueInput {
                    id: inCaveLengthInput
                    objectName: "inCaveLengthInput"
                    anchors.horizontalCenter: parent.horizontalCenter
                    unitValue: null
                    valueVisible: itemId.valueVisible
                    valueReadOnly: itemId.readOnly
                    defaultUnit: itemId.inCaveDefaultUnit
                }
            }
        }
    }


    QC.Label {
        Layout.alignment: Qt.AlignVCenter
        text: "="
    }

    QC.Label {
        id: scaleText
        Layout.alignment: Qt.AlignVCenter
        visible: !errorText.visible
        text: "1:" + Utils.fixed(1 / itemId.scaleValue, 1)
    }

    QC.Label {
        id: errorText
        color: Theme.danger
        Layout.alignment: Qt.AlignVCenter
        visible: false
        text: "Weird scaling units"
        font.italic: true
        font.bold: true
    }
}

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

                Text {
                    id: onPaperId
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "On Paper"
                }

                UnitValueInput {
                    id: onPaperLengthInput
                    anchors.horizontalCenter: parent.horizontalCenter
                    unitValue: null
                    valueVisible: itemId.valueVisible
                    valueReadOnly: itemId.readOnly
                    defaultUnit: Units.LengthUnitless
                }
            }
        }

        Text {
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

                Text {
                    id: inCaveId
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "In Cave"
                }

                UnitValueInput {
                    id: inCaveLengthInput
                    anchors.horizontalCenter: parent.horizontalCenter
                    unitValue: null
                    valueVisible: itemId.valueVisible
                    valueReadOnly: itemId.readOnly
                    defaultUnit: Units.LengthUnitless
                }
            }
        }
    }


    Text {
        Layout.alignment: Qt.AlignVCenter
        text: "="
    }

    Text {
        id: scaleText
        Layout.alignment: Qt.AlignVCenter
        visible: !errorText.visible
        text: "1:" + Utils.fixed(1 / itemId.scaleValue, 1)
    }

    Text {
        id: errorText
        color: Theme.danger
        Layout.alignment: Qt.AlignVCenter
        visible: false
        text: "Weird scaling units"
        font.italic: true
        font.bold: true
    }
}

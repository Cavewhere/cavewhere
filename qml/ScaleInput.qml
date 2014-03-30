import QtQuick 2.0
import QtQuick.Layouts 1.1
import Cavewhere 1.0
import "Utils.js" as Utils

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

    Row {
        id: rowId
        spacing: 3
        anchors.verticalCenter: parent.verticalCenter

        LabelWithHelp {
            id: scaleLabelId
            text: "Scale"
            anchors.verticalCenter: parent.verticalCenter
        }

        Rectangle {

            anchors.verticalCenter: parent.verticalCenter
            radius: 5

            width: childrenRect.width + columnOnPaper.x * 2.0
            height: childrenRect.height

            Column {
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
            anchors.verticalCenter: parent.verticalCenter
            text: "="
        }

        Rectangle {

            anchors.verticalCenter: parent.verticalCenter
            radius: 5

            width: childrenRect.width + columnInCave.x * 2.0
            height: childrenRect.height

            Column {
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
        anchors.verticalCenter: parent.verticalCenter
        text: "="
    }

    Text {
        id: scaleText
        anchors.verticalCenter: parent.verticalCenter
        visible: !errorText.visible
        text: "1:" + Utils.fixed(1 / scaleValue, 1)
    }

    Text {
        id: errorText
        color: "red"
        anchors.verticalCenter: parent.verticalCenter
        visible: false
        text: "Weird scaling units"
        font.italic: true
        font.bold: true
    }


}

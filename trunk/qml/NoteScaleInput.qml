import QtQuick 1.1
import QtDesktop 0.1 as Destkop
import Cavewhere 1.0

Item {
    property NoteTransform noteTransform
    property HelpArea scaleHelp

    signal scaleInteractionActivated()

    height: childrenRect.height
    width: inputRow.width

    Row {
        id: inputRow
        spacing: 5

        Button {
            id: setLength
            anchors.verticalCenter: parent.verticalCenter
            width: 24

            onClicked: scaleInteractionActivated()
        }

        Row {
            spacing: 3
            anchors.verticalCenter: parent.verticalCenter

            LabelWithHelp {
                id: scaleLabelId
                helpArea: scaleHelp
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
                    anchors.verticalCenter: parent.verticalCenter
                    x: 3

                    Text {
                        id: onPaperId
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "On Paper"
                    }

                    Row {
                        anchors.horizontalCenter: parent.horizontalCenter

                        ClickTextInput {
                            id: paperUnits
                            text: noteTransform.scaleNumerator.value.toFixed(0)
                            onFinishedEditting: noteTransform.scaleNumerator.value = newText
                        }

                        UnitInput {
                            unitModel: noteTransform.scaleNumerator.unitNames
                            unit: noteTransform.scaleNumerator.unit
                            onUnitChanged: noteTransform.scaleNumerator.unit = unit
                        }

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
                    anchors.verticalCenter: parent.verticalCenter

                    Text {
                        id: inCaveId
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "In Cave"
                    }


                    Row {
                        anchors.horizontalCenter: parent.horizontalCenter

                        ClickTextInput {
                            id: caveUnits
                            text: noteTransform.scaleDenominator.value.toFixed(0)
                            onFinishedEditting: noteTransform.scaleDenominator.value = newText
                        }

                        UnitInput {
                            unitModel: noteTransform.scaleDenominator.unitNames
                            unit: noteTransform.scaleDenominator.unit
                            onUnitChanged: noteTransform.scaleDenominator.unit = unit
                        }
                    }
                }
            }
        }


        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "="
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            visible: !errorText.visible
            text: "1:" + new Number((1 / noteTransform.scale).toFixed(1)).toString()
        }

        Text {
            id: errorText
            color: "red"
            anchors.verticalCenter: parent.verticalCenter
            visible: (noteTransform.scaleDenominator.unit === Units.Unitless ||
                      noteTransform.scaleNumerator.unit === Units.Unitless) &&
                     !(noteTransform.scaleDenominator.unit === Units.Unitless &&
                      noteTransform.scaleNumerator.unit === Units.Unitless)
            text: "Weird scaling units"
            font.italic: true
            font.bold: true
        }

    }


}

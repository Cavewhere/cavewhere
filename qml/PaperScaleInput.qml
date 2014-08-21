/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0
import Cavewhere 1.0
import "Utils.js" as Utils

Item {
    id: scaleInput

//    property NoteTransform noteTransform
    property Scale scaleObject;
    property HelpArea scaleHelp
    property bool autoScaling: false
    property bool usingInteraction: true

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
            visible: !autoScaling && usingInteraction
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
                        valueVisible: false
                        valueReadOnly: autoScaling
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
                        valueVisible: false
                        valueReadOnly: autoScaling
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
            text: ""
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

    states: [
        State {
            when: scaleObject !== null

            PropertyChanges {
                target: onPaperLengthInput
                unitValue: scaleObject.scaleNumerator
                valueVisible: (!autoScaling || scaleObject.scaleNumerator.unit !== Units.Unitless) && !errorText.visible
            }

            PropertyChanges {
                target: inCaveLengthInput
                unitValue: scaleObject.scaleDenominator
                valueVisible: (!autoScaling || scaleObject.scaleDenominator.unit !== Units.Unitless) && !errorText.visible
            }

            PropertyChanges {
                target: scaleText
                text: "1:" + Utils.fixed(1 / scaleObject.scale, 1)
            }

            PropertyChanges {
                target: errorText
                visible: (scaleObject.scaleDenominator.unit === Units.Unitless ||
                          scaleObject.scaleNumerator.unit === Units.Unitless) &&
                         !(scaleObject.scaleDenominator.unit === Units.Unitless &&
                          scaleObject.scaleNumerator.unit === Units.Unitless)

            }
        }

    ]

}
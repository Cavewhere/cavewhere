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

    implicitHeight: childrenRect.height
    implicitWidth: inputRow.width

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
                text: qsTr("Scale")
                anchors.verticalCenter: parent.verticalCenter
            }

            TitledRectangle {
                anchors.verticalCenter: parent.verticalCenter
                title: qsTr("On Paper")
                UnitValueInput {
                    id: onPaperLengthInput
                    anchors.horizontalCenter: parent.horizontalCenter
                    unitValue: null
                    valueVisible: false
                    valueReadOnly: autoScaling
                    defaultUnit: Units.LengthUnitless
                }
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("=")
            }

            TitledRectangle {
                anchors.verticalCenter: parent.verticalCenter
                title: qsTr("In Cave")
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


        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("=")
        }

        Text {
            id: scaleText
            anchors.verticalCenter: parent.verticalCenter
            visible: !errorText.visible
            text: ""
        }

        Text {
            id: errorText
            color: qsTr("red")
            anchors.verticalCenter: parent.verticalCenter
            visible: false
            text: qsTr("Weird scaling units")
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

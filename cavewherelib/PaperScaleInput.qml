/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0 as QQ
import cavewherelib
import "Utils.js" as Utils

QQ.Item {
    id: scaleInput

//    property NoteTransform noteTransform
    property Scale scaleObject;
    property HelpArea scaleHelp
    property bool autoScaling: false
    property bool usingInteraction: true

    signal scaleInteractionActivated()

    implicitHeight: childrenRect.height
    implicitWidth: inputRow.width

    QQ.Row {
        id: inputRow
        spacing: 5

        Button {
            id: setLength
            anchors.verticalCenter: parent.verticalCenter
            iconSource: "qrc:/icons/measurement.png"
            visible: !scaleInput.autoScaling && scaleInput.usingInteraction
            onClicked: scaleInput.scaleInteractionActivated()
        }

        QQ.Row {
            spacing: 3
            anchors.verticalCenter: parent.verticalCenter

            LabelWithHelp {
                id: scaleLabelId
                helpArea: scaleInput.scaleHelp
                text: "Scale"
            }

            TitledRectangle {
                anchors.verticalCenter: parent.verticalCenter
                title: "On Paper"
                UnitValueInput {
                    id: onPaperLengthInput
                    unitValue: null
                    valueVisible: false
                    valueReadOnly: scaleInput.autoScaling
                    defaultUnit: Units.LengthUnitless
                }
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "="
            }

            TitledRectangle {
                anchors.verticalCenter: parent.verticalCenter
                title: "In Cave"
                UnitValueInput {
                    id: inCaveLengthInput
                    unitValue: null
                    valueVisible: false
                    valueReadOnly: scaleInput.autoScaling
                    defaultUnit: Units.LengthUnitless
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
        QQ.State {
            when: scaleInput.scaleObject !== null

            QQ.PropertyChanges {
                onPaperLengthInput {
                    unitValue: scaleObject.scaleNumerator
                    valueVisible: (!autoScaling || scaleObject.scaleNumerator.unit !== Units.Unitless) && !errorText.visible
                }
            }

            QQ.PropertyChanges {
                inCaveLengthInput {
                    unitValue: scaleObject.scaleDenominator
                    valueVisible: (!autoScaling || scaleObject.scaleDenominator.unit !== Units.Unitless) && !errorText.visible
                }
            }

            QQ.PropertyChanges {
                scaleText {
                    text: "1:" + Utils.fixed(1 / scaleObject.scale, 1)
                }
            }

            QQ.PropertyChanges {
                errorText {
                    visible: (scaleObject.scaleDenominator.unit === Units.Unitless ||
                              scaleObject.scaleNumerator.unit === Units.Unitless) &&
                             !(scaleObject.scaleDenominator.unit === Units.Unitless &&
                               scaleObject.scaleNumerator.unit === Units.Unitless)
                }
            }
        }

    ]

}

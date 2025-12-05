/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib
import "Utils.js" as Utils

QQ.Item {
    id: scaleInput

//    property NoteTransform noteTransform
    property Scale scaleObject;
    property HelpArea scaleHelp
    property bool autoScaling: false
    property bool usingInteraction: true
    property alias onPaperLabel: onPaperRect.title
    property alias inCaveLabel: inCaveRect.title

    signal scaleInteractionActivated()

    implicitHeight: childrenRect.height
    implicitWidth: inputRow.width

    RowLayout {
        id: inputRow
        spacing: 5

        NoteToolIconButton {
            id: setLength
            objectName: "setLengthButton"
            iconSource: "qrc:/icons/svg/measurement.svg"
            visible: !scaleInput.autoScaling && scaleInput.usingInteraction
            onClicked: scaleInput.scaleInteractionActivated()
            toolTipText: "Find the scale tool"
        }

        RowLayout {
            spacing: 5

            LabelWithHelp {
                id: scaleLabelId
                helpArea: scaleInput.scaleHelp
                text: "Scale"
            }

            TitledRectangle {
                id: onPaperRect
                title: "On Paper"
                UnitValueInput {
                    id: onPaperLengthInput
                    objectName: "onPaperLengthInput"
                    unitValue: null
                    valueVisible: false
                    valueReadOnly: scaleInput.autoScaling
                    defaultUnit: Units.LengthUnitless
                    Layout.alignment: Qt.AlignHCenter
                }
            }

            Text {
                text: "="
            }

            TitledRectangle {
                id: inCaveRect
                title: "In Cave"
                UnitValueInput {
                    id: inCaveLengthInput
                    objectName: "inCaveLengthInput"
                    unitValue: null
                    valueVisible: false
                    valueReadOnly: scaleInput.autoScaling
                    defaultUnit: Units.LengthUnitless
                    Layout.alignment: Qt.AlignHCenter
                }
            }
        }


        Text {
            text: "="
        }

        Text {
            id: scaleText
            visible: !errorText.visible
            text: ""
        }

        Text {
            id: errorText
            color: Theme.danger
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

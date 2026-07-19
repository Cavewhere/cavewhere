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
    readonly property bool scaleValueValid: scaleObject !== null &&
                                            Number.isFinite(scaleObject.scale) &&
                                            scaleObject.scale > 0
    readonly property bool scaleUnitMismatch: scaleObject !== null &&
                                              (scaleObject.scaleDenominator.unit === Units.Unitless ||
                                               scaleObject.scaleNumerator.unit === Units.Unitless) &&
                                              !(scaleObject.scaleDenominator.unit === Units.Unitless &&
                                                scaleObject.scaleNumerator.unit === Units.Unitless)

    // Fallback units when no scaleObject is bound yet, matching the project's
    // paper/cave unit pairing (Imperial → in / ft, Metric → cm / m). A bound
    // scrap scale shows its own stored units (seeded by cwScrap::seedDefaultScale);
    // these only surface for an unbound input, mirroring ScaleInput.qml.
    readonly property int onPaperDefaultUnit: ProjectUnits.unitSystem === Units.Imperial
        ? Units.Inches : Units.Centimeters
    readonly property int inCaveDefaultUnit: ProjectUnits.unitSystem === Units.Imperial
        ? Units.Feet : Units.Meters

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
                    defaultUnit: scaleInput.onPaperDefaultUnit
                    Layout.alignment: Qt.AlignHCenter
                }
            }

            QC.Label {
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
                    defaultUnit: scaleInput.inCaveDefaultUnit
                    Layout.alignment: Qt.AlignHCenter
                }
            }
        }


        QC.Label {
            text: "="
        }

        QC.Label {
            id: scaleText
            visible: !errorText.visible
            text: ""
        }

        QC.Label {
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
                    valueVisible: (!autoScaling || scaleObject.scaleNumerator.unit !== Units.Unitless)
                }
            }

            QQ.PropertyChanges {
                inCaveLengthInput {
                    unitValue: scaleObject.scaleDenominator
                    valueVisible: (!autoScaling || scaleObject.scaleDenominator.unit !== Units.Unitless)
                }
            }

            QQ.PropertyChanges {
                scaleText {
                    text: scaleInput.scaleValueValid
                          ? "1:" + Utils.fixed(1 / scaleObject.scale, 1)
                          : ""
                }
            }

            QQ.PropertyChanges {
                errorText {
                    visible: scaleInput.scaleUnitMismatch || !scaleInput.scaleValueValid
                    text: !scaleInput.scaleValueValid
                          ? "Invalid scale (check DPI)"
                          : "Weird scaling units"
                }
            }
        }

    ]

}

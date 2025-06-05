/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// import QtQuick as QQ // to target S60 5th Edition or Maemo 5
import QtQuick as QQ
import cavewherelib
import "Utils.js" as Utils

GroupBox {
    id: editorId

    property TripCalibration calibration

    anchors.margins: 3
    contentHeight: tapeContent.height
    text: "Distance"

    QQ.Column {
        id: tapeContent
        anchors.left: parent.left
        anchors.right: parent.right

        spacing: 3

        //Tape calibration
        QQ.Row {

            spacing: 15

            QQ.Row {
                spacing: 3

                LabelWithHelp {
                    id: tapeCalibrationLabel
                    text: "Calibration"
                    helpArea: tapeCalibarationHelpArea
                }

                ClickTextInput {
                    id: tapeCalInput
                    text: Utils.fixed(editorId.calibration.tapeCalibration, 2)

                    onFinishedEditting: (newText) => {
                        editorId.calibration.tapeCalibration = newText
                    }
                }
            }

            QQ.Row {
                LabelWithHelp {
                    id: tapeUnits
                    text: "Units"
                    helpArea: tapeUnitsHelpArea
                }

                UnitInput {
                    id: lengthUnits
                    unitModel: {
                        if(editorId.calibration !== null) {
                            return editorId.calibration.supportedUnits;
                        } else {
                            return null
                        }
                    }

                    unit: {
                        if(editorId.calibration !== null) {
                            return editorId.calibration.mapToSupportUnit(editorId.calibration.distanceUnit);
                        } else {
                            return Units.LengthUnitless
                        }
                    }

                    onNewUnit: (unit) => {
                        if(editorId.calibration !== null) {
                            editorId.calibration.distanceUnit = editorId.calibration.mapToLengthUnit(unit)
                        }
                    }
                }
            }
        }

        HelpArea {
            id: tapeCalibarationHelpArea
            text: "<p>Distance calibration allow you to correct a tape that's too long" +
"or too short.  The calibration is added to uncorrected value (the value you read off the tape) to find the true value.</p>" +
"<b>UncorrectedValue + Calibration = TrueValue</b>" +
"<p> For example, the reading in the cave was 10m. You have a tape that's a 1m short. The true measured length" +
"is 9m.  So you would enter -1 for the calibration. UncorrectedValue = 10m, Calibration = -1m, so 10m + (-1m) = 9m </p>"

            anchors.left: parent.left
            anchors.right: parent.right
        }

        HelpArea {
            id: tapeUnitsHelpArea
            text: "Tape's measurement units: meters (m) or feet (ft)"
            anchors.left: parent.left
            anchors.right: parent.right
        }
    }
}

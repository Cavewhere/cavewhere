/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// import QtQuick 2.0 // to target S60 5th Edition or Maemo 5
import QtQuick 2.0
import Cavewhere 1.0
import "Utils.js" as Utils

GroupBox {

    property Calibration calibration

    anchors.margins: 3
    contentHeight: tapeContent.height
    text: qsTr("Distance")

    Column {
        id: tapeContent
        anchors.left: parent.left
        anchors.right: parent.right

        spacing: 3

        //Tape calibration
        Row {

            spacing: 15

            Row {
                spacing: 3

                LabelWithHelp {
                    id: tapeCalibrationLabel
                    text: qsTr("Calibration")
                    helpArea: tapeCalibarationHelpArea
                }

                ClickTextInput {
                    id: tapeCalInput
                    text: Utils.fixed(calibration.tapeCalibration, 2)

                    onFinishedEditting: {
                        calibration.tapeCalibration = newText
                    }
                }
            }

            Row {
                LabelWithHelp {
                    id: tapeUnits
                    text: qsTr("Units")
                    helpArea: tapeUnitsHelpArea
                }

                UnitInput {
                    id: lengthUnits
                    unitModel: {
                        if(calibration !== null) {
                            return calibration.supportedUnits;
                        } else {
                            return null
                        }
                    }

                    unit: {
                        if(calibration !== null) {
                            return calibration.mapToSupportUnit(calibration.distanceUnit);
                        } else {
                            return Units.Unitless
                        }
                    }

                    onNewUnit: {
                        if(calibration !== null) {
                            calibration.distanceUnit = calibration.mapToLengthUnit(unit)
                        }
                    }
                }
            }
        }

        HelpArea {
            id: tapeCalibarationHelpArea
            text: qsTr("<p>Distance calibration allow you to correct a tape that's too long
or too short.  The calibration is added to uncorrected value (the value you read off the tape) to find the true value.</p>
<b>UncorrectedValue + Calibration = TrueValue</b>
<p> For example, the reading in the cave was 10m. You have a tape that's a 1m short. The true measured length
is 9m.  So you would enter -1 for the calibration. UncorrectedValue = 10m, Calibration = -1m, so 10m + (-1m) = 9m </p>")

            anchors.left: parent.left
            anchors.right: parent.right
        }

        HelpArea {
            id: tapeUnitsHelpArea
            text: qsTr("Tape's measurement units: meters (m) or feet (ft)")
            anchors.left: parent.left
            anchors.right: parent.right
        }
    }
}

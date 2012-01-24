// import QtQuick 1.0 // to target S60 5th Edition or Maemo 5
import QtQuick 1.1
import Cavewhere 1.0
import "Utils.js" as Utils

GroupBox {

    property Calibration calibration

    anchors.margins: 3
    contentHeight: tapeContent.height
    text: "Distance"

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
                    text: "Calibration"
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
                    text: "Units"
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
            text: "Tape calibration help"
            anchors.left: parent.left
            anchors.right: parent.right
        }

        HelpArea {
            id: tapeUnitsHelpArea
            text: "Tape units help"
            anchors.left: parent.left
            anchors.right: parent.right
        }
    }
}

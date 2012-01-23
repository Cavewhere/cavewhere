// import QtQuick 1.0 // to target S60 5th Edition or Maemo 5
import QtQuick 1.1
import QtDesktop 0.1 as Desktop
import Cavewhere 1.0
import "Utils.js" as Utils

CheckableGroupBox {

    property Calibration calibration

    anchors.left: parent.left
    anchors.right: parent.right
    anchors.margins: 3
    contentHeight: frontSightContent.height
    text: checked ? "<b>Back Sights</b>" : "Back Sights"

    Column {
        id: frontSightContent
        anchors.left: parent.left
        anchors.right: parent.right

        Row {
            spacing: 15

            Row {
                spacing: 3

                LabelWithHelp {
                    id: compassCalibrationLabel
                    text: "Compass calibration"
                    helpArea: clinoCalibarationHelpArea
                }

                ClickTextInput {
                    id: clinoCalInput
                    text: Utils.fixed(calibration.backCompassCalibration, 2)

                    onFinishedEditting: {
                        calibration.backCompassCalibration = newText
                    }
                }
            }

            Row {
                spacing: 3

                LabelWithHelp {
                    id: clinoCalibrationLabel
                    text: "Clino calibration"
                    helpArea: compassCalibarationHelpArea
                }

                ClickTextInput {
                    id: compassCalInput
                    text: Utils.fixed(calibration.backClinoCalibration, 2)

                    onFinishedEditting: {
                        calibration.backClinoCalibration = newText
                    }
                }
            }
        }

        HelpArea {
            id: compassCalibarationHelpArea
            anchors.left: parent.left
            anchors.right: parent.right
            text: "Help text for the compass calibration"
        }

        HelpArea {
            id: clinoCalibarationHelpArea
            anchors.left: parent.left
            anchors.right: parent.right
            text: "Help text for the clino calibration"
        }

        Column {
            spacing: 3
            Desktop.CheckBox {
                id: compassCorrected
                text: "Corrected <i>Compass</i> Back Sights"
            }

            Desktop.CheckBox {
                id: clinoCorrected
                text: "Corrected <i>Clino</i> Back Sights"
            }
        }

        HelpArea {
            id: correctedCompassHelpArea
            anchors.left: parent.left
            anchors.right: parent.right
            text: "Help text for the corrected compass calibration"
        }

        HelpArea {
            id: correctedClinoHelpArea
            anchors.left: parent.left
            anchors.right: parent.right
            text: "Help text for the correct clino calibration"
        }
    }
}

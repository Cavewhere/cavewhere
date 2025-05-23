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

CheckableGroupBox {
    id: calibrationEditor
    objectName: "frontSightCalibrationEditor"

    property TripCalibration calibration

    anchors.margins: 3
    text: "<b>Front Sights</b>"
    checked: calibration.frontSights

    onCalibrationChanged: {
        checked = calibration.frontSights
    }

    onCheckedChanged: {
        calibration.frontSights = checked
    }

    QQ.Column {
        id: frontSightContent
        anchors.left: parent.left
        anchors.right: parent.right

        QQ.Row {
            spacing: 3

            LabelWithHelp {
                id: compassCalibrationLabel
                text: "Compass calibration"
                helpArea: compassCalibarationHelpArea
            }

            ClickTextInput {
                id: clinoCalInput
                text: Utils.fixed(calibrationEditor.calibration.frontCompassCalibration, 2)

                onFinishedEditting: (newText) => {
                    calibrationEditor.calibration.frontCompassCalibration = newText
                }
            }
        }

        HelpArea {
            id: compassCalibarationHelpArea
            anchors.left: parent.left
            anchors.right: parent.right
            text: "<p>Front sight compass calibration allows you to correct an instrument that's off." +
"The calibration is added to uncorrected value" +
"(the value you read off the instrument) to find the true value.</p>" +
"<b>UncorrectedValue + Calibration = TrueValue</b>" +
"<p> For example, the reading in the cave was 180°. Your instrument is off by -2°. The bearing is really" +
"182° instead of 180 (because your insturment is subtracting 2° at every shot).  So you need to enter 2" +
"for the calibration to correct it. UncorrectedValue = 180°," +
"Calibration = 2°, so 180° + (2°) = 182° </p>"
        }

        QQ.Row {
            spacing: 3

            LabelWithHelp {
                id: clinoCalibrationLabel
                text: "Clino calibration"
                helpArea: clinoCalibarationHelpArea
            }

            ClickTextInput {
                id: compassCalInput
                text: Utils.fixed(calibrationEditor.calibration.frontClinoCalibration, 2)

                onFinishedEditting: (newText) => {
                    calibrationEditor.calibration.frontClinoCalibration = newText
                }
            }
        }



        HelpArea {
            id: clinoCalibarationHelpArea
            anchors.left: parent.left
            anchors.right: parent.right
            text: "<p>Front sight clino calibration allows you to correct an instrument that's off." +
"The calibration is added to uncorrected value" +
"(the value you read off the instrument) to find the true value.</p>" +
"<b>UncorrectedValue + Calibration = TrueValue</b>" +
"<p> For example, the reading in the cave was +4°. Your instrument is off by +1°. The bearing is really" +
"+3° instead of +4° (because your insturment is adding extra 1° at every shot).  So you need to enter -1" +
"for the calibration to correct it. UncorrectedValue = +4°," +
"Calibration = -1°, so +4° + (-1°) = +3° </p>"
        }

        BreakLine {}

        QQ.Row {
            InformationButton {
                anchors.verticalCenter: parent.verticalCenter
                onClicked: {
                    correctedCompassHelpArea.visible = !correctedCompassHelpArea.visible
                }
            }

            CheckBox {
                id: compassCorrected
                text: "Backwards <i>Compass</i>"

                checked: calibrationEditor.calibration.correctedCompassFrontsight

                onCheckedChanged: {
                    calibrationEditor.calibration.correctedCompassFrontsight = checked
                }
            }
        }


        HelpArea {
            id: correctedCompassHelpArea
            anchors.left: parent.left
            anchors.right: parent.right
            text: "Read backwards compass allow you to entry front sight as if they were read as" +
            "a back sight.  This will <b>subtract 180°</b> to all front sight compass readings to get" +
            "the true value."
        }

        QQ.Row {
            InformationButton {
                anchors.verticalCenter: parent.verticalCenter
                onClicked: {
                    correctedClinoHelpArea.visible = !correctedClinoHelpArea.visible
                }
            }

            CheckBox {
                id: clinoCorrected
                text: "Backwards <i>Clino</i>"

                checked: calibrationEditor.calibration.correctedClinoFrontsight

                onCheckedChanged: {
                    calibrationEditor.calibration.correctedClinoFrontsight = checked
                }
            }
        }

        HelpArea {
            id: correctedClinoHelpArea
            anchors.left: parent.left
            anchors.right: parent.right
            text: "Read backwards clino allow you to entry front sights as if they were read as" +
            "a back sight.  This will <b>multiple -1</b> to all front sight clino readings to get" +
            "the true value."
        }
    }

}

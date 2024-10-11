/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// import QtQuick 2.0 as QQ // to target S60 5th Edition or Maemo 5
import QtQuick as QQ
// import QtQuick.Controls 2.0 as QC
import cavewherelib
import "Utils.js" as Utils

CheckableGroupBox {
    id: editorId

    property TripCalibration calibration

    anchors.margins: 3
    text: "<b>Back Sights</b>"
    checked: calibration.backSights

    onCalibrationChanged: {
        checked = calibration.backSights
        compassCorrected.checked = calibration.correctedCompassBacksight
        clinoCorrected.checked = calibration.correctedClinoBacksight
    }

    onCheckedChanged: {
        calibration.backSights = checked
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
                text: Utils.fixed(editorId.calibration.backCompassCalibration, 2)

                onFinishedEditting: (newText) => {
                    editorId.calibration.backCompassCalibration = newText
                }
            }
        }

        HelpArea {
            id: compassCalibarationHelpArea
            anchors.left: parent.left
            anchors.right: parent.right
            text: "<p>Back sight compass calibration allows you to correct an instrument that's off." +
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
                text: Utils.fixed(editorId.calibration.backClinoCalibration, 2)

                onFinishedEditting: (newText) => {
                    editorId.calibration.backClinoCalibration = newText
                }
            }
        }

        HelpArea {
            id: clinoCalibarationHelpArea
            anchors.left: parent.left
            anchors.right: parent.right
            text: "Back sight clino calibration allows you to correct an instrument that's off." +
            "The calibration is added to uncorrected value" +
            "(the value you read off the instrument) to find the true value." +
            "UncorrectedValue + Calibration = TrueValue" +
            "For example, the reading in the cave was +4°." +
            "Your instrument is off by +1°." +
            "The bearing is really +3° instead of +4°" +
            "(because your instrument is adding an extra 1° at every shot)." +
            "So you need to enter -1 for the calibration to correct it." +
            "UncorrectedValue = +4°, Calibration = -1°, so +4° + (-1°) = +3°"
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
                text: "Corrected <i>Compass</i>"

                checked: editorId.calibration.correctedCompassBacksight

                onCheckedChanged: {
                    editorId.calibration.correctedCompassBacksight = checked
                }
            }
        }


        HelpArea {
            id: correctedCompassHelpArea
            anchors.left: parent.left
            anchors.right: parent.right
            text: "Corrected compass allow you to entry back sights as if they were read as" +
            "a front sight.  This will <b>subtract 180°</b> to all back sight compass readings to get" +
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
                text: "Corrected <i>Clino</i>"

                checked: editorId.calibration.correctedClinoBacksight

                onCheckedChanged: {
                    editorId.calibration.correctedClinoBacksight = checked
                }
            }
        }

        HelpArea {
            id: correctedClinoHelpArea
            anchors.left: parent.left
            anchors.right: parent.right
            text: "Corrected clino allow you to entry back sights as if they were read as" +
            "a front sight.  This will <b>multiple -1</b> to all back sight clino readings to get" +
            "the true value."
        }
    }
}

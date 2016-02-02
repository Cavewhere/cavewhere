/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// import QtQuick 2.0 // to target S60 5th Edition or Maemo 5
import QtQuick 2.0
import QtQuick.Controls 1.0 as Controls
import Cavewhere 1.0
import "Utils.js" as Utils

CheckableGroupBox {

    property Calibration calibration

    anchors.margins: 3
    contentHeight: frontSightContent.height
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

    Column {
        id: frontSightContent
        anchors.left: parent.left
        anchors.right: parent.right

        Row {
            spacing: 3

            LabelWithHelp {
                id: compassCalibrationLabel
                text: "Compass calibration"
                helpArea: compassCalibarationHelpArea
            }

            ClickTextInput {
                id: clinoCalInput
                text: Utils.fixed(calibration.backCompassCalibration, 2)

                onFinishedEditting: {
                    calibration.backCompassCalibration = newText
                }
            }
        }

        HelpArea {
            id: compassCalibarationHelpArea
            anchors.left: parent.left
            anchors.right: parent.right
            text: "<p>Back sight compass calibration allows you to correct an instrument that's off.
The calibration is added to uncorrected value
(the value you read off the instrument) to find the true value.</p>
<b>UncorrectedValue + Calibration = TrueValue</b>
<p> For example, the reading in the cave was 180°. Your instrument is off by -2°. The bearing is really
182° instead of 180 (because your insturment is subtracting 2° at every shot).  So you need to enter 2
for the calibration to correct it. UncorrectedValue = 180°,
Calibration = 2°, so 180° + (2°) = 182° </p>"
        }

        Row {
            spacing: 3

            LabelWithHelp {
                id: clinoCalibrationLabel
                text: "Clino calibration"
                helpArea: clinoCalibarationHelpArea
            }

            ClickTextInput {
                id: compassCalInput
                text: Utils.fixed(calibration.backClinoCalibration, 2)

                onFinishedEditting: {
                    calibration.backClinoCalibration = newText
                }
            }
        }

        HelpArea {
            id: clinoCalibarationHelpArea
            anchors.left: parent.left
            anchors.right: parent.right
            text: "<p>Back sight clino calibration allows you to correct an instrument that's off.
The calibration is added to uncorrected value
(the value you read off the instrument) to find the true value.</p>
<b>UncorrectedValue + Calibration = TrueValue</b>
<p> For example, the reading in the cave was +4°. Your instrument is off by +1°. The bearing is really
+3° instead of +4° (because your insturment is adding extra 1° at every shot).  So you need to enter -1
for the calibration to correct it. UncorrectedValue = +4°,
Calibration = -1°, so +4° + (-1°) = +3° </p>"
        }

        BreakLine {}

        Row {
            InformationButton {
                anchors.verticalCenter: parent.verticalCenter
                onClicked: {
                    correctedCompassHelpArea.visible = !correctedCompassHelpArea.visible
                }
            }

            Controls.CheckBox {
                id: compassCorrected
                text: "Corrected <i>Compass</i>"

                checked: calibration.correctedCompassBacksight

                onCheckedChanged: {
                    calibration.correctedCompassBacksight = checked
                }
            }
        }


        HelpArea {
            id: correctedCompassHelpArea
            anchors.left: parent.left
            anchors.right: parent.right
            text: "Corrected compass allow you to entry back sights as if they were read as
            a front sight.  This will <b>subtract 180°</b> to all back sight compass readings to get
            the true value."
        }

        Row {
            InformationButton {
                anchors.verticalCenter: parent.verticalCenter
                onClicked: {
                    correctedClinoHelpArea.visible = !correctedClinoHelpArea.visible
                }
            }

            Controls.CheckBox {
                id: clinoCorrected
                text: "Corrected <i>Clino</i>"

                checked: calibration.correctedClinoBacksight

                onCheckedChanged: {
                    calibration.correctedClinoBacksight = checked
                }
            }
        }

        HelpArea {
            id: correctedClinoHelpArea
            anchors.left: parent.left
            anchors.right: parent.right
            text: "Corrected clino allow you to entry back sights as if they were read as
            a front sight.  This will <b>multiple -1</b> to all back sight clino readings to get
            the true value."
        }
    }
}

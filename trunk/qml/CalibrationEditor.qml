// import QtQuick 1.0 // to target S60 5th Edition or Maemo 5
import QtQuick 1.1
import Cavewhere 1.0
import "Utils.js" as Utils

Rectangle {
    id: calibrationEditor
    property Calibration calibration

//    color: style.floatingWidgetColor
    radius: 8
    height: childrenRect.height

    Style {
        id: style
    }

    Column {

        anchors.left: parent.left
        anchors.right: parent.right

        spacing: 5

        SectionLabel {
            text: "Calibration"
        }

        Row {

            anchors.left: parent.left
            anchors.leftMargin: 6

            spacing: 3

            LabelWithHelp {
                id: declination
                text: "Declination"
                helpArea: declinationHelp
            }

            ClickTextInput {
                id: tapeCalInput
                text: Utils.fixed(calibration.declination, 2)

                onFinishedEditting: {
                    calibration.declination = newText
                }
            }
        }

        HelpArea {
            id: declinationHelp
            text: "Declination help"
            anchors.left: parent.left
            anchors.right: parent.right
        }

        TapeCalibrationEditor {
            calibration: calibrationEditor.calibration
        }

        FrontSightCalibrationEditor {
            id: frontSightCalibrationEditor
            calibration: calibrationEditor.calibration
            contentsVisible: checked
        }

        BackSightCalibrationEditor {
            id: backSightCalibrationEditor
            calibration: calibrationEditor.calibration
            contentsVisible: checked
        }
    }
}

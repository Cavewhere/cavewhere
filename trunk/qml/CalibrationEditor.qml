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

        Item {
            anchors.left: parent.left
            anchors.right: parent.right
            height: childrenRect.height

            DeclainationEditor {
                calibration: calibrationEditor.calibration
                anchors.left: parent.left
                anchors.right: parent.horizontalCenter
            }

            //            Column {
            //                anchors.left: parent.left
            //                anchors.right: parent.horizontalCenter
            //                anchors.top: parent.top
            //                anchors.topMargin: 15

            //                Row {
            //                    spacing: 3

            //                    anchors.horizontalCenter: parent.horizontalCenter

            //                    LabelWithHelp {
            //                        id: declination
            //                        text: "Declination"
            //                        helpArea: declinationHelp
            //                    }

            //                    ClickTextInput {
            //                        id: tapeCalInput
            //                        text: Utils.fixed(calibration.declination, 2)

            //                        onFinishedEditting: {
            //                            calibration.declination = newText
            //                        }
            //                    }


            //                }

            //                HelpArea {
            //                    id: declinationHelp
            //                    text: "Declination help"
            //                    anchors.left: parent.left
            //                    anchors.right: parent.right
            //                }
            //            }

            TapeCalibrationEditor {
                id: tapeEditor
                calibration: calibrationEditor.calibration
                anchors.left: parent.horizontalCenter
                anchors.right: parent.right
            }
        }




        Item {
            anchors.left: parent.left
            anchors.right: parent.right

            height: childrenRect.height

            FrontSightCalibrationEditor {
                id: frontSightCalibrationEditor
                calibration: calibrationEditor.calibration
                contentsVisible: checked

                anchors.left: parent.left
                anchors.right: parent.horizontalCenter
            }

            BackSightCalibrationEditor {
                id: backSightCalibrationEditor
                calibration: calibrationEditor.calibration
                contentsVisible: checked

                anchors.left: parent.horizontalCenter
                anchors.right: parent.right
            }
        }
    }
}

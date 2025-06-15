/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// import QtQuick as QQ // to target S60 5th Edition or Maemo 5
import QtQuick as QQ
import QtQuick.Layouts
import cavewherelib

QQ.Rectangle {
    id: calibrationEditor
    property TripCalibration calibration

    radius: 8
    implicitHeight: childrenRect.height

    ColumnLayout {
        anchors.left: parent.left
        anchors.right: parent.right

        spacing: 5

        SectionLabel {
            text: "Calibration"
            Layout.alignment: Qt.AlignHCenter
        }

        QQ.Item {
            Layout.fillWidth: true;
            implicitHeight: childrenRect.height

            DeclainationEditor {
                calibration: calibrationEditor.calibration
                anchors.left: parent.left
                anchors.right: parent.horizontalCenter
            }

            TapeCalibrationEditor {
                id: tapeEditor
                calibration: calibrationEditor.calibration
                anchors.left: parent.horizontalCenter
                anchors.right: parent.right
            }
        }


        QQ.Item {
            Layout.fillWidth: true

            implicitHeight: childrenRect.height

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

        ErrorHelpArea {
            Layout.fillWidth: true

            visible: !backSightCalibrationEditor.checked && !frontSightCalibrationEditor.checked
            animationToInvisible: false
            text: "Hmm, you need to <b>check</b> either <i>front</i> or <i>back sights</i> box, or both, depending on your data."
        }
    }
}

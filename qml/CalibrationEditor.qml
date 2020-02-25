/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// import QtQuick 2.0 as QQ // to target S60 5th Edition or Maemo 5
import QtQuick 2.0 as QQ
import Cavewhere 1.0
import "Utils.js" as Utils
import "Theme.js" as Theme

QQ.Rectangle {
    id: calibrationEditor
    property Calibration calibration

    radius: 8
    height: childrenRect.height

    QQ.Column {

        anchors.left: parent.left
        anchors.right: parent.right

        spacing: 5

        SectionLabel {
            text: "Calibration"
        }

        QQ.Item {
            anchors.left: parent.left
            anchors.right: parent.right
            height: childrenRect.height

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

        ErrorHelpArea {
            anchors.left: parent.left
            anchors.right: parent.right

            visible: !backSightCalibrationEditor.checked && !frontSightCalibrationEditor.checked
            animationToInvisible: false
            text: "Hmm, you need to <b>check</b> either <i>front</i> or <i>back sights</i> box, or both, depending on your data."
        }
    }
}

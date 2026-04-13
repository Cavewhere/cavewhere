/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Layouts
import cavewherelib

QQ.Item {
    id: calibrationEditor
    property TripCalibration calibration

    readonly property real _minColumnWidth: 200

    implicitHeight: childrenRect.height

    ColumnLayout {
        anchors.left: parent.left
        anchors.right: parent.right

        spacing: 5

        SectionLabel {
            text: "Calibration"
            Layout.alignment: Qt.AlignHCenter
        }

        QQ.Flow {
            id: decTapeFlow
            Layout.fillWidth: true
            spacing: Theme.flowSpacing

            readonly property real halfWidth: (width - spacing) / 2
            readonly property bool twoColumn: halfWidth >= calibrationEditor._minColumnWidth

            DeclainationEditor {
                calibration: calibrationEditor.calibration
                width: decTapeFlow.twoColumn ? decTapeFlow.halfWidth : decTapeFlow.width
            }

            TapeCalibrationEditor {
                id: tapeEditor
                calibration: calibrationEditor.calibration
                width: decTapeFlow.twoColumn ? decTapeFlow.halfWidth : decTapeFlow.width
            }
        }

        QQ.Flow {
            id: sightsFlow
            Layout.fillWidth: true
            spacing: Theme.flowSpacing

            readonly property real halfWidth: (width - spacing) / 2
            readonly property bool twoColumn: halfWidth >= calibrationEditor._minColumnWidth

            FrontSightCalibrationEditor {
                id: frontSightCalibrationEditor
                calibration: calibrationEditor.calibration
                contentsVisible: checked
                width: sightsFlow.twoColumn ? sightsFlow.halfWidth : sightsFlow.width
            }

            BackSightCalibrationEditor {
                id: backSightCalibrationEditor
                calibration: calibrationEditor.calibration
                contentsVisible: checked
                width: sightsFlow.twoColumn ? sightsFlow.halfWidth : sightsFlow.width
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

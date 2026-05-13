/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib
import "Utils.js" as Utils

GroupBox {
    id: editorId

    property TripCalibration calibration

    readonly property bool effectiveAutoMode: calibration !== null
                                              && calibration.autoDeclination
                                              && calibration.autoDeclinationAvailable

    contentHeight: column.height
    text: "Declination"

    QQ.Column {
        id: column
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 4

        QQ.Row {
            spacing: 6

            LabelWithHelp {
                id: declination
                anchors.verticalCenter: parent.verticalCenter
                text: "Declination"
                helpArea: declinationHelp
            }

            // Mode picker is only shown when auto is actually available — when
            // it isn't, there's nothing to choose between, so the manual editor
            // appears alone.
            QC.ComboBox {
                id: modeComboId
                objectName: "declinationModeCombo"
                anchors.verticalCenter: parent.verticalCenter
                visible: editorId.calibration !== null
                         && editorId.calibration.autoDeclinationAvailable
                model: ["Auto", "Manual"]
                currentIndex: editorId.calibration !== null
                              && editorId.calibration.autoDeclination ? 0 : 1

                onActivated: (index) => {
                    if (editorId.calibration !== null) {
                        editorId.calibration.autoDeclination = (index === 0)
                    }
                }
            }

            // One control for both modes: ClickTextInput renders read-only in
            // the regular text color and editable in textLink color, so users
            // see the auto vs manual state without a separate readout widget.
            ClickTextInput {
                id: valueInputId
                objectName: "declinationEdit"
                anchors.verticalCenter: parent.verticalCenter
                readOnly: editorId.effectiveAutoMode
                text: editorId.calibration !== null
                      ? Utils.fixed(editorId.effectiveAutoMode
                                    ? editorId.calibration.declination
                                    : editorId.calibration.declinationManual, 2)
                      : ""

                onFinishedEditting: (newText) => {
                    if (editorId.calibration !== null) {
                        editorId.calibration.declinationManual = newText
                    }
                }
            }

            QQ.Image {
                id: warningIconId
                objectName: "declinationWarningIcon"
                anchors.verticalCenter: parent.verticalCenter
                visible: editorId.calibration !== null
                         && editorId.calibration.declinationWarning.length > 0
                source: "qrc:icons/svg/warning.svg"
                sourceSize: Qt.size(16, 16)

                QC.ToolTip.visible: warningHoverId.hovered
                QC.ToolTip.text: editorId.calibration !== null
                                 ? editorId.calibration.declinationWarning
                                 : ""

                QQ.HoverHandler {
                    id: warningHoverId
                }
            }
        }

        HelpArea {
            id: declinationHelp
            anchors.left: parent.left
            anchors.right: parent.right
            text: "<p>Magnetic declination is the <b>angle between magnetic north and true north</b></p>" +
            "CaveWhere calculates the true bearing (<b>TB</b>) by adding declination (<b>D</b>) to magnetic bearing (<b>MB</b>)." +
            "<center><b>MB + D = TB</b></center>" +
            "<p>In <b>Auto</b> mode, declination is computed from the trip's date and the cave's location using the IGRF magnetic model.</p>"
        }
    }
}

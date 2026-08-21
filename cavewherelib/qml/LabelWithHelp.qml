/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

QC.Label {
    id: label

    property HelpArea helpArea

//    font.bold: true
    font.underline: false

    QQ.MouseArea {
        id: textMouseArea
        objectName: "labelWithHelpMouseArea"
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor

        onClicked: {
            label.helpArea.visible = !label.helpArea.visible
        }
    }

    states: [
        QQ.State {
            name: "HOVER"
            when: textMouseArea.containsMouse

            QQ.PropertyChanges {
                label {
                    color: Theme.accent
                    font.underline: true
                }
            }
        }
    ]
}








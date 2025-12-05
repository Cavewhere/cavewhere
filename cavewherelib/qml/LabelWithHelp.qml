/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

Text {
    id: label

    property HelpArea helpArea

//    font.bold: true
    font.underline: false
    color: Theme.text

    QQ.MouseArea {
        id: textMouseArea
        anchors.fill: parent
        hoverEnabled: true

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








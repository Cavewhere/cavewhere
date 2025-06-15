/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ

Text {
    id: label

    property HelpArea helpArea

//    font.bold: true
    font.underline: false

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
                    color: "blue"
                    font.underline: true
                }
            }
        }
    ]
}









/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0 as QQ

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
            helpArea.visible = !helpArea.visible
        }
    }

    states: [
        QQ.State {
            name: "HOVER"
            when: textMouseArea.containsMouse

            QQ.PropertyChanges {
                target: label
                color: "blue"
                font.underline: true
            }
        }
    ]
}









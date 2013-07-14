/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0

Text {
    id: label

    property HelpArea helpArea

//    font.bold: true
    font.underline: false

    MouseArea {
        id: textMouseArea
        anchors.fill: parent
        hoverEnabled: true

        onClicked: {
            helpArea.visible = !helpArea.visible
        }
    }

    states: [
        State {
            name: "HOVER"
            when: textMouseArea.containsMouse

            PropertyChanges {
                target: label
                color: "blue"
                font.underline: true
            }
        }
    ]
}









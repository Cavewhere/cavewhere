/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Layouts

QQ.Item {
    property alias text: groupText.text
    default property alias buttons: buttonArea.children

    width: childrenRect.width
    height: childrenRect.height

    QQ.Rectangle {
        id: buttonAreaRect

        width: buttonArea.implicitWidth + 6
        height: buttonArea.implicitHeight + 3 + textRect.height / 2.0

        border.width: 1
        border.color: "black"

        //color: "gray"

        radius: 3

        RowLayout {
            id: buttonArea
            x: 3
            // anchors.horizontalCenter: buttonAreaRect.horizontalCenter

            y: 2

            // width: childrenRect.width
            // height: childrenRect.height

            spacing: 3
        }
    }

    QQ.Rectangle {
        id: textRect

        anchors.verticalCenter: buttonAreaRect.bottom
        anchors.horizontalCenter: buttonAreaRect.horizontalCenter

        width: groupText.width + 4
        height: groupText.height + 3

        radius: 3
//        color: "white"
        border.width: 1

        Text {
            id: groupText

            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: 1

            font.bold: true
        }
    }

}

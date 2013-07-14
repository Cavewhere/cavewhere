/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0
import QtQuick.Window 2.0

Window {
    id: aboutWindow
    width: 350
    height: columnId.height + 20
    title: "About"

    visible: true

    Rectangle {
        anchors.fill: parent
        color: "#E8E8E8"
    }

    Column {
        id: columnId
        anchors.topMargin: 10
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right

        spacing: 10

        Row {
            anchors.horizontalCenter: parent.horizontalCenter

            spacing: 10

            Image {
                id: imageId
                source: "qrc:/icons/svg/cave.svg"
            }

            Text {
                text: "Cavewhere"
                font.pointSize: 40
                anchors.verticalCenter: imageId.verticalCenter
            }
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Version: " + version
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: {
                return "Copyright 2013 Philip Schuchardt"
            }
        }
    }
}

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
    color: "#E8E8E8"
    visible: true

    Column {
        id: columnId
        anchors.topMargin: 10
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right

        spacing: 10

        CavewhereLogo {
            anchors.horizontalCenter: parent.horizontalCenter
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Version: " + version
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: {
                return "Copyright 2016 Philip Schuchardt"
            }
        }
    }
}

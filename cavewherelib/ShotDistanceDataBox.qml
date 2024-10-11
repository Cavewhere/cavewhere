/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0 as QQ
import QtQuick.Controls as Controls
import cavewherelib

DataBox {
    id: dataBoxId

    property bool distanceIncluded: true //This show's if the distance is include (true) or excluded

    QQ.Rectangle {
        visible: !distanceIncluded

        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: 2

        width: textId.width + 4
        height: textId.height - 2

        color: "gray"
        Text {
            id: textId

            anchors.centerIn: parent
            anchors.verticalCenterOffset: 1

            font.family: "monospace"
            font.pixelSize: 14
            font.bold: true
            color: "#EEEEEE"
            text: "Excluded"
        }
    }

    ContextMenuButton {
        id: menuButtonId
        visible: dataBoxId.focus

        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 3

        opacity: state == "" ? .75 : 1.0

        Controls.Menu {
            Controls.MenuItem {
                text: distanceIncluded ? "Exclude Distance" : "Include Distance"
                onTriggered: {
                    surveyChunk.setData(SurveyChunk.ShotDistanceIncludedRole, rowIndex, !distanceIncluded)
                }
            }
        }
    }
}

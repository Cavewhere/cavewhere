/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as Controls
import cavewherelib

DataBox {
    id: dataBoxId

    property bool distanceIncluded: true //This show's if the distance is include (true) or excluded

    QQ.Rectangle {
        visible: !dataBoxId.distanceIncluded

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

            // font.family: "monospace"
            font.pixelSize: 14
            font.bold: true
            color: "#EEEEEE"
            text: "Excluded"
        }
    }

    ContextMenuButton {
        id: menuButtonId
        objectName: "excludeMenuButton"
        visible: dataBoxId.focus

        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 3

        focusPolicy: Qt.NoFocus

        opacity: state === "" ? .75 : 1.0

        menu: Controls.Menu {
            id: menuId
            objectName: "excludeMenuId"

            Controls.MenuItem {
                objectName: "excludeDistanceMenuItem"
                text: dataBoxId.distanceIncluded ? "Exclude Distance" : "Include Distance"
                onTriggered: {
                    dataBoxId.surveyChunk.setData(SurveyChunk.ShotDistanceIncludedRole,
                                                  dataBoxId.rowIndex,
                                                  !dataBoxId.distanceIncluded)
                }
            }
        }
    }
}

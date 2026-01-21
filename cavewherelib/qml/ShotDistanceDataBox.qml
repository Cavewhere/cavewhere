/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

DataBox {
    id: dataBoxId

    property bool distanceIncluded: true //This show's if the distance is include (true) or excluded

    ShotMenu {
        id: removeMenuId
        dataValue: dataBoxId.dataValue
        removePreview: dataBoxId.removePreview
    }

    rightClickMenu: removeMenuId.item

    QQ.Rectangle {
        visible: !dataBoxId.distanceIncluded

        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: 2

        width: textId.width + 4
        height: textId.height - 2

        color: Theme.border
        Text {
            id: textId

            anchors.centerIn: parent
            anchors.verticalCenterOffset: 1

            // font.family: "monospace"
            font.pixelSize: 14
            font.bold: true
            color: Theme.surfaceMuted
            text: "Excluded"
        }
    }

    QQ.Loader {
        // anchors.fill: parent
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 3
        active: dataBoxId.focus

        sourceComponent: ContextMenuButton {
            id: menuButtonId
            objectName: "excludeMenuButton"
            visible: dataBoxId.focus //&& dataBoxId.dataValue.reading.state === cwDistanceReading.Valid


            focusPolicy: Qt.NoFocus

            opacity: state === "" ? .75 : 1.0

            menu: QC.Menu {
                id: menuId
                objectName: "excludeMenuId"

                QC.MenuItem {
                    objectName: "excludeDistanceMenuItem"
                    text: dataBoxId.distanceIncluded ? "Exclude Distance" : "Include Distance"
                    onTriggered: {
                        dataBoxId.dataValue.chunk.setData(SurveyChunk.ShotDistanceIncludedRole,
                                                          dataBoxId.rowIndex,
                                                          !dataBoxId.distanceIncluded)
                    }
                }
            }
        }
    }
}

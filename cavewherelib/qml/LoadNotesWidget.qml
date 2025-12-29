/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
// import QtGraphicalEffects
import cavewherelib

QQ.Item {
    id: widgetId

    signal filesSelected(var images)

    width: shadowContainerId.width
    height: shadowContainerId.height

    anchors.centerIn: parent

    // onVisibleChanged: {
    //     if(visible) {
    //         visibleAnimation.restart()
    //     }
    // }

    QQ.Item {
        id: shadowContainerId
        width: shadowBoxId.width + 20
        height: shadowBoxId.height + 20

        //FIXME: QtGraphicsEffects was removed an this probably doesn't work
        // visible: !fastBlurId.visible

        ShadowRectangle {
            id: shadowBoxId

            anchors.centerIn: parent

            width: columnId.width + 20
            height: columnId.height + 20

            radius: Theme.floatingWidgetRadius
            color: Theme.floatingWidgetColor

            ColumnLayout {
                id: columnId
                spacing: 10

                anchors.centerIn: parent

                Text {
                    id: noteTextId
                    text: "No notes found..."
                    font.pixelSize: 18
                    font.bold: true
                    anchors.horizontalCenter: parent.horizontalCenter
                }

                RowLayout {
                    id: rowId
                    spacing: 5

                    // anchors.centerIn: parent

                    Text {
                        text: "Add scanned notes\nAdd 3d Model"
                        // anchors.verticalCenter: loadNoteButtonId.verticalCenter
                    }

                    QQ.Image {
                        // anchors.verticalCenter: loadNoteButtonId.verticalCenter
                        rotation: 180
                        source: "qrc:/icons/back.png"
                        sourceSize: Qt.size(30, 30)
                    }

                    LoadNotesIconButton {
                        id: loadNoteButtonId
                        onFilesSelected: (images) => widgetId.filesSelected(images)
                        sourceSize: Qt.size(48, 48)
                    }

                }
            }
        }
    }
}

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
                    Layout.alignment: Qt.AlignHCenter
                }

                RowLayout {
                    id: rowId
                    spacing: 5
                    Layout.alignment: Qt.AlignHCenter

                    Text {
                        text: "Scanned notes\n 3d Model"
                        Layout.alignment: Qt.AlignVCenter
                    }

                    QQ.Image {
                        Layout.alignment: Qt.AlignVCenter
                        rotation: 180
                        source: "qrc:/icons/back.png"
                        sourceSize: Qt.size(30, 30)
                    }

                    LoadNotesIconButton {
                        Layout.alignment: Qt.AlignVCenter
                        id: loadNoteButtonId
                        onFilesSelected: (images) => widgetId.filesSelected(images)
                        sourceSize: Qt.size(48, 48)
                    }

                }
            }
        }
    }
}

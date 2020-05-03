/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0 as QQ
import QtGraphicalEffects 1.0
import "Theme.js" as Theme

QQ.Item {
    id: widgetId

    signal filesSelected(var images)

    width: shadowContainerId.width
    height: shadowContainerId.height

    anchors.centerIn: parent

    onVisibleChanged: {
        if(visible) {
            visibleAnimation.restart()
        }
    }

    QQ.Item {
        id: shadowContainerId
        width: shadowBoxId.width + 20
        height: shadowBoxId.height + 20

        visible: !fastBlurId.visible

        ShadowRectangle {
            id: shadowBoxId

            anchors.centerIn: parent

            width: columnId.width + 20
            height: columnId.height + 20

            radius: Theme.floatingWidgetRadius
            color: Theme.floatingWidgetColor

            QQ.Column {
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

                QQ.Row {
                    anchors.horizontalCenter: noteTextId.horizontalCenter
                    spacing: 5

                    Text {
                        text: "Add scanned notes"
                        anchors.verticalCenter: loadNoteButtonId.verticalCenter
                    }

                    QQ.Image {
                        anchors.verticalCenter: loadNoteButtonId.verticalCenter
                        rotation: 180
                        source: "qrc:/icons/back.png"
                        sourceSize: Qt.size(30, 30)
                    }

                    LoadNotesIconButton {
                        id: loadNoteButtonId
                        onFilesSelected: widgetId.filesSelected(images)
                        sourceSize: Qt.size(48, 48)
                    }

                }
            }
        }
    }

    FastBlur {
        id: fastBlurId
        source: shadowContainerId
        width: shadowContainerId.width
        height: shadowContainerId.height
        transparentBorder: true

        visible: radius != 0

        anchors.centerIn: widgetId
    }

    QQ.ParallelAnimation {
        id: visibleAnimation


        QQ.PropertyAnimation {
            target: fastBlurId
            property: "radius"
            from: 100
            to: 0
        }

        QQ.PropertyAnimation {

            target: fastBlurId
            property: "opacity"
            from: 0.0
            to: 1.0
        }
    }

}

/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0
import QtGraphicalEffects 1.0

Item {
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

    Item {
        id: shadowContainerId
        width: shadowBoxId.width + 20
        height: shadowBoxId.height + 20

        visible: !fastBlurId.visible

        ShadowRectangle {
            id: shadowBoxId

            anchors.centerIn: parent

            width: columnId.width + 40
            height: columnId.height + 20

            radius: style.floatingWidgetRadius
            color: style.floatingWidgetColor


            Style {
                id: style
            }

            Column {
                id: columnId

                width: noteTextId.width + 20
                spacing: 10

                anchors.centerIn: parent

                Text {
                    id: noteTextId
                    text: "This trip has no Notes..."
                    font.pointSize: 14
                    font.bold: true
                    anchors.horizontalCenter: parent.horizontalCenter
                }

                Row {
                    anchors.horizontalCenter: noteTextId.horizontalCenter
                    spacing: 5

                    Text {
                        text: "Add scanned notes"
                        anchors.verticalCenter: loadNoteButtonId.verticalCenter
                    }

                    Image {
                        anchors.verticalCenter: loadNoteButtonId.verticalCenter
                        rotation: 180
                        source: "qrc:/icons/back.png"
                        width: 30
                        height: width
                    }

                    LoadNotesIconButton {
                        id: loadNoteButtonId
                        onFilesSelected: widgetId.filesSelected(images)
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

    ParallelAnimation {
        id: visibleAnimation


        PropertyAnimation {
            target: fastBlurId
            property: "radius"
            from: 100
            to: 0
        }

        PropertyAnimation {

            target: fastBlurId
            property: "opacity"
            from: 0.0
            to: 1.0
        }
    }

}

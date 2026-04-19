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
    signal sketchRequested()

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
                spacing: 14

                anchors.centerIn: parent

                QC.Label {
                    id: noteTextId
                    text: "No notes found..."
                    font.pixelSize: Theme.fontSizeMedium
                    font.bold: true
                    Layout.alignment: Qt.AlignHCenter
                }

                RowLayout {
                    id: rowId
                    spacing: 16
                    Layout.alignment: Qt.AlignHCenter

                    ColumnLayout {
                        spacing: 6
                        Layout.alignment: Qt.AlignVCenter

                        QC.Label {
                            text: "Import"
                            font.bold: true
                            Layout.alignment: Qt.AlignHCenter
                        }

                        QC.Label {
                            text: "Scanned notes\n3D model"
                            horizontalAlignment: QQ.Text.AlignHCenter
                            Layout.alignment: Qt.AlignHCenter
                        }

                        LoadNotesIconButton {
                            Layout.alignment: Qt.AlignHCenter
                            id: loadNoteButtonId
                            objectName: "emptyStateLoadNotesButton"
                            onFilesSelected: (images) => widgetId.filesSelected(images)
                            sourceSize: Qt.size(48, 48)
                        }
                    }

                    VLine {}

                    ColumnLayout {
                        spacing: 6
                        Layout.alignment: Qt.AlignVCenter

                        QC.Label {
                            text: "Create"
                            font.bold: true
                            Layout.alignment: Qt.AlignHCenter
                        }

                        QC.Label {
                            text: "Draw a blank\nsketch"
                            horizontalAlignment: QQ.Text.AlignHCenter
                            Layout.alignment: Qt.AlignHCenter
                        }

                        NeutralIconButton {
                            Layout.alignment: Qt.AlignHCenter
                            objectName: "emptyStateNewSketchButton"
                            iconSource: "qrc:/twbs-icons/icons/pencil-square.svg"
                            sourceSize: Qt.size(48, 48)
                            text: "Sketch"
                            onClicked: widgetId.sketchRequested()
                        }
                    }
                }
            }
        }
    }
}

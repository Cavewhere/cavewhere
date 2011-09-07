import Qt 4.7
import Cavewhere 1.0

Rectangle {
    id: noteGallery

    property alias notesModel: galleryView.model;

    signal imagesAdded(variant images)

    anchors.fill: parent
    anchors.margins: 3

    /**
      For displaying the note as icons in a gallery
      */
    Component {
        id: listDelegate

        Item {
            id: container

            property int border: 6
            property variant noteObject: model.noteObject
            property real maxImageWidth: galleryView.width

            width: maxImageWidth
            height: maxImageWidth

            onChildrenRectChanged: {
                console.log("Children rect change:" + childrenRect)
            }

            Image {
                id: imageItem
                asynchronous: true
                //                anchors.fill: parent
                //                anchors.margins: container.border

                anchors.centerIn: parent

                source: model.imageIconPath
                width: container.maxImageWidth - 2 * container.border
                height: width;
                fillMode: Image.PreserveAspectFit
                rotation: model.noteObject.rotate
                smooth: true

                function updateHeight() {
                    if(paintedHeight == 0.0 || paintedWidth == 0.0) {
                        container.height = container.maxImageWidth
                        return;
                    }

                    var p1 = mapToItem(container, x, y);
                    var p2 = mapToItem(container, x + paintedWidth, y + paintedHeight);
                    var p3 = mapToItem(container, x, y + paintedHeight);
                    var p4 = mapToItem(container, x + paintedWidth, y);

                    var maxY = Math.max(p1.y, Math.max(p2.y, Math.max(p3.y, p4.y)));
                    var minY = Math.min(p1.y, Math.min(p2.y, Math.min(p3.y, p4.y)));

                    container.height = maxY - minY + 2 * container.border;
                }

                onPaintedGeometryChanged: {
                    updateHeight();
                }

                onRotationChanged:  {
                    updateHeight();
                }

                Component.onCompleted: {
                    updateHeight();
                }


            }


            /**
                  Probably could be allocated and deleted on the fly
                  */
            Image {
                id: statusImage
                anchors.centerIn: parent
                visible: imageItem.status == Image.Loading

                source: "qrc:icons/loadingSwirl.png"

                NumberAnimation {
                    running: imageItem.status == Image.Loading
                    target: statusImage;
                    property: "rotation";
                    from: 0
                    to: 360
                    duration: 1500
                    loops: Animation.Infinite
                }
            }
        }
    }

    Rectangle {
        id: galleryContainer
        anchors.bottom: parent.bottom
        anchors.top:  buttonArea.bottom
        anchors.right: parent.right
        anchors.topMargin: 3
        width: 210
        radius: 7

        color: "#4A4F57"

        ListView {
            id: galleryView

            anchors.fill: parent
            anchors.margins: 4

            delegate: listDelegate
            //model: surveyNoteModel

            clip: true

            orientation: ListView.Vertical

            highlight: Rectangle {
                color: "#418CFF"
                radius:  3
                width: galleryView.width
            }

            highlightMoveDuration: 300
            highlightResizeDuration: -1
            highlightResizeSpeed: -1

            spacing: 3

            MouseArea {
                anchors.fill: parent

                onClicked: {
                    var index = galleryView.indexAt(galleryView.contentX + mouseX, galleryView.contentY + mouseY);
                    galleryView.currentIndex = index;
                }
            }

            onCurrentIndexChanged: {
                noteArea.note = currentItem.noteObject;
            }
        }
    }

    Rectangle {
        id: buttonArea

        z: 1

        anchors.right: parent.right
        anchors.top:  parent.top

        width: childrenRect.width + 6
        height: childrenRect.height + 6

        radius: 3
        color: "#DDDDDD"

        Row {
            id: toolBar
            property variant iconSize: Qt.size(42, 42)

            spacing: 3

            anchors.centerIn: parent

            IconButton {
                id: panButton
                iconSource: "qrc:icons/notes.png"
                sourceSize: toolBar.iconSize
                text: "Pan"

                onClicked: {
                    noteGallery.state = "PAN"
                }
            }

            IconButton {
                iconSource: "qrc:icons/notes.png"
                sourceSize: toolBar.iconSize
                text: "Rotate"

                onClicked: {
                    //Update the note's rotation
                    noteRotationAnimation.from = noteArea.note.rotate
                    noteRotationAnimation.to = noteArea.note.rotate + 90
                    noteRotationAnimation.start()
                }
            }

            ButtonGroup {
                id: addButtonGroup

                text: "Add"

                IconButton {
                    iconSource: "qrc:icons/notes.png"
                    sourceSize: toolBar.iconSize
                    text: "Notes"

                    onClicked: {
                        fileDialog.open();
                    }

                    FileDialogHelper {
                        id: fileDialog;
                        onFilesSelected: noteGallery.imagesAdded(selected)
                    }
                }

                IconButton {
                    id: addStationId
                    iconSource: "qrc:icons/notes.png"
                    sourceSize: toolBar.iconSize
                    text: "Stations"

                    onClicked: noteGallery.state = "ADD-STATION"

                }

                IconButton {
                    id: addScrapId
                    iconSource: "qrc:icons/notes.png"
                    sourceSize: toolBar.iconSize
                    text: "Scraps"

                    onClicked: noteGallery.state = "ADD-SCRAP"
                }
            }
        }
    }

    Splitter {
        anchors.bottom: parent.bottom
        anchors.top: parent.top
        anchors.left: galleryContainer.left
        resizeObject: galleryContainer
        expandDirection: "left"
    }

    NoteItem {
        id: noteArea

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: galleryContainer.left
        anchors.bottom: parent.bottom

        glWidget: mainGLWidget
        projectFilename: project.filename

        clip: true

        MouseArea {
            id: noteItemMouseArea
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.RightButton
        }

        Keys.onDeletePressed: {

        }
    }

    PropertyAnimation {
        id: noteRotationAnimation
        target: noteArea.note
        property: "rotate"
        easing.type: Easing.InOutCubic
    }

    states: [

        State {
            name: "PAN"
            PropertyChanges {
                target: panButton
                selected: true
            }

            PropertyChanges {
                target: noteItemMouseArea
                onPressed: noteArea.panFirstPoint(Qt.point(mouse.x, mouse.y))
                onMousePositionChanged: noteArea.panMove(Qt.point(mouse.x, mouse.y))
            }

        },

        State {
            name: "ADD-STATION"
            PropertyChanges {
                target: addStationId
                selected: true
            }

            PropertyChanges {
                target: noteItemMouseArea

                onPressed: {
                    if(mouse.button == Qt.RightButton) {
                        noteArea.panFirstPoint(Qt.point(mouse.x, mouse.y))
                    }
                }

                onReleased: {
                    if(mouse.button == Qt.LeftButton) {
                        noteArea.addStation(Qt.point(mouse.x, mouse.y))
                    }
                }

                onMousePositionChanged: {
                    if(pressedButtons == Qt.RightButton) {
                        noteArea.panMove(Qt.point(mouse.x, mouse.y))
                    }
                }
            }

        },

        State {
            name:  "ADD-SCRAP"
            PropertyChanges {
                target: addScrapId
                selected: true
            }

            PropertyChanges {
                target: noteItemMouseArea
                onPressed: {
                    if(pressedButtons == Qt.RightButton) {
                        noteArea.panFirstPoint(Qt.point(mouse.x, mouse.y))
                    }
                }

                onMousePositionChanged: {
                    if(pressedButtons == Qt.RightButton) {
                        noteArea.panMove(Qt.point(mouse.x, mouse.y))
                    }
                }

            }
        }




    ]


}

import Qt 4.7
import Cavewhere 1.0

Rectangle {
    id: noteGallery

    property alias notesModel: galleryView.model;

    signal imagesAdded(variant images)

    anchors.fill: parent
    anchors.margins: 3

    Component {
        id: listDelegate

        Item {
            id: container

            property int border: 6
            property variant noteObject: model.noteObject
            property real maxImageWidth: galleryView.width

            width: maxImageWidth
            height: maxImageWidth

            Image {
                id: imageItem
                asynchronous: true
                anchors.fill: parent
                anchors.margins: container.border

                anchors.centerIn: parent

                source: model.imageIconPath
                sourceSize.width: container.maxImageWidth - 2 * container.border
                fillMode: Image.PreserveAspectFit
                rotation: model.noteObject.rotate

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
            }

            spacing: 3

            MouseArea {
                anchors.fill: parent

                onClicked: {
                    var index = galleryView.indexAt(mouseX, mouseY);
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
                iconSource: "qrc:icons/notes.png"
                sourceSize: toolBar.iconSize
                text: "Pan"
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
                    iconSource: "qrc:icons/notes.png"
                    sourceSize: toolBar.iconSize
                    text: "Stations"
                }

                IconButton {
                    iconSource: "qrc:icons/notes.png"
                    sourceSize: toolBar.iconSize
                    text: "Scraps"
                }
            }
        }
    }

    NoteItem {
        id: noteArea
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: galleryContainer.left
        anchors.bottom: parent.bottom

        anchors.margins: 3

        clip: true

        //  onImageSourceChanged: fitToView();
        glWidget: mainGLWidget
        projectFilename: project.filename
    }

    PropertyAnimation {
        id: noteRotationAnimation
        target: noteArea.note
        property: "rotate"
        easing.type: Easing.InOutCubic
    }

}

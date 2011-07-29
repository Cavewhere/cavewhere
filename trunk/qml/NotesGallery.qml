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

            property int border: 3
            property variant imageIds: image

            width: 200
            height: {
                if(imageItem.status == Image.Ready) {
                    return imageItem.height + imageContainter.border.width + border * 2
                }
                return 200;
            }

            Rectangle {
                id: imageContainter

                border.color: "black"
                border.width: 1
                anchors.fill: parent;
                anchors.margins: container.border

                Image {
                    id: imageItem
                    asynchronous: true
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.margins: 1
                    source: imageOriginalPath
                    sourceSize.width: imageContainter.width - imageContainter.border.width
                    //sourceSize.height: imageContainter.height
                    fillMode: Image.PreserveAspectFit
                }

                /**
                  Probably could be allocated and deleted on the fly
                  */
                Image {
                    id: statusImage
                    anchors.centerIn: parent
                    visible: image.status == Image.Loading

                    source: "qrc:icons/loadingSwirl.png"

                    NumberAnimation {
                        running: image.status == Image.Loading
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
    }

    Rectangle {
        id: galleryContainer
        anchors.bottom: parent.bottom
        anchors.top:  parent.top
        anchors.right: parent.right
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

            MouseArea {
                anchors.fill: parent

                onClicked: {
                    var index = galleryView.indexAt(mouseX, mouseY);
                    galleryView.currentIndex = index;
                }

            }

            onCurrentIndexChanged: {
                console.log(currentItem.imageIds + " " + currentItem.imageOriginalPath);
                noteArea.image = currentItem.imageIds;
            }
        }


    }

    Button {
        id: addNoteButton

        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 2

        iconSource: "qrc:/icons/plus.png"
        text: "Add Notes"

        z:1

        onClicked: {
            fileDialog.open();
        }

        FileDialogHelper {
            id: fileDialog;

            onFilesSelected: noteGallery.imagesAdded(selected)
        }

//        QFileDialog {
//            id: dialog
//        }

//        FileDialog {
//            id: fileDialog
//        }

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

}

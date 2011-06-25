import Qt 4.7
import Cavewhere 1.0

Rectangle {
//    width: 640
//    height: 480

    property alias notesModel: galleryView.model;



 anchors.fill: parent;

     Component {
        id: listDelegate

        Item {
            id: container

            property int border: 10
            property alias imageSource: image.source

            width: 200
            height: 200

            Image {
                id: image
                asynchronous: true
                pos.x:  border / 2
                pos.y:  border / 2
                source: imagePath
                sourceSize.width: container.width - border
                sourceSize.height: container.height - border
                fillMode: Image.PreserveAspectFit
            }
        }

    }

    ListView {
        id: galleryView
        anchors.left: parent.left
        anchors.top:  parent.top
        anchors.right: parent.right

        height: 210

        delegate: listDelegate
        //model: surveyNoteModel

        clip: true

        orientation: ListView.Horizontal

        highlight: Rectangle { color: "lightsteelblue"; radius: 5 }

        MouseArea {
            anchors.fill: parent

            onClicked: {
                console.log("Clicked!");
               var index = galleryView.indexAt(mouseX, mouseY);
                galleryView.currentIndex = index;
            }

        }

        onCurrentIndexChanged: {
            console.log(currentItem.imageSource);
            noteArea.imageSource = currentItem.imageSource;
        }

    }

    NoteItem {
        id: noteArea
        anchors.top: galleryView.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        anchors.margins: 5

        clip: true

      //  onImageSourceChanged: fitToView();
        glWidget: mainGLWidget

    }

}

import Qt 4.7

Rectangle {
    width: 640
    height: 480

    border.width: 1
    border.color: "blue"

    //clip: true

    Component {
        id: listDelegate



//        Rectangle {
//            width:  100;
//            height: 50;
//            border.width: 1

//            Text {
//                anchors.fill: parent
//                text: imagePath
//            }
//        }

        Item {
            id: container

            property int border: 10
            property alias imageSource: image.source

            width: 200
            height: 200

            Image {
                id: image
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
        anchors.top: parent.top

        height: 300
        width: 750

        delegate: listDelegate
        model: surveyNoteModel

        orientation: ListView.Horizontal

        highlight: Rectangle { color: "lightsteelblue"; radius: 5 }

        MouseArea {
            anchors.fill: parent

            onPressed: {
               var index = galleryView.indexAt(mouseX, mouseY);
                galleryView.currentIndex = index;
            }

        }

        onCurrentIndexChanged: {
            console.log(currentItem.imageSource);
            fullSizeImage.source = currentItem.imageSource;
        }

    }

    Flickable {
        id: imageArea
        anchors.top: galleryView.bottom

        width: 750
        height: 750

        clip:  true

        contentHeight: fullSizeImage.sourceSize.height
        contentWidth: fullSizeImage.sourceSize.width

        Image {
            id: fullSizeImage
            smooth: false

        }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.RightButton

            onDoubleClicked: {
                var scale = 1.0
                if(mouse.button == Qt.LeftButton) {
                    scale = 1.1;
                } else if(mouse.button == Qt.RightButton) {
                    scale = 0.9;
                }

                fullSizeImage.sourceSize.height = fullSizeImage.sourceSize.height * scale;
                fullSizeImage.sourceSize.width = fullSizeImage.sourceSize.width * scale;
            }
        }
    }


}

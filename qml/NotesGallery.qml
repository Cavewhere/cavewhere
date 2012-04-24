import QtQuick 2.0
import Cavewhere 1.0

Rectangle {
    id: noteGallery

    property alias notesModel: galleryView.model;
    property Note currentNote

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
        anchors.top:  mainButtonArea.bottom
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
                if(currentItem != null) {
                    currentNote = currentItem.noteObject
                    noteArea.image = currentItem.noteObject.image;
                }
            }
        }
    }

    Rectangle {
        id: mainButtonArea

        z: 1

        anchors.right: parent.right
        anchors.top:  parent.top

        width: childrenRect.width + 6
        height: childrenRect.height + 6

        radius: style.floatingWidgetRadius
        color: style.floatingWidgetColor

        Style {
            id: style
        }

        Row {
            id: mainToolBar
            property variant iconSize: Qt.size(42, 42)

            spacing: 3

//            anchors.centerIn: parent

            IconButton {
                id: rotateIconButtonId
                iconSource: "qrc:icons/rotate.png"
                sourceSize: mainToolBar.iconSize
                text: "Rotate"

                onClicked: {
                    //Update the note's rotation
                    noteRotationAnimation.from = currentNote.rotate
                    noteRotationAnimation.to = currentNote.rotate + 90
                    noteRotationAnimation.start()
                }
            }

            IconButton {
                id: carpetButtonId
                iconSource: "qrc:icons/carpet.png"
                sourceSize: mainToolBar.iconSize
                text: "Carpet"

                onClicked:  {
                    noteGallery.state = "SELECT"
                }
            }

            IconButton {
                iconSource: "qrc:icons/addNotes.png"
                sourceSize: mainToolBar.iconSize
                text: "Load"

                onClicked: {
                    fileDialog.open();
                }

                FileDialogHelper {
                    id: fileDialog;
                    onFilesSelected: noteGallery.imagesAdded(selected)
                }
            }
        }
    }


    Rectangle {
        id: carpetButtonArea
        visible: false
        z: 1

        anchors.right: parent.right
        anchors.top:  parent.top

        width: childrenRect.width + 6
        height: childrenRect.height + 6

        radius: mainButtonArea.radius
        color: "#EEEEEE"
        Row {
            spacing: 3
            //anchors.centerIn: parent

            IconButton {
                iconSource: "qrc:icons/back.png"
                sourceSize: mainToolBar.iconSize
                text: "Back"

                onClicked: {
                    noteGallery.state = "CARPET"
                    noteGallery.state = ""
                }
            }

            IconButton {
                id: selectObjectId
                iconSource: "qrc:icons/select.png"
                sourceSize: mainToolBar.iconSize
                text: "Select"

                onClicked: {
                    noteGallery.state = "SELECT"
                }
            }

            ButtonGroup {
                id: addButtonGroup
                text: "Add"

                IconButton {
                    id: addStationId
                    iconSource: "qrc:icons/addStation.png"
                    sourceSize: mainToolBar.iconSize
                    text: "Station"

                    onClicked: noteGallery.state = "ADD-STATION"
                }

                IconButton {
                    id: addScrapId
                    iconSource: "qrc:icons/addScrap.png"
                    sourceSize: mainToolBar.iconSize
                    text: "Scrap"

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

        note: currentNote
    }



    SequentialAnimation {
        id: noteRotationAnimation

        property alias to: subAnimation.to
        property alias from: subAnimation.from

        ScriptAction {
            script: noteArea.updateRotationCenter()
        }

        PropertyAnimation {
            id: subAnimation
            target: currentNote
            property: "rotate"
            easing.type: Easing.InOutCubic
        }
    }



    states: [

        State {
            name: "CARPET"

            PropertyChanges {
                target: mainButtonArea
                visible: false
            }

            PropertyChanges {
                target: carpetButtonArea
                visible: true
            }

            PropertyChanges {
                target: galleryContainer
                anchors.top:  carpetButtonArea.bottom
            }

//            PropertyChanges {
//                target: noteArea
//                state: ""
//            }
        },

        State {
            name: "SELECT"
            extend: "CARPET"

            PropertyChanges {
                target: selectObjectId
                selected: true
            }

            PropertyChanges {
                target: noteArea
                state: "SELECT"
            }
        },

        State {
            name: "ADD-STATION"
            extend:  "CARPET"
            PropertyChanges {
                target: addStationId
                selected: true
            }

            PropertyChanges {
                target: noteArea
                state: "ADD-STATION"
            }
        },

        State {
            name:  "ADD-SCRAP"
            extend:  "CARPET"
            PropertyChanges {
                target: addScrapId
                selected: true
            }

            PropertyChanges {
                target: noteArea
                state: "ADD-SCRAP"
            }
        }
    ]

    transitions: [
        Transition {
            from: ""
            to: "SELECT"

            PropertyAnimation {
                target: carpetButtonArea;
                property: "anchors.rightMargin"
                from: carpetButtonArea.mapFromItem(carpetButtonId, carpetButtonId.width + 20, 0.0).x - carpetButtonArea.width
                to: 0
            }

            PropertyAnimation {
                target: carpetButtonArea;
                property: "anchors.topMargin"
                from: carpetButtonArea.mapFromItem(carpetButtonId, 0.0, carpetButtonId.height).y - carpetButtonArea.height
                to: 0
            }

            PropertyAction { target: mainButtonArea; property: "visible"; value: true }
            PropertyAnimation {
                target: carpetButtonArea
                properties: "scale"
                from: 0.0
                to: 1.0
                easing.type: Easing.OutQuad
            }
        },

        Transition {
            to: ""
            from: "CARPET"

            PropertyAnimation {
                target: carpetButtonArea;
                property: "anchors.rightMargin"
                to: carpetButtonArea.mapFromItem(carpetButtonId, carpetButtonId.width + 20, 0.0).x - carpetButtonArea.width
                from: 0
            }

            PropertyAnimation {
                target: carpetButtonArea;
                property: "anchors.topMargin"
                to: carpetButtonArea.mapFromItem(carpetButtonId, 0.0, carpetButtonId.height).y - carpetButtonArea.height
                from: 0
            }

            PropertyAction { target: carpetButtonArea; property: "visible"; value: true }
            PropertyAnimation {
                target: carpetButtonArea
                properties: "scale"
                from: 1.0
                to: 0.0
                easing.type: Easing.InQuad
            }
        }
    ]
}

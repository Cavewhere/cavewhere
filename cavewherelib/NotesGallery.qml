/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

QQ.Rectangle {
    id: noteGallery

    objectName: "noteGallery"

    property alias notesModel: galleryView.model;
    property Note currentNote
    property alias currentNoteIndex: galleryView.currentIndex

    readonly property string mode: {
        switch(state) {
            case "": return "DEFAULT"
            case "NO_NOTES": return "DEFAULT"
            case "CARPET": return "CARPET"
            case "SELECT": return "CARPET"
            case "ADD-STATION": return "CARPET"
            case "ADD-SCRAP": return "CARPET"
            case "ADD-LEAD": return "CARPET"
            default: return "ERROR"
        }
    }

    function setMode(mode) {
        if(mode !== noteGallery.mode) {
            switch(mode) {
            case "DEFAULT":
                if(noteGallery.mode == "CARPET") {
                    noteGallery.state = "CARPET"
                }
                if(notesModel.rowCount() === 0) {
                    noteGallery.state = "NO_NOTES"
                } else {
                    noteGallery.state = ""
                    galleryContainer.visible = true;
                    mainButtonArea.visible = true;
                }
                break;
            case "CARPET":
                noteGallery.state = "SELECT"
            }
        }
    }

    signal imagesAdded(list<url> images)
    signal backClicked();

    anchors.margins: 3

    LoadNotesWidget {
        id: loadNoteWidgetId
        onFilesSelected: (images) => noteGallery.imagesAdded(images)
        visible: false
    }

    /**
      For displaying the note as icons in a gallery
      */
    component ListDelegate : QQ.Item {
        id: container

        property int border: 6
        required property Note noteObject
        required property url imageIconPath
        required property int index
        property real maxImageWidth: galleryView.width

        width: maxImageWidth
        height: maxImageWidth

        QQ.Image {
            id: imageItem
            asynchronous: true

            anchors.centerIn: parent

            source: container.imageIconPath
            width: container.maxImageWidth - 2 * container.border
            height: width;
            fillMode: QQ.Image.PreserveAspectFit
            rotation: container.noteObject.rotate
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

            QQ.Component.onCompleted: {
                updateHeight();
            }
        }

        QQ.Image {
            id: removeImageId

            source: "qrc:/icons/red-cancel.png"

            width: 24
            height: 24

            anchors.right: parent.right
            anchors.top: parent.top

            visible: galleryView.currentIndex == container.index

            opacity: removeImageMouseArea.containsMouse ? 1.0 : .75;

            QQ.MouseArea {
                id: removeImageMouseArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: removeAskDialog.show()
            }
        }

        RemoveAskBox {
            id: removeAskDialog
            anchors.right: removeImageId.horizontalCenter
            anchors.top: removeImageId.verticalCenter
            onRemove: {
                noteGallery.notesModel.removeNote(container.index);
            }
        }

        QC.BusyIndicator {
            anchors.centerIn: parent
            running: imageItem.status == QQ.Image.Loading
        }
    }

    QQ.Component {
        id: listDelegate
        ListDelegate {}
    }


    QQ.Rectangle {
        id: galleryContainer
        anchors.bottom: parent.bottom
        anchors.top:  mainButtonArea.bottom
        anchors.right: parent.right
        anchors.topMargin: 3
        anchors.rightMargin: mainButtonArea.anchors.rightMargin
        width: 210
        radius: 7
        visible: true

        color: "#4A4F57"

        ShadowRectangle {
            id: errorBoxId

            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: visible ? errorColumnId.height + 10 : 0
            anchors.leftMargin: 5
            anchors.rightMargin: 5
            anchors.topMargin: 5
            visible: errorText.text != ""

            color: "red";

            QQ.Column {
                id: errorColumnId
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.right: parent.right

                Text {
                    id: errorText
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: 3
                    text: imageValidator.errorMessage
                    wrapMode: Text.WordWrap
                }

                Button {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "Okay"
                    onClicked: {
                        imageValidator.clearErrorMessage()
                    }
                }
            }

        }

        ImageValidator {
            id: imageValidator
        }

        QQ.ListView {
            id: galleryView

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: errorBoxId.bottom
            anchors.bottom: parent.bottom

            anchors.margins: 4

            delegate: listDelegate

            clip: true

            orientation: QQ.ListView.Vertical

            highlight: QQ.Rectangle {
                color: "#418CFF"
                radius:  3
                width: galleryView.width
            }

            highlightMoveDuration: 300
            highlightResizeDuration: -1

            spacing: 3

            function updateCurrentNote() {
                if(currentItem != null) {
                    noteGallery.currentNote = (currentItem as ListDelegate).noteObject;
                    // noteArea.image = Qt.binding(function() { return currentItem.noteObject.image });
                } else {
                    noteGallery.currentNote = null;
                    // noteArea.clearImage();
                }
            }

            QQ.MouseArea {
                anchors.fill: parent

                propagateComposedEvents: true

                onClicked: (mouse) => {
                    var index = galleryView.indexAt(galleryView.contentX + mouseX, galleryView.contentY + mouseY);
                    if(index >= 0 && index < galleryView.count) {
                        galleryView.currentIndex = index;
                    }

                    mouse.accepted = false
                }
            }

            onCurrentIndexChanged: updateCurrentNote()
            onCountChanged: {
                updateCurrentNote()
                if(count == 0) {
                    noteGallery.state = "NO_NOTES"
                }
            }

        }
    }

    ShadowRectangle {
        id: mainButtonArea
        visible: true
        z: 1

        anchors.right: parent.right
        anchors.top:  parent.top
        anchors.topMargin: 5
        anchors.rightMargin: 8

        width: mainToolBar.width + 6
        height: mainToolBar.height + 6

        radius: Theme.floatingWidgetRadius
        color: Theme.floatingWidgetColor

        QQ.Row {
            id: mainToolBar
            property size iconSize: Qt.size(42, 42)

            spacing: 3

            IconButton {
                id: rotateIconButtonId
                iconSource: "qrc:icons/svg/rotate.svg"
                sourceSize: mainToolBar.iconSize
                text: "Rotate"

                onClicked: {
                    //Update the note's rotation
                    noteRotationAnimation.from = noteGallery.currentNote.rotate
                    noteRotationAnimation.to = noteGallery.currentNote.rotate + 90
                    noteRotationAnimation.start()
                }
            }

            IconButton {
                id: carpetButtonId
                objectName: "carpetButtonId"
                iconSource: "qrc:icons/svg/carpet.svg"
                sourceSize: mainToolBar.iconSize
                text: "Carpet"

                onClicked:  {
                    noteGallery.state = "SELECT"
                }
            }

            LoadNotesIconButton {
                sourceSize: mainToolBar.iconSize
                onFilesSelected: (images) =>  {
                    noteGallery.imagesAdded(images)
                }
            }
        }
    }


    ShadowRectangle {
        id: carpetButtonArea
        visible: false
        z: 1

        anchors.right: parent.right
        anchors.top:  parent.top
        anchors.rightMargin: mainButtonArea.anchors.rightMargin
        anchors.topMargin: mainButtonArea.anchors.topMargin

        width: carpetRowId.width + 6
        height: carpetRowId.height + 6

        radius: mainButtonArea.radius
        color: "#EEEEEE"
        QQ.Row {
            id: carpetRowId
            spacing: 3

            IconButton {
                iconSource: "qrc:icons/svg/back.svg"
                sourceSize: mainToolBar.iconSize
                text: "Back"

                onClicked: {
                    noteGallery.state = "CARPET"
                    noteGallery.state = ""
                    noteGallery.backClicked()
                }
            }

            IconButton {
                id: selectObjectId
                iconSource: "qrc:icons/svg/select.svg"
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
                    id: addScrapId
                    iconSource: "qrc:icons/svg/addScrap.svg"
                    sourceSize: mainToolBar.iconSize
                    text: "Scrap"

                    onClicked: noteGallery.state = "ADD-SCRAP"
                }

                IconButton {
                    id: addStationId
                    iconSource: "qrc:icons/svg/addStation.svg"
                    sourceSize: mainToolBar.iconSize
                    text: "Station"

                    onClicked: noteGallery.state = "ADD-STATION"
                }

                IconButton {
                    id: addLeadId
                    iconSource: "qrc:icons/svg/addLead.svg"
                    sourceSize: mainToolBar.iconSize
                    text: "Lead"

                    onClicked: noteGallery.state = "ADD-LEAD"
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

        visible: true
        scrapsVisible: false
        note: noteGallery.currentNote
    }

    QQ.SequentialAnimation {
        id: noteRotationAnimation

        property alias to: subAnimation.to
        property alias from: subAnimation.from

        QQ.PropertyAnimation {
            id: subAnimation
            target: noteGallery.currentNote
            property: "rotate"
            easing.type: QQ.Easing.InOutCubic
        }
    }

    states: [

        QQ.State {
            name: "NO_NOTES"

            QQ.PropertyChanges {
                loadNoteWidgetId {
                    visible: true
                }
            }

            QQ.PropertyChanges {
                galleryContainer {
                    visible: false
                }
            }

            QQ.PropertyChanges {
                mainButtonArea {
                    visible: false
                }
            }

            QQ.PropertyChanges {
                noteArea {
                    visible: false
                }
            }

            QQ.PropertyChanges {
                carpetButtonArea {
                    visible: false
                }
            }

            QQ.PropertyChanges {
                galleryView {
                    onCountChanged: {
                        if(count > 0) {
                            noteGallery.state = ""
                            galleryContainer.visible = true
                            mainButtonArea.visible = true
                            noteArea.visible = true
                            currentIndex = 0;
                        }
                        updateCurrentNote()
                    }
                }
            }
        },

        QQ.State {
            name: "CARPET"

            QQ.PropertyChanges {
                loadNoteWidgetId {
                    visible: false
                }
            }

            QQ.PropertyChanges {
                mainButtonArea {
                    visible: false
                }
            }

            QQ.PropertyChanges {
                carpetButtonArea {
                    visible: true
                }
            }

            QQ.PropertyChanges {
                galleryContainer {
                    anchors.top:  carpetButtonArea.bottom
                    visible: true
                }
            }

            QQ.PropertyChanges {
                noteArea {
                    scrapsVisible: true
                }
            }

        },

        QQ.State {
            name: "SELECT"
            extend: "CARPET"

            QQ.PropertyChanges {
                selectObjectId {
                    selected: true
                }
            }

            QQ.PropertyChanges {
                noteArea {
                    state: "SELECT"
                }
            }
        },

        QQ.State {
            name: "ADD-STATION"
            extend:  "CARPET"
            QQ.PropertyChanges {
                addStationId {
                    selected: true
                }
            }

            QQ.PropertyChanges {
                noteArea {
                    state: "ADD-STATION"
                }
            }
        },

        QQ.State {
            name: "ADD-LEAD"
            extend:  "CARPET"
            QQ.PropertyChanges {
                addLeadId {
                    selected: true
                }
            }

            QQ.PropertyChanges {
                noteArea {
                    state: "ADD-LEAD"
                }
            }
        },

        QQ.State {
            name:  "ADD-SCRAP"
            extend:  "CARPET"
            QQ.PropertyChanges {
                addScrapId {
                    selected: true
                }
            }

            QQ.PropertyChanges {
                noteArea {
                    state: "ADD-SCRAP"
                }
            }
        }
    ]

    transitions: [
        QQ.Transition {
            from: ""
            to: "SELECT"

            QQ.PropertyAnimation {
                target: carpetButtonArea;
                property: "anchors.rightMargin"
                from: carpetButtonArea.mapFromItem(carpetButtonId, carpetButtonId.width + 20, 0.0).x - carpetButtonArea.width
                to: mainButtonArea.anchors.rightMargin
            }

            QQ.PropertyAnimation {
                target: carpetButtonArea;
                property: "anchors.topMargin"
                from: carpetButtonArea.mapFromItem(carpetButtonId, 0.0, carpetButtonId.height).y - carpetButtonArea.height
                to: mainButtonArea.anchors.topMargin
            }

            QQ.PropertyAction { target: mainButtonArea; property: "visible"; value: true }
            QQ.PropertyAnimation {
                target: carpetButtonArea
                properties: "scale"
                from: 0.0
                to: 1.0
                easing.type: QQ.Easing.OutQuad
            }
        },

        QQ.Transition {
            to: ""
            from: "CARPET"

            QQ.PropertyAnimation {
                target: carpetButtonArea;
                property: "anchors.rightMargin"
                to: carpetButtonArea.mapFromItem(carpetButtonId, carpetButtonId.width + 20, 0.0).x - carpetButtonArea.width
                from: mainButtonArea.anchors.rightMargin
            }

            QQ.PropertyAnimation {
                target: carpetButtonArea;
                property: "anchors.topMargin"
                to: carpetButtonArea.mapFromItem(carpetButtonId, 0.0, carpetButtonId.height).y - carpetButtonArea.height
                from: mainButtonArea.anchors.topMargin
            }

            QQ.PropertyAction { target: carpetButtonArea; property: "visible"; value: true }
            QQ.PropertyAnimation {
                target: carpetButtonArea
                properties: "scale"
                from: 1.0
                to: 0.0
                easing.type: QQ.Easing.InQuad
            }
        }
    ]
}

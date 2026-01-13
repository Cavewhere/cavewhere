/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

QQ.Rectangle {
    id: noteGallery

    objectName: "noteGallery"

    property alias notesModel: galleryView.model;
    property Note currentNote
    property NoteLiDAR currentNoteLiDAR
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
    color: Theme.background

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
        objectName: "noteImage" + index

        property int border: 6
        required property QQ.QtObject noteObject //This will be typecase other
        required property url iconPath
        required property int index
        property real maxImageWidth: galleryView.width
        readonly property bool hasIconSource: iconPath !== undefined && iconPath.toString().length > 0
        readonly property string fallbackFileName: {
            if (!noteObject) {
                return ""
            }
            if (noteObject.filename !== undefined && noteObject.filename !== "") {
                return RootData.fileName(noteObject.filename)
            }
            if (noteObject.name !== undefined && noteObject.name !== "") {
                return noteObject.name
            }
            return ""
        }
        readonly property string fallbackFileExtension: {
            const dotIndex = fallbackFileName.lastIndexOf(".")
            if (dotIndex > 0 && dotIndex < fallbackFileName.length - 1) {
                return fallbackFileName.slice(dotIndex + 1).toUpperCase()
            }
            return qsTr("GLB")
        }

        width: maxImageWidth
        height: maxImageWidth

        QQ.Rectangle {
            //Background for the image for transparent image and svgs
            color: "#FFFFFF"
            anchors.centerIn: imageItem
            width: imageItem.paintedWidth
            height: imageItem.paintedHeight
        }

        QQ.Image {
            id: imageItem
            objectName: "noteImageItem"
            asynchronous: true

            anchors.centerIn: parent

            source: container.iconPath

            sourceSize: Qt.size(width, height)
            width: container.maxImageWidth - 2 * container.border
            height: width;
            fillMode: QQ.Image.PreserveAspectFit
            rotation: container.noteObject === null || container.noteObject.rotate == undefined ? 0 : container.noteObject.rotate
            smooth: true
            visible: container.hasIconSource

            function updateHeight() {
                if(paintedHeight === 0.0 || paintedWidth === 0.0) {
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
                updateHeight(); }

            QQ.Component.onCompleted: {
                updateHeight();
            }
        }

        QQ.Rectangle {
            id: placeholderIcon
            anchors.fill: imageItem
            anchors.margins: 6
            radius: 6
            visible: !container.hasIconSource || imageItem.status === QQ.Image.Error
            color: "#2c3037"
            border.color: "#5a6170"
            border.width: 2

            QQ.Column {
                anchors.centerIn: parent
                spacing: 6
                width: parent.width - 16

                QQ.Text {
                    text: container.fallbackFileExtension
                    font.bold: true
                    font.pixelSize: 32
                    color: "#f2f4f8"
                    horizontalAlignment: Text.AlignHCenter
                    width: parent.width
                }

                QQ.Text {
                    text: container.fallbackFileName
                    font.bold: true
                    font.pixelSize: 16
                    color: "#cbd0da"
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                    width: parent.width
                }
            }
        }

        QQ.Image {
            id: removeImageId
            objectName: "removeImageButton"

            source: "qrc:/icons/red-cancel.png"

            width: 24
            height: 24

            anchors.right: parent.right
            anchors.top: parent.top

            visible: galleryView.currentIndex === container.index

            QQ.TapHandler {
                gesturePolicy: QQ.TapHandler.ReleaseWithinBounds
                onSingleTapped: removeAskDialog.show()
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
            running: imageItem.visible && imageItem.status == QQ.Image.Loading
        }

        QQ.TapHandler {
            gesturePolicy: QQ.TapHandler.ReleaseWithinBounds
            onSingleTapped: {
                galleryView.currentIndex = container.index
            }
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

        color: Theme.floatingWidgetColor

        ShadowRectangle {
            id: errorBoxId

            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: visible ? errorColumnId.height + 10 : 0
            anchors.leftMargin: 5
            anchors.rightMargin: 5
            anchors.topMargin: 5
            visible: errorText.text !== ""

            color: Theme.danger;

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
            objectName: "galleryView"

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: errorBoxId.bottom
            anchors.bottom: parent.bottom

            anchors.margins: 4

            delegate: listDelegate

            clip: true

            orientation: QQ.ListView.Vertical

            highlight: QQ.Rectangle {
                color: Theme.accent
                radius:  3
                width: galleryView.width
            }

            highlightMoveDuration: 300
            highlightResizeDuration: -1

            spacing: 3

            function updateCurrentNote() {
                if(currentItem != null) {
                    noteGallery.currentNote = (currentItem as ListDelegate).noteObject as Note;
                    noteGallery.currentNoteLiDAR = (currentItem as ListDelegate).noteObject as NoteLiDAR;

                    // console.log("currentNote:" + (currentItem as ListDelegate).noteObject)

                    // noteArea.image = Qt.binding(function() { return currentItem.noteObject.image });
                } else {
                    noteGallery.currentNote = null;
                    noteGallery.currentNoteLiDAR = null;
                    // noteArea.clearImage();
                }
            }

            onCurrentIndexChanged: updateCurrentNote()
            onCountChanged: {
                updateCurrentNote()
                if(count <= 0) {
                    noteGallery.state = "NO_NOTES"
                } else {
                    noteGallery.state = ""
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

            NeutralIconButton {
                id: rotateIconButtonId
                iconSource: "qrc:icons/svg/rotate.svg"
                sourceSize: mainToolBar.iconSize
                text: "Rotate"
                adjustColor: false

                onClicked: {
                    //Update the note's rotation
                    noteRotationAnimation.from = noteGallery.currentNote.rotate
                    noteRotationAnimation.to = noteGallery.currentNote.rotate + 90
                    noteRotationAnimation.start()
                }
            }

            NeutralIconButton {
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
        color: Theme.surfaceMuted
        RowLayout {
            id: carpetRowId
            spacing: 3

            NeutralIconButton {
                objectName: "backButton"
                iconSource: "qrc:icons/svg/back.svg"
                sourceSize: mainToolBar.iconSize
                text: "Back"

                onClicked: {
                    noteGallery.state = "CARPET"
                    noteGallery.state = ""
                    noteLidarArea.state = "SELECT"
                    noteGallery.backClicked()
                }
            }

            NeutralIconButton {
                id: selectObjectId
                objectName: "selectButton"
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

                NeutralIconButton {
                    id: addScrapId
                    objectName: "addScrapButton"

                    iconSource: "qrc:icons/svg/addScrap.svg"
                    sourceSize: mainToolBar.iconSize
                    text: "Scrap"
                    visible: noteGallery.currentNote !== null

                    onClicked: noteGallery.state = "ADD-SCRAP"
                }

                NeutralIconButton {
                    id: addStationId
                    objectName: "addScrapStation"

                    iconSource: "qrc:icons/svg/addStation.svg"
                    sourceSize: mainToolBar.iconSize
                    text: "Station"

                    onClicked: noteGallery.state = "ADD-STATION"
                }

                NeutralIconButton {
                    id: addLeadId
                    iconSource: "qrc:icons/svg/addLead.svg"
                    objectName: "addLeads"
                    sourceSize: mainToolBar.iconSize
                    text: "Lead"
                    visible: noteGallery.currentNote !== null

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

        visible: noteGallery.currentNote !== null
        scrapsVisible: false
        note: noteGallery.currentNote
    }

    NoteLiDARItem {
        id: noteLidarArea
        anchors.fill: noteArea
        visible: noteGallery.currentNoteLiDAR !== null
        note: noteGallery.currentNoteLiDAR
    }

    QQ.SequentialAnimation {
//        id: renderViewId
//        width: 1024
//        height: width
//        anchors.bottom: parent.bottom
//        anchors.left: parent.left
//        anchors.margins: 10
//        border.width: 1
//        visible: false

//        Renderer3D {
//            id: renderer3DId
//            anchors.fill: parent
//        }
//    }

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
                galleryView.onCountChanged: () => {
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

            // QQ.PropertyChanges {
            //     target: renderViewId
            //     visible: true
            // }

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

                noteLidarArea {
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

                noteLidarArea {
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
            objectName: "toCarpetTransition"
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
            QQ.PropertyAction { target: noteArea; property: "scrapsVisible"; value: false }
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

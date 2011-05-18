import Qt 4.7
import Cavewhere 1.0

Rectangle {
    id: area

    property alias currentTrip: view.trip

    anchors.fill: parent

    Flickable {
        id: flickArea

        contentHeight: view.contentHeight
        width: view.contentWidth

        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.margins: 1;

        clip: true;

        /**
              Moves the flickable such that r is always shown
              */
        function ensureVisible(r){
            if (contentY >= r.y) {
                contentY = r.y;
            } else if (contentY+height <= r.y+r.height) {
                contentY = r.y+r.height-height;
            }
        }

        SurveyChunkGroupView {
            id: view

            anchors.fill: parent
            anchors.margins: 10;

            viewportX: flickArea.contentX;
            viewportY: flickArea.contentY;
            viewportWidth: flickArea.width;
            viewportHeight: flickArea.height;

            onEnsureVisibleRectChanged: flickArea.ensureVisible(ensureVisibleRect);
        }
    }


//    NotesGallery {
//        anchors.left: flickArea.right
//        anchors.right: parent.right
//        anchors.top: area.top
//        anchors.bottom: area.bottom
//    }
}


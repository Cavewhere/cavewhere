import Qt 4.7
import Cavewhere 1.0

Rectangle {
    id: area

    border.width: 2
    border.color: "green"


    ProxyWidget {
        id: regionTree

        width: 300;

        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.bottomMargin: 1

        widget: regionTreeView

        Component.onCompleted: {
            console.debug("Loading Widget: " + widget);
        }

        Rectangle {
            border.width: 1
            border.color: "black"

            anchors.fill: parent

            color: Qt.rgba(0, 0, 0, 0);
        }
    }

    Flickable {
        id: flickArea

        contentHeight: view.contentHeight

        width: view.contentWidth;

        anchors.top: parent.top
        anchors.bottom: parent.bottom;
        anchors.left: regionTree.right;
        anchors.margins: 1;

       // clip: true;

        /**
              Moves the flickable such that r is always shown
              */
        function ensureVisible(r){
            //console.log("Ensure visible:" + r.x + " " + r.y  + " " + r.width + " " + r.height);
//            if (contentX >= r.x) {
//                contentX = r.x;
//            } else if (contentX+width <= r.x+r.width) {
//                contentX = r.x+r.width-width;
//            }

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

            chunkGroup: surveyData

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


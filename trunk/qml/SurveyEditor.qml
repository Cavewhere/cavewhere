import Qt 4.7
import Cavewhere 1.0

Rectangle {
    id: area

    border.width: 2
    border.color: "green"

        Flickable {
            id: flickArea

            contentHeight: view.contentHeight
            contentWidth: view.contentWidth

            width: 600;
            height: area.height;

            clip: true;

            /**
              Moves the flickable such that r is always shown
              */
            function ensureVisible(r){
                console.log("Ensure visible:" + r.x + " " + r.y  + " " + r.width + " " + r.height);
                if (contentX >= r.x) {
                    contentX = r.x;
                } else if (contentX+width <= r.x+r.width) {
                    contentX = r.x+r.width-width;
                } if (contentY >= r.y) {
                    contentY = r.y;
                } else if (contentY+height <= r.y+r.height) {
                    contentY = r.y+r.height-height;
                }
            }

            SurveyChunkGroupView {
                id: view

                viewportX: flickArea.contentX;
                viewportY: flickArea.contentY;
                viewportWidth: flickArea.width;
                viewportHeight: flickArea.height;

                chunkGroup: surveyData

                onEnsureVisibleRectChanged: flickArea.ensureVisible(ensureVisibleRect);
            }
        }




//    Component {
//        id: surveyChunkViewComponent

//        FocusScope {
//            width: childrenRect.width
//            height: childrenRect.height

//            SurveyChunkView {
//                id: view
//                model: chunk

//                onXChanged: {
//                    console.log("X change:" + x + chunk + view);
//                }

//                Component.onCompleted: {
//                    console.log("Dim:" + width + height);
//                }

//            }
//        }

//    }

//    ListView {
//        id: listView

//        anchors.fill: parent

//        highlightFollowsCurrentItem: false

//        model:  surveyData
//        delegate: surveyChunkViewComponent

//    }

//    SurveyChunkView {
//       // stationDelegate: stationRow
//        model: testChunk //From the c++

////        MouseArea {
////            anchors.fill: parent;
////            onPressed: {
////                console.log("Pressed!");
////            }
////        }

//   }

//    SurveyChunkView {
//        x: 500
//        y: 0

//       // stationDelegate: stationRow
//        model: testChunk //From the c++
//   }

//    Flickable {
//        id: flickableArea;

//        anchors.fill:parent;

//        contentHeight: chunkList.height
//        //contentWidth: childrenRect.width

//        clip: true;



//        SurveyChunkList {
//            id: chunkList
//            chunkGroup: survey
//        }

//    }

//    ScrollBar {
//        scrollArea: flickableArea
//        anchors.top: flickableArea.top
//        anchors.left: flickableArea.right
//        height: flickableArea.height
//        width: 7
//    }

}

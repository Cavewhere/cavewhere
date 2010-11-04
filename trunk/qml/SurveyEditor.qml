import Qt 4.7
import Cavewhere 1.0

Rectangle {
    id: area


    border.width: 2
    border.color: "green"

    Rectangle {

        border.width: 1
        border.color: "red"

        Flickable {
            id: flickArea

            contentHeight: view.contentHeight
            contentWidth: view.contentWidth

            width: 600;
            height: 200;

            clip: true;

            SurveyChunkGroupView {
                id: view

                viewportX: flickArea.contentX;
                viewportY: flickArea.contentY;
                viewportWidth: flickArea.width;
                viewportHeight: flickArea.height;

                chunkGroup: surveyData
            }
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

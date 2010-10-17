import Qt 4.7
import Cavewhere 1.0

Rectangle {
    id: area

    height: 600
    width: 600


    border.width: 2
    border.color: "green"

    Component {
        id: stationComponent
        StationBox {
            id: stationBox
            width: 20
            height: 20
        }
    }


    SurveyChunkView {
       // stationDelegate: stationRow
        model: testChunk //From the c++

        Rectangle {
            anchors.fill: parent;
            color: 'red'
        }

//        MouseArea {
//            anchors.fill: parent;
//            onPressed: {
//                console.log("Pressed!");
//            }
//        }

   }

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

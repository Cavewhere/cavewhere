import Qt 4.7
import Cavewhere 1.0

Rectangle {

    id: page

    property variant chunkGroup

    clip: true;

    Component {
        id: stationRow
        StationBox {
            width: 20
            height: 20
        }
    }

    SurveyChunkView {
        stationDelegate: stationRow
        model: testChunk //From the c++
   }


//    height:  childrenRect.height



    /**
    Populate the survey with data
    */
//    Component.onCompleted: {
//        var chunkComponent = Qt.createComponent("SurveyChunk.qml");
//        var lastChunk = null;

//        if(chunkGroup.chunkCount() == 0) { return; }

//        var lastChunk = chunkComponent.createObject(page);
//        lastChunk.surveyData = chunkGroup.chunk(i);
//        lastChunk.x = 0;
//        lastChunk.y = 0;

//        var height = lastChunk.height;
//        for(var i = 1; i < chunkGroup.chunkCount(); i++) {
//            var surveyChunk = chunkComponent.createObject(page);
//            surveyChunk.surveyData = chunkGroup.chunk(i);
//            surveyChunk.anchors.top = lastChunk.bottom;
//            surveyChunk.anchors.left = lastChunk.left;
//            lastChunk = surveyChunk;
//            height += lastChunk.height;
//        }

//        page.width = lastChunk.width;
//    }
}

import Qt 4.7

Rectangle {
    id: area

    height: 1000
    width: 600

    border.width: 2
    border.color: "green"

    Flickable {
        id: flickableArea;

        anchors.fill:parent;

        contentHeight: chunkList.height
        //contentWidth: childrenRect.width

        clip: true;



        SurveyChunkList {
            id: chunkList
            chunkGroup: survey
        }

    }

    ScrollBar {
        scrollArea: flickableArea
        anchors.top: flickableArea.top
        anchors.left: flickableArea.right
        height: flickableArea.height
        width: 7
    }

}

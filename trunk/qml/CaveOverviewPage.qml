import Qt 4.7
import Cavewhere 1.0

Rectangle {
    id: cavePageArea

    anchors.fill: parent;

    Text {
        id: caveNameText
        text: currentCave.name //From c++
        anchors.left: parent.left
        anchors.leftMargin: 5
        anchors.top: parent.top
        anchors.topMargin: 5
        font.bold: true
        font.pointSize: 20
    }

    UsedStationsWidget {

        anchors.left: parent.left
        anchors.top: caveNameText.bottom
        anchors.bottom: parent.bottom
        anchors.leftMargin: 5
        anchors.bottomMargin: 5
        width: 250
    }

}

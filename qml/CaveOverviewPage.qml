import QtQuick 2.0
import Cavewhere 1.0

Rectangle {
    id: cavePageArea

    property Cave currentCave: null

    anchors.fill: parent;


    DoubleClickTextInput {
        id: caveNameText
        text: currentCave != null ? currentCave.name : "" //From c++
        anchors.left: parent.left
        anchors.leftMargin: 5
        anchors.top: parent.top
        anchors.topMargin: 5
        font.bold: true
        font.pointSize: 20

        onFinishedEditting: {
            currentCave.name = newText
        }
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

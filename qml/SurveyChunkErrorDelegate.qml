import QtQuick 2.0
import QtQuick.Layouts 1.0
import Cavewhere 1.0

Rectangle {
    property SurveyChunk chunk;

    z: 1
    width: parent !== null ? parent.width + 2 : 0
    height: parent !== null ? parent.height : 0
    border.color: "red"
    border.width: 1
    color: "#00000000"

    visible: {
        chunk !== null ? chunk.errorModel.errors.count > 0 : false
    }

    RowLayout {
        id: layout
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 3

        Image {
            source: "qrc:icons/stopSignError.png"
        }

        Text {

            text: "This leg isn't connected to the cave!"
        }
    }
}


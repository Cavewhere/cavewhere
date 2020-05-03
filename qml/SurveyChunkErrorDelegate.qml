import QtQuick 2.0 as QQ
import QtQuick.Layouts 1.0
import Cavewhere 1.0

QQ.Rectangle {
    id: delegateId
    property SurveyChunk chunk;

    z: 1
    width: parent !== null ? parent.width + 2 : 0
    height: parent !== null ? parent.height : 0
    border.color: "#7E0000"
    border.width: 1
    color: "#00000000"
    visible: chunk !== null ? chunk.errorModel.errors.count > 0 : false


    QQ.Rectangle {
        color: "#F6A8AA"
        parent: delegateId.parent
        width: delegateId.width
        height: delegateId.height
        z: -1
        visible: delegateId.visible
    }

    RowLayout {
        id: layout
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 3

        QQ.Image {
            source: "qrc:icons/stopSignError.png"
        }

        Text {
            id: textId
            text: {
                if(chunk !== null) {
                    if(chunk.errorModel.errors.count > 0) {
                        return chunk.errorModel.errors.getFirst().message
                    }
                }
                return ""
            }
        }
    }
}


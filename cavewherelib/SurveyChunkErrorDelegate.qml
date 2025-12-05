import QtQuick as QQ
import QtQuick.Layouts
import cavewherelib
import QtQuick.Controls as QC
QQ.Rectangle {
    id: delegateId

    required property SurveyChunk chunk;
    readonly property real errorMessageHeight: layout.implicitHeight + 3

    z: 1
    // width: parent !== null ? parent.width + 2 : 0
    // height: parent !== null ? parent.height : 0
    // border.color: "#7E0000"
    // border.width: 1
    color: Theme.transparent
    visible: chunk !== null ? chunk.errorModel.errors.count > 0 : false



    QQ.Rectangle {
        color: Theme.danger
        parent: delegateId.parent
        width: delegateId.width
        height: delegateId.height
        z: -1
        y: -errorMessageHeight
        visible: delegateId.visible
        // anchors.fill: parent
    }

    RowLayout {
        id: layout
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        // anchors.topMargin: 3

        QQ.Image {
            source: "qrc:icons/svg/stopSignError.svg"
            sourceSize: Qt.size(16,16)
        }

        Text {
            id: textId
            objectName: "errorText"
            text: {
                if(delegateId.chunk !== null) {
                    if(delegateId.chunk.errorModel.errors.count > 0) {
                        return delegateId.chunk.errorModel.errors.first().message
                    }
                }
                return ""
            }
        }
    }
}

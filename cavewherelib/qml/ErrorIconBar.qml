import QtQuick as QQ
import QtQuick.Layouts
import cavewherelib

QQ.Item {
    id: itemId

    property ErrorModel errorModel

    //A row can outlive the object whose errors it shows by a frame.
    readonly property int fatalCount: itemId.errorModel !== null ? itemId.errorModel.fatalCount : 0
    readonly property int warningCount: itemId.errorModel !== null ? itemId.errorModel.warningCount : 0

    implicitHeight: layoutId.height + 2
    implicitWidth: layoutId.width + 2

    RowLayout {
        id: layoutId

        anchors.centerIn: parent

        spacing: 1
        QQ.Image {
            source: "qrc:icons/svg/stopSignError.svg"
            sourceSize: Qt.size(16, 16)
            visible: itemId.fatalCount > 0
        }

        QQ.Image {
            source: "qrc:icons/svg/warning.svg"
            sourceSize: Qt.size(16, 16)
            visible: itemId.warningCount > 0
        }

        QQ.Image {
            source: "qrc:icons/good.png"
            sourceSize: Qt.size(16, 16)
            visible: itemId.warningCount === 0 &&
                     itemId.fatalCount === 0
        }
    }
}


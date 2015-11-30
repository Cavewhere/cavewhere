import QtQuick 2.0
import QtQuick.Layouts 1.0
import Cavewhere 1.0

Item {
    property ErrorModel errorModel

    implicitHeight: layoutId.height + 2
    implicitWidth: layoutId.width + 2

    RowLayout {
        id: layoutId

        anchors.centerIn: parent

        spacing: 1
        Image {
            source: "qrc:icons/stopSignError.png"
            sourceSize: Qt.size(16, 16)
            visible: errorModel.fatalCount > 0
        }

        Image {
            source: "qrc:icons/warning.png"
            sourceSize: Qt.size(16, 16)
            visible: errorModel.warningCount > 0
        }

        Image {
            source: "qrc:icons/good.png"
            sourceSize: Qt.size(16, 16)
            visible: errorModel.warningCount === 0 &&
                     errorModel.fatalCount === 0
        }
    }
}


import QtQuick as QQ
import QtQuick.Layouts
import cavewherelib

QQ.Item {
    id: itemId

    property ErrorModel errorModel

    implicitHeight: layoutId.height + 2
    implicitWidth: layoutId.width + 2

    // A clean trip or cave shows nothing here, so the name sits flush
    visible: itemId.errorModel !== null
             && (itemId.errorModel.fatalCount > 0 || itemId.errorModel.warningCount > 0)

    RowLayout {
        id: layoutId

        anchors.centerIn: parent

        spacing: 1
        QQ.Image {
            source: "qrc:icons/svg/stopSignError.svg"
            sourceSize: Qt.size(16, 16)
            visible: itemId.errorModel.fatalCount > 0
        }

        QQ.Image {
            source: "qrc:icons/svg/warning.svg"
            sourceSize: Qt.size(16, 16)
            visible: itemId.errorModel.warningCount > 0
        }
    }
}


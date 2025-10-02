import QtQuick as QQ

QQ.Image {
    id: stationImage
    anchors.centerIn: parent
    source: "qrc:icons/svg/station.svg"

    sourceSize: Qt.size(18, 18)

    width: sourceSize.width
    height: sourceSize.height
}

import QtQuick as QQ
import cavewherelib

QQ.Image {
    id: stationImage
    anchors.centerIn: parent
    source: "qrc:icons/svg/station.svg"

    sourceSize: Qt.size(Math.round(18 * Theme.pointSizeFactor),
                        Math.round(18 * Theme.pointSizeFactor))

    width: sourceSize.width
    height: sourceSize.height
}

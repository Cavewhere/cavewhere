import QtQuick 1.0
import Cavewhere 1.0

BorderImage {
    source: "qrc:icons/border.png"
    anchors.fill: parent;
    border { left: 5; right: 5; top: 5; bottom: 5 }
    horizontalTileMode: BorderImage.Repeat
    verticalTileMode: BorderImage.Repeat
}

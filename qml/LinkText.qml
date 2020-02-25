import QtQuick 2.0 as QQ
import Cavewhere 1.0

Text {
    id: linkTextId

    signal clicked();

    Pallete {
        id: pallete
    }

    color:  pallete.inputTextColor
    font.underline: mouseAreaId.containsMouse

    QQ.MouseArea {
        id: mouseAreaId
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor

        onClicked: {
            linkTextId.clicked()
        }
    }
}

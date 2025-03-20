import QtQuick as QQ
import cavewherelib

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

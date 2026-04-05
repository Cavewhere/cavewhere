import QtQuick as QQ
import cavewherelib
import QtQuick.Controls as QC
QC.Label {
    id: linkTextId

    signal clicked();

    color:  Theme.textLink
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

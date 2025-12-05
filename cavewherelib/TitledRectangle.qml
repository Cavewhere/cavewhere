import QtQuick as QQ
import QtQuick.Layouts
import QtQuick.Controls as QC
import cavewherelib
QQ.Rectangle {

    property alias title: onPaperId.text
    default property alias whereTheChildrenGo: columnOnPaper.children

    radius: 5
    color: Theme.tag

    implicitWidth: childrenRect.width + columnOnPaper.x * 2.0
    implicitHeight: childrenRect.height

    ColumnLayout {
        id: columnOnPaper
        x: 3
        spacing: 2

        Text {
            id: onPaperId
            Layout.alignment: Qt.AlignHCenter
        }
    }
}

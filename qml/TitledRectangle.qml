import QtQuick 2.0 as QQ
import QtQuick.Layouts 1.1

QQ.Rectangle {

    property alias title: onPaperId.text
    default property alias whereTheChildrenGo: columnOnPaper.children

    radius: 5

    width: childrenRect.width + columnOnPaper.x * 2.0
    height: childrenRect.height

    ColumnLayout {
        id: columnOnPaper
        x: 3
        spacing: 2

        Text {
            id: onPaperId
        }
    }
}

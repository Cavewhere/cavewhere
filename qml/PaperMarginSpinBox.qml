import QtQuick 2.0
import QtQuick.Controls 1.1
import QtQuick.Layouts 1.1

Rectangle {
    property alias text: textId.text
    property alias unit: unitId.text
    property alias value: spinBoxId.value

    radius: 3
    width: topId.width + 2 * radius
    height: topId.height + 2 * radius
    border.width: 1

    ColumnLayout {
        id: topId
        anchors.centerIn: parent
        Text {
            id: textId
            Layout.alignment: Qt.AlignHCenter
            text: "Top"
        }

        RowLayout {

            SpinBox {
                id: spinBoxId
                decimals: unit === "in" ? 2 : 0
                stepSize: unit === "in" ? .1 : 10
            }

            Text {
                id: unitId
            }

        }
    }
}

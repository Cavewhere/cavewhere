import QtQuick as QQ
import QtQuick.Layouts
import cavewherelib

QQ.Rectangle {
    id: pageMarginId

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

            DoubleSpinBox {
                id: spinBoxId
                decimals: pageMarginId.unit === "in" ? 2 : 0
                realValue: pageMarginId.unit === "in" ? .1 : 10
                implicitWidth: 70
            }

            Text {
                id: unitId
            }

        }
    }
}

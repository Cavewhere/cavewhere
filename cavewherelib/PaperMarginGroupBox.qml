import QtQuick as QQ
import QtQuick.Controls
import QtQuick.Layouts 1.1

GroupBox {
    id: groupBoxId

    property alias leftMargin: leftId.value
    property alias rightMargin: rightId.value
    property alias topMargin: topId.value
    property alias bottomMargin: bottomId.value
    property string unit: ""

    title: "Margins - inches"

    function setDefaultLeft(margin) {
        leftId.value = margin
        allId.value = margin
    }

    function setDefaultRight(margin) {
        rightId.value = margin
    }

    function setDefaultTop(margin) {
        topId.value = margin
    }

    function setDefaultBottom(margin) {
        bottomId.value = margin
    }


    QQ.Item {
        width: marginLayoutId.width
        height: marginLayoutId.height
        QQ.Rectangle {
            width: Math.round(marginLayoutId.width * 2 / 3)
            height: Math.round(marginLayoutId.height * 2 / 3)
            x: Math.round(marginLayoutId.width / 6)
            y: Math.round(marginLayoutId.height / 6)
            border.width: 1
        }

        GridLayout {
            id: marginLayoutId
            rows: 3
            columns: 3

            component Spacer:
                QQ.Item { implicitWidth: 1; implicitHeight: 1 }

            Spacer {}

            PaperMarginSpinBox {
                id: topId
                text: "Top"
                unit: groupBoxId.unit
            }

            Spacer {}

            PaperMarginSpinBox {
                id: leftId
                text: "Left"
                unit: groupBoxId.unit
            }

            PaperMarginSpinBox {
                id: allId
                text: "All"
                unit: groupBoxId.unit

                onValueChanged: {
                    topId.value = value
                    leftId.value = value
                    rightId.value = value
                    bottomId.value = value
                }
            }

            PaperMarginSpinBox {
                id: rightId
                text: "Right"
                unit: groupBoxId.unit
            }

            Spacer {}

            PaperMarginSpinBox {
                id: bottomId
                text: "Bottom"
                unit: groupBoxId.unit
            }

            Spacer {}
        }
    }

}

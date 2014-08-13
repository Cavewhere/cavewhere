import QtQuick 2.0
import QtQuick.Controls 1.1
import QtQuick.Layouts 1.1

GroupBox {
    id: groupBoxId

    property alias leftMargin: leftId.value
    property alias rightMargin: rightId.value
    property alias topMargin: topId.value
    property alias bottomMargin: bottomId.value

    title: "Margins - inches"

    property string unit: ""

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


    Item {
        width: marginLayoutId.width
        height: marginLayoutId.height
        Rectangle {
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

            Item { width: 1; height: 1}

            PaperMarginSpinBox {
                id: topId
                text: "Top"
                unit: groupBoxId.unit
            }

            Item { width: 1; height: 1 }

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

            Item { width:1; height:1 }

            PaperMarginSpinBox {
                id: bottomId
                text: "Bottom"
                unit: groupBoxId.unit
            }

            Item { width:1; height:1 }
        }
    }

}

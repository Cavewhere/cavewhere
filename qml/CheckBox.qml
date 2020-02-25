import QtQuick.Controls 2.12 as QC
import QtQuick 2.12 as QQ

QC.CheckBox {
    id: control
    checked: true

    indicator: QQ.Rectangle {
        implicitWidth: 18
        implicitHeight: 18
        x: control.leftPadding
        y: parent.height / 2 - height / 2
        radius: 3
        border.color: control.down ? "#4a4a4a" : "#1f1f1f"

        QQ.Rectangle {
            width: 10
            height: 10
            x: 4
            y: 4
            radius: 2
            color: control.down ? "#4a4a4a" : "#1f1f1f"
            visible: control.checked
        }
    }

    contentItem: Text {
        text: control.text
        opacity: enabled ? 1.0 : 0.3
        verticalAlignment: Text.AlignVCenter
        leftPadding: control.indicator.width + control.spacing
    }
}

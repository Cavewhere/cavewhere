import QtQuick
import QtQuick.Controls

Rectangle {
    id: delegate

    required property string name;
    required property int columnWidth;

    required property var model

    // Qt6: add cellPadding (and font etc) as public API in headerview
    readonly property real cellPadding: 2

    implicitWidth: text.implicitWidth + (cellPadding * 2)
    implicitHeight: text.implicitHeight + (cellPadding * 2)
    color: "#f6f6f6"
    border.color: "#e4e4e4"

    Label {
        id: text
        text: delegate.name
        width: delegate.width
        height: delegate.height
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        color: "#ff26282a"
    }
}

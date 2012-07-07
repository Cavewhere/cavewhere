// import QtQuick 1.0 // to target S60 5th Edition or Maemo 5
import QtQuick 2.0

Item {
    id: floatingGroupBoxId

    property int margin: 3
    property alias titleText: titleTextId.text
    default property alias boxGroupChildren: boxGroupId.children

    width: childrenRect.width
    height: childrenRect.height


    Style {
        id: style
    }

    Text {
        id: titleTextId
        z: 1
        anchors.right: boxGroupId.right
        anchors.bottom: boxGroupId.top
        anchors.rightMargin: 5
        font.bold:  true
    }

    Rectangle {
        id: backgroundRect

        color: style.floatingWidgetColor
        radius: style.floatingWidgetRadius
        height: boxGroupId.height + titleTextId.height
        width: boxGroupId.width
        //        y: groupAreaRect.height / 2.0
    }

    Rectangle {
        id: boxGroupId

        y: titleTextId.height

        border.width: 1
        radius: style.floatingWidgetRadius
        color: "#00000000"

        width: childrenRect.width + margin * 2;
        height: childrenRect.height + margin * 2;
    }
}

// import QtQuick 1.0 // to target S60 5th Edition or Maemo 5
import QtQuick 1.1
//import QtDesktop 0.1 as Desktop

Item {

    property color backgroundColor: "white"
            // TODO: QtDesktop include
//    property alias checked: checkbox.checked
//    property alias text: checkbox.text
    property bool checked: false
    property string text: "Fix me"
    property bool contentsVisible: true
    property int contentHeight
    default property alias contentData: contentArea.data

            // TODO: QtDesktop include
//    height: contentsVisible ? checkbox.height + contentHeight + 3 : checkbox.height

    Style {
        id: style
    }

    Rectangle {
        id: checkBoxGroup
        border.width: 1
        border.color: "gray"
        radius: style.floatingWidgetRadius
        color: "#00000000"
        visible: contentsVisible

                // TODO: QtDesktop include
//        anchors.top: checkbox.verticalCenter
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        Item {
            id: contentArea
            anchors.top: checkBoxGroup.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom

                    // TODO: QtDesktop include
//            anchors.topMargin: checkbox.height / 2
            anchors.leftMargin: 3
            anchors.rightMargin: 3
            anchors.bottomMargin: 3
        }
    }

    Rectangle {
        color: backgroundColor
                // TODO: QtDesktop include
//        anchors.fill: checkbox
        visible: contentsVisible
    }

    // TODO: Fix CheckableGroupBox, add checkboxes back in
//    Desktop.CheckBox {
//        id: checkbox
//        anchors.left: checkBoxGroup.left
//        anchors.leftMargin: 6
//    }

}

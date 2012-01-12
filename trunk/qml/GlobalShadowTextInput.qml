import QtQuick 1.1

MouseArea {
    id: globalMouseArea

    property alias textInput: input
    property alias editor: shadowEditor
    property int minWidth: 0
    property int minHeight: 0

    signal commitChanges()

    anchors.fill: parent
    enabled: false

    onPressed: {
        commitChanges()
        mouse.accepted = false
    }

    ShadowRectangle {
        id: shadowEditor
        visible: false; // true; //input.visible

        color: "white"

        width:  minWidth > input.width + 6 ? minWidth : input.width  + 6
        height: minHeight > input.height + 6 ? minHeight : input.height + 6

        TextInput {
            id: input
            visible: editor.visible
            anchors.centerIn: parent;

            selectByMouse: activeFocus;
            activeFocusOnPress: false

            Keys.onPressed: {
                if(event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                    commitChanges();
                    event.accepted = true
                } else if(event.key === Qt.Key_Escape) {
                    closeEditor();
                    event.accepted = true
                }
            }
        }
    }
}

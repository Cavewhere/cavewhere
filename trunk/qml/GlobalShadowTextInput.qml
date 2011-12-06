import QtQuick 1.1

MouseArea {
    id: globalMouseArea

    property alias textInput: input
    property alias editor: shadowEditor

    signal commitChanges()

    anchors.fill: parent
    enabled: false

    onPressed: {

        console.log("Global click area");
        commitChanges()
        mouse.accepted = false
    }

    ShadowRectangle {
        id: shadowEditor
        visible: false; // true; //input.visible

        color: "white"

        width: input.width + 5
        height: input.height + 5;

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

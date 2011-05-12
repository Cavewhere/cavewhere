import QtQuick 1.0

Rectangle {
    id: doubleClickTextInput
    property alias text: textArea.text
    property alias selected: textArea.font.bold

    signal startedEditting()
    signal finishedEditting(string newText)

    height: textArea.visible ? textArea.height : input.height
    width: textArea.visible ? textArea.width : input.width

    function commitChanges() {
        input.visible = false;
        input.focus = false;

        globalMouseArea.enabled = false
        globalMouseArea.parent = doubleClickTextInput
        globalMouseArea.ignoreFirstClick = false
        doubleClickArea.enabled = true

        input.x = textArea.x
        input.y = textArea.y

        //Emit the finishedEditting signal
        finishedEditting(input.text)
    }


    Text {
        id: textArea
        visible: !input.visible
        anchors.left: parent.left
    }

    MouseArea {
        id: globalMouseArea
        anchors.fill: parent
        enabled: false
        parent: doubleClickTextInput

        property bool ignoreFirstClick: false

        onPressed: {
            if(ignoreFirstClick) {
                ignoreFirstClick = false;
                return;
            }

            console.log("Global click area");
            commitChanges()
            mouse.accepted = false
        }

        TextInput {
            id: input
            visible: false

            x: textArea.x
            y: textArea.y

            selectByMouse: activeFocus;
            activeFocusOnPress: false

            Keys.onPressed: {
                if(event.key == Qt.Key_Return) {
                    commitChanges();
                    event.accepted = true
                }
            }
        }

        MouseArea {
            id: doubleClickArea
            x: textArea.x
            y: textArea.y
            width: textArea.width
            height: textArea.height
            enabled:  true

            onDoubleClicked: {
                console.log("Double click");
                doubleClickTextInput.startedEditting()

                input.text = doubleClickTextInput.text
                input.visible = true;
                input.focus = true;
                input.selectAll();

                //Change the globalMouseArea to fill the root
                globalMouseArea.enabled = true
                globalMouseArea.parent = rootQMLItem
                globalMouseArea.ignoreFirstClick = true
                doubleClickArea.enabled = false

                var globalPosition = textArea.mapToItem(input, 0, 0)

                input.x = globalPosition.x
                input.y = globalPosition.y
            }

            onPressed: {
                mouse.accepted = false
            }
        }
    }
}

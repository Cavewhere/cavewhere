import QtQuick 1.0

Item {
    id: clickTextInput
    property alias text: textArea.text
    property alias font: textArea.font
    property alias style: textArea.style
    property alias styleColor: textArea.styleColor
    property alias color: textArea.color
    property bool acceptMousePress: false  //This make the double click text box accept mouse clicks (This
    property bool doubleClickEdit: false

    signal startedEditting()
    signal finishedEditting(string newText)

    //border.width: 1

    height: textArea.visible ? textArea.height : input.height
    width: {
        var width;
        if(textArea.visible) {
            width = textArea.width
        } else {
            width = input.width
        }
        if(width <= 0) {
            width = 100
        }
        return width;
    }

    function commitChanges() {
        closeEditor();

        //Emit the finishedEditting signal
        finishedEditting(input.text)
    }

    function closeEditor() {
        edittor.visible = false;
        input.focus = false;

        globalMouseArea.enabled = false
        globalMouseArea.parent = clickTextInput
        globalMouseArea.ignoreFirstClick = false
        doubleClickArea.enabled = true

        edittor.x = textArea.x
        edittor.y = textArea.y
    }

    Text {
        id: textArea
        visible: !input.visible
        //anchors.left: parent.left

        onTextChanged: {
            textChangedAnimation.restart()
        }

        SequentialAnimation {
            id: textChangedAnimation
            NumberAnimation {
                target: textArea;
                property: "scale";
                easing.type: Easing.OutInElastic
                from: 1.0;
                to: 1.2
                duration: 100
            }
            NumberAnimation {
                target: textArea;
                property: "scale";
                easing.type: Easing.OutInElastic
                to: 1.0
                duration: 100
            }
        }
    }

    MouseArea {
        id: globalMouseArea
        anchors.fill: parent
        enabled: false
        parent: clickTextInput

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

        ShadowRectangle {
            id: edittor
            visible: false; // true; //input.visible

            x: textArea.x
            y: textArea.y

            color: "white"

            width: input.width + 5
            height: input.height + 5;

            TextInput {
                id: input
                visible: edittor.visible
                font: textArea.font
                anchors.centerIn: parent;
                //                x: textArea.x
                //                y: textArea.y

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

        MouseArea {
            id: doubleClickArea

            x: edittor.x
            y: edittor.y
            width: clickTextInput.width
            height: clickTextInput.height
            enabled:  true

            function openEdittor() {
                console.log("Double click");
                clickTextInput.startedEditting()

                input.text = clickTextInput.text
                edittor.visible = true;
                input.focus = true;
                input.selectAll();

                //Change the globalMouseArea to fill the root
                globalMouseArea.enabled = true
                globalMouseArea.parent = rootObject
                globalMouseArea.ignoreFirstClick = doubleClickEdit
                doubleClickArea.enabled = false

                var globalPosition = textArea.mapToItem(edittor, 0, 0)

                edittor.x = globalPosition.x
                edittor.y = globalPosition.y
            }

            states: [
                State {
                    name: "DOUBLE-CLICK"
                    when: doubleClickEdit

                    PropertyChanges {
                        target: doubleClickArea

                        onDoubleClicked: openEdittor()
                        onPressed: mouse.accepted = acceptMousePress

                    }
                },

                State {
                    name: "SIGNLE-CLICK"
                    when: !doubleClickEdit

                    PropertyChanges {
                        target: doubleClickArea

                        onClicked: openEdittor()
                    }
                }
            ]
        }
    }
}

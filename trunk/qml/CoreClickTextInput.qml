import QtQuick 1.1

Item {
    id: clickTextInput
    property alias text: textArea.text
    property alias font: textArea.font
    property alias style: textArea.style
    property alias styleColor: textArea.styleColor
    property alias color: textArea.color
    property bool acceptMousePress: false  //This make the double click text box accept mouse clicks (This
    property bool doubleClickEdit: false
    property bool isEditting: false
    property variant validator;
    property bool readOnly: false

    signal clicked();
    signal startedEditting()
    signal finishedEditting(string newText)

    height: textArea.height
    width: textArea.width

    function commitChanges() {
        //Emit the finishedEditting signal
        finishedEditting(globalShadowTextInput.textInput.text)

        closeEditor();
    }

    function closeEditor() {
        globalShadowTextInput.editor.visible = false;
        globalShadowTextInput.textInput.focus = false;
        globalShadowTextInput.textInput.validator = null;

        globalShadowTextInput.enabled = false
        doubleClickArea.enabled = true;
        textArea.visible = true;
        isEditting = false;

        globalShadowTextInput.commitChanges.disconnect(commitChanges)
        globalShadowTextInput.closeEditor.disconnect(closeEditor)
    }

    function openEditor() {
        clickTextInput.startedEditting()

        textArea.visible = false

        globalShadowTextInput.textInput.text = clickTextInput.text
        globalShadowTextInput.textInput.font = textArea.font
        globalShadowTextInput.editor.visible = true
        globalShadowTextInput.textInput.forceActiveFocus()
        globalShadowTextInput.textInput.selectAll();

        if(clickTextInput.validator !== undefined) {
            globalShadowTextInput.textInput.validator = clickTextInput.validator
        }

        globalShadowTextInput.enabled = true
        doubleClickArea.enabled = false
        isEditting = true

        //Set the editor's position
        var globalPosition = clickTextInput.mapToItem(globalShadowTextInput, 0, 0)
        globalShadowTextInput.editor.x = globalPosition.x - 3
        globalShadowTextInput.editor.y = globalPosition.y - 3

        globalShadowTextInput.minWidth = clickTextInput.width + 6
        globalShadowTextInput.minHeight = clickTextInput.height + 6

        //Connect to commitChanges()
        globalShadowTextInput.commitChanges.connect(commitChanges)
        globalShadowTextInput.closeEditor.connect(closeEditor)
    }

    Text {
        id: textArea

        anchors.centerIn: parent

//        onTextChanged: {
//            textChangedAnimation.restart()
//        }

//        SequentialAnimation {
//            id: textChangedAnimation
//            NumberAnimation {
//                target: textArea;
//                property: "scale";
//                easing.type: Easing.OutInElastic
//                from: 1.0;
//                to: 1.2
//                duration: 100
//            }
//            NumberAnimation {
//                target: textArea;
//                property: "scale";
//                easing.type: Easing.OutInElastic
//                to: 1.0
//                duration: 100
//            }
//        }
    }

    MouseArea {
        id: doubleClickArea

        anchors.fill: parent
        enabled:  true



        states: [
            State {
                name: "DOUBLE-CLICK"
                when: doubleClickEdit && !clickTextInput.readOnly

                PropertyChanges {
                    target: doubleClickArea
                    onDoubleClicked: openEditor()
                    onPressed: mouse.accepted = acceptMousePress
                    onClicked: clickTextInput.clicked()
                }
            },

            State {
                name: "SIGNLE-CLICK"
                when: !doubleClickEdit && !clickTextInput.readOnly

                PropertyChanges {
                    target: doubleClickArea
                    onClicked: openEditor()
                }
            }
        ]
    }
}

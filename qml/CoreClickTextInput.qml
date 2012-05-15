import QtQuick 2.0
import Cavewhere 1.0

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
    property Validator validator;
    property bool readOnly: false
    property string errorText

    signal startedEditting()
    signal finishedEditting(string newText)

    height: textArea.height
    width: textArea.width

    function commitChanges() {
        //Emit the finishedEditting signal
        if(validator !== null) {
            var accepted = 2;
            if(validator.validate(globalShadowTextInput.textInput.text) === accepted) {
                finishedEditting(globalShadowTextInput.textInput.text)
                closeEditor();
                return true;
            } else {
                globalShadowTextInput.errorHelpBox.text = validator.errorText
                globalShadowTextInput.errorHelpBox.visible = true
                globalShadowTextInput.textInput.focus = true
                return false;
            }
        } else {
            finishedEditting(globalShadowTextInput.textInput.text);
            closeEditor();
            return true;
        }
    }

    function closeEditor() {
        globalShadowTextInput.editor.visible = false;
        globalShadowTextInput.textInput.focus = false;
        globalShadowTextInput.textInput.validator = null;

        globalShadowTextInput.enabled = false
        doubleClickArea.enabled = true;
        textArea.visible = true;
        isEditting = false;
        globalShadowTextInput.errorHelpBox.visible = false;

        globalShadowTextInput.coreClickInput = null
    }

    function openEditor() {
        clickTextInput.startedEditting()

        textArea.visible = false

        globalShadowTextInput.textInput.text = clickTextInput.text
        globalShadowTextInput.textInput.font = textArea.font
        globalShadowTextInput.editor.visible = true
        globalShadowTextInput.textInput.forceActiveFocus()
        globalShadowTextInput.textInput.selectAll();

        if(clickTextInput.validator !== null) {
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
        globalShadowTextInput.coreClickInput = clickTextInput
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
                    propagateComposedEvents: !acceptMousePress
                    onDoubleClicked: openEditor()
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

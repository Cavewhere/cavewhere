/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0 as QQ
import Cavewhere 1.0

QQ.Item {
    id: clickTextInput
    property alias text: textAreaId.text
    property alias font: textAreaId.font
    property alias style: textAreaId.style
    property alias styleColor: textAreaId.styleColor
    property alias color: textAreaId.color
    property bool acceptMousePress: false  //This make the double click text box accept mouse clicks (This
    property bool doubleClickEdit: false
    property bool isEditting: false
    property Validator validator;
    property bool readOnly: false
    property bool autoResize: false
    property string errorText

    signal startedEditting()
    signal finishedEditting(string newText)

    implicitWidth: textAreaId.width
    implicitHeight: textAreaId.height

    onWidthChanged: updateTextDimensions()

    onFocusChanged: {
        if(focus) {
            if(globalShadowTextInput.coreClickInput !== null) {
                globalShadowTextInput.coreClickInput.commitChanges()
            }
            openEditor()
        }
    }

    function updateTextDimensions() {
        if(width != implicitWidth && autoResize) {
            if(textAreaId.width != width) {
                textAreaId.width = width
            }
        }
    }

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
            var newText = globalShadowTextInput.textInput.text
            closeEditor();
            finishedEditting(globalShadowTextInput.textInput.text);
            return true;
        }
    }

    function closeEditor() {
        globalShadowTextInput.editor.visible = false;
        globalShadowTextInput.textInput.focus = false;
        globalShadowTextInput.textInput.validator = null;

        globalShadowTextInput.enabled = false
        doubleClickArea.enabled = true;
        textAreaId.visible = true;
        isEditting = false;
        globalShadowTextInput.errorHelpBox.visible = false;

        globalShadowTextInput.coreClickInput = null
    }

    function openEditor() {
        clickTextInput.startedEditting()

        textAreaId.visible = false

        globalShadowTextInput.textInput.text = clickTextInput.text
        globalShadowTextInput.textInput.font = textAreaId.font
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
        id: textAreaId

//        anchors.left: parent.left
//        anchors.right: parent.right
//        anchors.margins: 3
        horizontalAlignment: Qt.AlignHCenter
        anchors.verticalCenter: parent.verticalCenter
        elide: Text.ElideRight


//        anchors.centerIn: parent


//        onTextChanged: {
//            textChangedAnimation.restart()
//        }

//        QQ.SequentialAnimation {
//            id: textChangedAnimation
//            QQ.NumberAnimation {
//                target: textAreaId;
//                property: "scale";
//                easing.type: QQ.Easing.OutInElastic
//                from: 1.0;
//                to: 1.2
//                duration: 100
//            }
//            QQ.NumberAnimation {
//                target: textAreaId;
//                property: "scale";
//                easing.type: QQ.Easing.OutInElastic
//                to: 1.0
//                duration: 100
//            }
//        }
    }

    QQ.MouseArea {
        id: doubleClickArea

        anchors.fill: parent
        enabled:  true



        states: [
            QQ.State {
                name: "DOUBLE-CLICK"
                when: doubleClickEdit && !clickTextInput.readOnly

                QQ.PropertyChanges {
                    target: doubleClickArea
                    propagateComposedEvents: !acceptMousePress
                    onDoubleClicked: openEditor()
                }
            },

            QQ.State {
                name: "SIGNLE-CLICK"
                when: !doubleClickEdit && !clickTextInput.readOnly

                QQ.PropertyChanges {
                    target: doubleClickArea
                    onClicked: openEditor()
                }
            }
        ]
    }
}

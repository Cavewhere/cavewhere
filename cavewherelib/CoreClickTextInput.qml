/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import cavewherelib

QQ.Item {
    id: clickTextInput
    objectName: "coreTextInput"
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

    signal clicked() //Emitted only when doubleClickEdit is true

    implicitWidth: textAreaId.width
    implicitHeight: textAreaId.height

    onWidthChanged: updateTextDimensions()

    onFocusChanged: {
        if(focus) {
            if(GlobalShadowTextInput.coreClickInput !== null) {
                GlobalShadowTextInput.coreClickInput.commitChanges()
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
            if(validator.validate(GlobalShadowTextInput.textInput.text) === accepted) {
                finishedEditting(GlobalShadowTextInput.textInput.text)
                closeEditor();
                return true;
            } else {
                GlobalShadowTextInput.errorHelpBox.text = validator.errorText
                GlobalShadowTextInput.errorHelpBox.visible = true
                GlobalShadowTextInput.textInput.focus = true
                return false;
            }
        } else {
            var newText = GlobalShadowTextInput.textInput.text
            closeEditor();
            finishedEditting(GlobalShadowTextInput.textInput.text);
            return true;
        }
    }

    function closeEditor() {
        GlobalShadowTextInput.editor.visible = false;
        GlobalShadowTextInput.textInput.focus = false;
        GlobalShadowTextInput.textInput.validator = null;

        GlobalShadowTextInput.enabled = false
        doubleClickArea.enabled = true;
        textAreaId.visible = true;
        isEditting = false;
        GlobalShadowTextInput.errorHelpBox.visible = false;

        GlobalShadowTextInput.coreClickInput = null
    }

    function openEditor() {
        clickTextInput.startedEditting()

        textAreaId.visible = false

        GlobalShadowTextInput.textInput.text = clickTextInput.text
        GlobalShadowTextInput.textInput.font = textAreaId.font
        GlobalShadowTextInput.editor.visible = true
        GlobalShadowTextInput.textInput.forceActiveFocus()
        GlobalShadowTextInput.textInput.selectAll();

        if(clickTextInput.validator !== null) {
            GlobalShadowTextInput.textInput.validator = clickTextInput.validator
        }

        GlobalShadowTextInput.enabled = true
        doubleClickArea.enabled = false
        isEditting = true

        //Set the editor's position
        //Calling this function with just GlobalShadowTextInput cause a crash, maybe because it's a singleton?
        //Using the parent, should be the CavewhereMainWindow
        var globalPosition = clickTextInput.mapToItem(GlobalShadowTextInput.parent, 0, 0)
        GlobalShadowTextInput.editor.x = globalPosition.x - 3
        GlobalShadowTextInput.editor.y = globalPosition.y - 3

        GlobalShadowTextInput.minWidth = clickTextInput.width + 6
        GlobalShadowTextInput.minHeight = clickTextInput.height + 6

        //Connect to commitChanges()
        GlobalShadowTextInput.coreClickInput = clickTextInput
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

    QQ.TapHandler {
        id: doubleClickArea
        enabled:  true
        gesturePolicy: QQ.TapHandler.ReleaseWithinBounds
    }

    states: [
        QQ.State {
            name: "DOUBLE-CLICK"
            when: clickTextInput.doubleClickEdit && !clickTextInput.readOnly

            QQ.PropertyChanges {
                doubleClickArea  {
                    onSingleTapped: { console.log("Click:"); clickTextInput.clicked() }
                    onDoubleTapped: { console.log("Openeditor!"); clickTextInput.openEditor() }
                }
            }
        },

        QQ.State {
            name: "SIGNLE-CLICK"
            when: !clickTextInput.doubleClickEdit && !clickTextInput.readOnly

            QQ.PropertyChanges {
                doubleClickArea {
                    onSingleTapped: clickTextInput.openEditor()
                }
            }
        }
    ]
}

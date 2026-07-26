/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import cavewherelib
import QtQuick.Controls as QC
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
    //The editor this field opens. Null gets the plain shadow editor; a field
    //with more to offer while editing names its own (see StationNameEditor).
    property QQ.Component editorComponent
    property bool readOnly: false
    property bool autoResize: false
    property alias wrapMode: textAreaId.wrapMode
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
            finishedEditting(newText);
            return true;
        }
    }

    function closeEditor() {
        GlobalShadowTextInput.closeEditor()

        doubleClickArea.enabled = true;
        textAreaId.visible = true;
        isEditting = false;
    }

    function openEditor() {
        //Open the virtual keyboard if any
        Qt.inputMethod.show()

        clickTextInput.startedEditting()

        textAreaId.visible = false

        //The editor itself — which one, where it sits, what it holds — is the
        //host's business
        if(!GlobalShadowTextInput.openEditor(clickTextInput)) {
            textAreaId.visible = true
            return
        }

        doubleClickArea.enabled = false
        isEditting = true
    }

    QC.Label {
        id: textAreaId

        //        anchors.left: parent.left
        //        anchors.right: parent.right
        //        anchors.margins: 3
        horizontalAlignment: Qt.AlignHCenter
        anchors.verticalCenter: parent.verticalCenter
        elide: QC.Label.ElideRight


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
                    onSingleTapped: { clickTextInput.clicked() }
                    onDoubleTapped: { clickTextInput.openEditor() }
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

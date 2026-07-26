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

    //The editor host for whichever window this field is in. Resolved rather
    //than named: there is one host per window, and a field can be moved
    //between windows (issue #494).
    readonly property ShadowEditorHost _shadowEditor:
        (WindowOverlay.overlay as AppOverlay)?.shadowEditor ?? null

    //The host this field actually handed its editor to. Held rather than
    //re-resolved, because _shadowEditor follows the field between windows: a
    //commit has to land in the editor holding the text, not in whichever
    //window the field has moved to since (issue #494).
    property ShadowEditorHost _openHost: null

    signal startedEditting()
    signal finishedEditting(string newText)

    signal clicked() //Emitted only when doubleClickEdit is true

    implicitWidth: textAreaId.width
    implicitHeight: textAreaId.height

    onWidthChanged: updateTextDimensions()

    onFocusChanged: {
        if(focus) {
            let openField = clickTextInput._shadowEditor?.coreClickInput ?? null
            if(openField !== null) {
                openField.commitChanges()
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
        const acceptable = 2 //QValidator.Acceptable, which Validator doesn't export

        let host = clickTextInput._openHost ?? clickTextInput._shadowEditor
        if(host === null) {
            return false;
        }

        let newText = host.editorText()

        if(validator !== null && validator.validate(newText) !== acceptable) {
            host.showError(validator.errorText)
            host.focusEditor()
            return false;
        }

        //Close before emitting, whether or not there's a validator: handlers
        //move focus and rewrite the model, and an editor still open for that
        //sees the focus leave and tries to commit a second time.
        closeEditor();
        finishedEditting(newText);
        return true;
    }

    function closeEditor() {
        let host = clickTextInput._openHost ?? clickTextInput._shadowEditor
        host?.closeEditor()
        clickTextInput._openHost = null

        doubleClickArea.enabled = true;
        textAreaId.visible = true;
        isEditting = false;
    }

    function openEditor() {
        let host = clickTextInput._shadowEditor
        if(host === null) {
            console.warn("No AppOverlay in this field's window, so there is "
                         + "nowhere to edit it", clickTextInput)
            return
        }

        //Open the virtual keyboard if any
        Qt.inputMethod.show()

        clickTextInput.startedEditting()

        textAreaId.visible = false

        //The editor itself — which one, where it sits, what it holds — is the
        //host's business
        if(!host.openEditor(clickTextInput)) {
            textAreaId.visible = true
            return
        }

        clickTextInput._openHost = host
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

import QtQuick as QQ
import cavewherelib

/**
  The live text input inside a ShadowTextEditor. It knows the field being
  edited — its editor hands that over — and nothing about who hosts the editor.
 */
QQ.TextInput {
    id: input

    //The CoreClickTextInput being edited: the target for commit / close, and
    //where tab navigation is forwarded from.
    property CoreClickTextInput field

    //FIXME: Revert back to orinial code
    //This is a work around to QTBUG-27300
    property var pressKeyEvent

    signal pressKeyPressed; //This is emitted every time key is pressed
    signal enterPressed()
    signal escapePressed()

    anchors.centerIn: parent;

    selectByMouse: activeFocus;
    activeFocusOnPress: false
    selectionColor: Theme.highlight
    color: Theme.text

    function defaultKeyHandling() {
        if(input.pressKeyEvent.key === Qt.Key_Return || input.pressKeyEvent.key === Qt.Key_Enter) {
            input.enterPressed()
            if(input.field !== null) {
                input.field.commitChanges()
            } else {
                input.escapePressed()
            }

        } else if(input.pressKeyEvent.key === Qt.Key_Escape) {
            input.escapePressed()
            if(input.field !== null) {
                input.field.closeEditor();
            }
            input.pressKeyEvent.accepted = true
        }
    }

    QQ.KeyNavigation.tab: {
        if(input.field === null) {
            return null
        }
        return input.field.QQ.KeyNavigation.tab
    }

    QQ.KeyNavigation.backtab: {
        if(input.field === null) {
            return null
        }
        return input.field.QQ.KeyNavigation.backtab
    }

    //activeFocus, not focus: the editor is loaded inside a QQ.Loader, which is a
    //focus scope, so `focus` stays true once set even after the user moves on.
    onActiveFocusChanged: {
        if(input.field === null) {
            return
        }

        //visible goes false with the editor, so closing never commits
        if(!input.activeFocus && input.visible && !input.field.focus) {
            input.field.commitChanges();
        }
    }

    onFocusChanged: {
        if(input.field !== null && input.field.focus) {
            input.forceActiveFocus()
            input.selectAll()
        }
    }

    QQ.Keys.onPressed: (event) => {
        input.pressKeyEvent = event;
        input.pressKeyPressed();
    }

    onPressKeyPressed: {
        input.defaultKeyHandling();
    }
}

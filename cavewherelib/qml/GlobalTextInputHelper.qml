import QtQuick as QQ

QQ.TextInput {
    id: input

    // required property GlobalShadowTextInput globalMouseArea

    visible: GlobalShadowTextInput.editor.visible
    anchors.centerIn: parent;

    selectByMouse: activeFocus;
    activeFocusOnPress: false

    //FIXME: Revert back to orinial code
    //This is a work around to QTBUG-27300
    property var pressKeyEvent
    signal pressKeyPressed; //This is emitted every time key is pressed

    QQ.KeyNavigation.tab: {
        if(GlobalShadowTextInput.coreClickInput === null) {
            return null
        }
        return GlobalShadowTextInput.coreClickInput.QQ.KeyNavigation.tab
    }

    QQ.KeyNavigation.backtab: {
        if(GlobalShadowTextInput.coreClickInput === null) {
            return null
        }
        return GlobalShadowTextInput.coreClickInput.QQ.KeyNavigation.backtab
    }

    onFocusChanged: {
        if(!focus && GlobalShadowTextInput.editor.visible && !GlobalShadowTextInput.coreClickInput.focus) {
            GlobalShadowTextInput.coreClickInput.commitChanges();
        }

        if(GlobalShadowTextInput.coreClickInput !== null && GlobalShadowTextInput.coreClickInput.focus)
        {
            forceActiveFocus()
            selectAll()
        }
    }

    function defaultKeyHandling() {
        if(pressKeyEvent.key === Qt.Key_Return || pressKeyEvent.key === Qt.Key_Enter) {
            enterPressed()
            if(coreClickInput !== null) {
                coreClickInput.commitChanges()
            } else {
                escapePressed()
            }

        } else if(pressKeyEvent.key === Qt.Key_Escape) {
            escapePressed()
            coreClickInput.closeEditor();
            pressKeyEvent.accepted = true
        }
    }

    QQ.Keys.onPressed: (event) => {
        pressKeyEvent = event;
        pressKeyPressed();
    }

    onPressKeyPressed: {
        defaultKeyHandling();
    }
}

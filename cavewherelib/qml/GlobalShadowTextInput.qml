/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/
pragma Singleton

import QtQuick as QQ

QQ.MouseArea {
    id: globalMouseArea

    property alias textInput: input
    property alias editor: shadowEditor
    property int minWidth: 0
    property int minHeight: 0
    property alias errorHelpBox: errorHelpBoxItem
    property CoreClickTextInput coreClickInput

    signal enterPressed()
    signal escapePressed()

    anchors.fill: parent
    enabled: false
    visible: enabled

    onPressed: (mouse) => {
        if(coreClickInput !== null) {
            var commited = coreClickInput.commitChanges()
            if(!commited) {
                coreClickInput.closeEditor()
            }
        }

        mouse.accepted = false
    }

    function clearSelection() {
        input.select(input.cursorPosition, input.cursorPosition)
    }

    ShadowRectangle {
        id: shadowEditor
        visible: false;

        color: "white"

        width:  globalMouseArea.minWidth > input.width + 6 ? globalMouseArea.minWidth : input.width  + 6
        height: globalMouseArea.minHeight > input.height + 6 ? globalMouseArea.minHeight : input.height + 6

        QQ.MouseArea {
            id: borderArea

            anchors.fill: parent

            onPressed: (mouse) => {
                input.forceActiveFocus()
                mouse.accepted = true
            }
        }

        GlobalTextInputHelper {
            id: input
        }

        // QQ.TextInput {
        //     id: input
        //     visible: shadowEditor.visible
        //     anchors.centerIn: parent;

        //     selectByMouse: activeFocus;
        //     activeFocusOnPress: false

        //     //FIXME: Revert back to orinial code
        //     //This is a work around to QTBUG-27300
        //     property var pressKeyEvent
        //     signal pressKeyPressed; //This is emitted every time key is pressed

        //     QQ.KeyNavigation.tab: {
        //         if(globalMouseArea.coreClickInput === null) {
        //             return null
        //         }
        //         return globalMouseArea.coreClickInput.QQ.KeyNavigation.tab
        //     }

        //     QQ.KeyNavigation.backtab: {
        //         if(globalMouseArea.coreClickInput === null) {
        //             return null
        //         }
        //         return globalMouseArea.coreClickInput.QQ.KeyNavigation.backtab
        //     }

        //     onFocusChanged: {
        //         if(!focus && globalMouseArea.editor.visible && !globalMouseArea.coreClickInput.focus) {
        //             globalMouseArea.coreClickInput.commitChanges();
        //         }

        //         if(globalMouseArea.coreClickInput !== null && globalMouseArea.coreClickInput.focus)
        //         {
        //             forceActiveFocus()
        //             selectAll()
        //         }
        //     }

        //     function defaultKeyHandling() {
        //         if(pressKeyEvent.key === Qt.Key_Return || pressKeyEvent.key === Qt.Key_Enter) {
        //             enterPressed()
        //             if(coreClickInput !== null) {
        //                 coreClickInput.commitChanges()
        //             } else {
        //                 escapePressed()
        //             }

        //         } else if(pressKeyEvent.key === Qt.Key_Escape) {
        //             escapePressed()
        //             coreClickInput.closeEditor();
        //             pressKeyEvent.accepted = true
        //         }
        //     }

        //     QQ.Keys.onPressed: (event) => {
        //         pressKeyEvent = event;
        //         pressKeyPressed();
        //     }

        //     onPressKeyPressed: {
        //         defaultKeyHandling();
        //     }
        // }

        ErrorHelpBox {
            id: errorHelpBoxItem
            y: parent.height + 10
            visible: false
            anchors.bottom: undefined
            anchors.bottomMargin: 0
        }
    }

}

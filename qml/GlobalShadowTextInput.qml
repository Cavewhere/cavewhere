/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0

MouseArea {
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

    onPressed: {
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

        width:  minWidth > input.width + 6 ? minWidth : input.width  + 6
        height: minHeight > input.height + 6 ? minHeight : input.height + 6

        MouseArea {
            id: borderArea

            anchors.fill: parent

            onPressed: {
                input.forceActiveFocus()
                mouse.accepted = true
            }
        }

        TextInput {
            id: input
            visible: shadowEditor.visible
            anchors.centerIn: parent;

            selectByMouse: activeFocus;
            activeFocusOnPress: false

            //FIXME: Revert back to orinial code
            //This is a work around to QTBUG-27300
            property int pressKeyEvent
            signal pressKeyPressed; //This is emitted every time key is pressed

            function defaultKeyHandling() {
                if(pressKeyEvent === Qt.Key_Return || pressKeyEvent === Qt.Key_Enter) {
                    enterPressed()
                    if(coreClickInput !== null) {
                        coreClickInput.commitChanges()
                    } else {
                        escapePressed()
                    }

//                    event.accepted = commited
                } else if(pressKeyEvent === Qt.Key_Escape) {
                    escapePressed()
                    coreClickInput.closeEditor();
//                    event.accepted = true
                }
            }

            Keys.onPressed: {
                pressKeyEvent = event.key;
                pressKeyPressed();
//                defaultKeyHandling(event)
            }

            onPressKeyPressed: {
                defaultKeyHandling();
            }
        }

        ErrorHelpBox {
            id: errorHelpBoxItem
            y: parent.height + 10
            visible: false
            anchors.bottom: undefined
            anchors.bottomMargin: 0
        }
    }


}

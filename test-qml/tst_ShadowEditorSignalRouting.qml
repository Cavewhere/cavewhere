pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtTest
import cavewherelib

// The host re-emits the live editor's key/focus signals so consumers never hold
// the editor itself. This mirrors how DataBox picks up Tab/Space/Enter while a
// cell is being typed into: a handler bound straight to the editor's text input
// dies the first time a field asks for a different editor, because the loader
// destroys and rebuilds both.
QQ.Item {
    id: rootId
    width: 400
    height: 200

    //Resolved and listened to the way DataBox does it
    readonly property ShadowEditorHost _host:
        (WindowOverlay.overlay as AppOverlay)?.shadowEditor ?? null
    property int keyHits: 0

    AppOverlay {
        id: overlayId
    }

    QQ.Connections {
        target: rootId._host
        enabled: rootId.state === "Typing"

        function onPressKeyPressed(pressKeyEvent) { rootId.keyHits++ }
    }

    ClickTextInput {
        id: plainFieldId
        text: "plain"
    }

    QQ.Item {
        id: focusSinkId
    }

    ClickTextInput {
        id: stationFieldId
        text: "a1"

        editorComponent: EditorComponents.stationName
    }

    states: [
        QQ.State {
            name: "Typing"
        }
    ]

    TestCase {
        name: "ShadowEditorSignalRouting"
        when: windowShown

        function init() {
            if(overlayId.shadowEditor.coreClickInput !== null) {
                overlayId.shadowEditor.coreClickInput.closeEditor()
            }
            rootId.state = ""
            rootId.keyHits = 0
        }

        function test_hostKeySignalSurvivesAnEditorSwap() {
            //Load the station editor, then leave it loaded, exactly as editing a
            //note station before touching the survey table would
            stationFieldId.openEditor()
            stationFieldId.closeEditor()
            let staleInput = overlayId.shadowEditor.currentEditor.textInput
            verify(staleInput !== null)

            //DataBox enters the state before it opens its editor
            rootId.state = "Typing"

            plainFieldId.openEditor()
            let liveInput = overlayId.shadowEditor.currentEditor.textInput
            verify(liveInput !== staleInput,
                   "expected the loader to swap editors, or this proves nothing")

            //The editor's own handler reads this off the event it was given
            liveInput.pressKeyEvent = { key: Qt.Key_A, accepted: false }
            liveInput.pressKeyPressed()
            compare(rootId.keyHits, 1,
                    "host key signal must survive the editor being replaced")
        }

        //The host reports focus leaving the editor, which is what returns
        //DataBox to its default state. The Loader is a focus scope, so this
        //has to follow activeFocus — `focus` stays true once set.
        function test_hostReportsFocusLeavingTheEditor() {
            let reported = []
            function onEditorFocus(editorFocus) { reported.push(editorFocus) }
            overlayId.shadowEditor.editorFocusChanged.connect(onEditorFocus)

            plainFieldId.openEditor()
            tryVerify(() => overlayId.shadowEditor.editorHasFocus(), 1000,
                      "the editor should take active focus when opened")

            focusSinkId.forceActiveFocus()
            tryVerify(() => reported.indexOf(false) >= 0, 1000,
                      "losing active focus must be reported through the host")

            overlayId.shadowEditor.editorFocusChanged.disconnect(onEditorFocus)
        }
    }
}

pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtTest
import cavewherelib

// The host re-emits the live editor's key/focus signals so consumers never hold
// the editor itself. This mirrors how DataBox's "MiddleTyping" state installs
// its Tab/Space/Enter handling: a QQ.PropertyChanges resolves its target object
// once and never re-resolves, so a handler bound straight to
// GlobalShadowTextInput.textInput dies the first time a field asks for a
// different editor — permanently, since re-entering the state won't re-resolve.
QQ.Item {
    id: rootId
    width: 400
    height: 200

    //Held the way DataBox holds it: naming the singleton directly inside
    //PropertyChanges parses as an attached object.
    property GlobalShadowTextInput _host: GlobalShadowTextInput
    property int keyHits: 0

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

            QQ.PropertyChanges {
                rootId._host.onPressKeyPressed: { rootId.keyHits++ }
            }
        }
    ]

    TestCase {
        name: "ShadowEditorSignalRouting"
        when: windowShown

        function init() {
            if(GlobalShadowTextInput.coreClickInput !== null) {
                GlobalShadowTextInput.coreClickInput.closeEditor()
            }
            rootId.state = ""
            rootId.keyHits = 0
        }

        function test_hostKeySignalSurvivesAnEditorSwap() {
            //Load the station editor, then leave it loaded, exactly as editing a
            //note station before touching the survey table would
            stationFieldId.openEditor()
            stationFieldId.closeEditor()
            let staleInput = GlobalShadowTextInput.textInput
            verify(staleInput !== null)

            //DataBox enters the state before it opens its editor
            rootId.state = "Typing"

            plainFieldId.openEditor()
            let liveInput = GlobalShadowTextInput.textInput
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
            //Put the host in the scene the way MainContent does, or nothing
            //here has real active focus
            GlobalShadowTextInput.parent = rootId

            let reported = []
            function onEditorFocus(editorFocus) { reported.push(editorFocus) }
            GlobalShadowTextInput.editorFocusChanged.connect(onEditorFocus)

            plainFieldId.openEditor()
            tryVerify(() => GlobalShadowTextInput.textInput.activeFocus, 1000,
                      "the editor should take active focus when opened")

            focusSinkId.forceActiveFocus()
            tryVerify(() => reported.indexOf(false) >= 0, 1000,
                      "losing active focus must be reported through the host")

            GlobalShadowTextInput.editorFocusChanged.disconnect(onEditorFocus)
        }
    }
}

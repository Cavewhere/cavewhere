pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtTest
import cavewherelib

// Fields talk to the shared editor through GlobalShadowTextInput's own verbs,
// never by reaching into the editor it loaded. These cover the two behaviours
// that contract has to keep: a rejected value leaves the editor open and
// focused, and a commit tears the editor down before it reports the new text.
QQ.Item {
    id: rootId
    width: 400
    height: 200

    StationValidator {
        id: stationValidatorId
    }

    ClickTextInput {
        id: plainFieldId
        text: "plain"
    }

    ClickTextInput {
        id: validatedFieldId
        text: "a1"

        validator: stationValidatorId
    }

    TestCase {
        name: "ShadowEditorHostContract"
        when: windowShown

        function init() {
            //Parented in the way MainContent does it, or nothing here has real
            //active focus to lose or take back
            GlobalShadowTextInput.parent = rootId
        }

        //These tests deliberately leave the editor open, and an editor still
        //focused while the fields are torn down blurs onto a dead field.
        function cleanup() {
            if(GlobalShadowTextInput.coreClickInput !== null) {
                GlobalShadowTextInput.coreClickInput.closeEditor()
            }
        }

        function test_rejectedTextKeepsTheEditorOpenAndFocused() {
            validatedFieldId.openEditor()
            tryVerify(() => GlobalShadowTextInput.editorHasFocus(), 1000)

            GlobalShadowTextInput.setEditorText("not a station name")

            compare(validatedFieldId.commitChanges(), false,
                    "a value the validator rejects must not commit")
            compare(GlobalShadowTextInput.coreClickInput, validatedFieldId,
                    "the field keeps the editor so the user can fix the value")
            verify(GlobalShadowTextInput.hasError(),
                   "the reason it was rejected has to be on screen")
            //focus stays true under the Loader's focus scope, so only
            //forceActiveFocus puts the cursor back — hence editorHasFocus()
            tryVerify(() => GlobalShadowTextInput.editorHasFocus(), 1000,
                      "typing should continue where the user left off")
        }

        function test_commitClosesTheEditorBeforeReportingTheNewText() {
            //Both branches, since they used to disagree: a handler moves focus
            //and rewrites the model, and an editor still open for that sees the
            //focus leave and commits a second time.
            let stillOpen = []
            function record() {
                stillOpen.push(GlobalShadowTextInput.coreClickInput !== null)
            }

            plainFieldId.finishedEditting.connect(record)
            validatedFieldId.finishedEditting.connect(record)

            plainFieldId.openEditor()
            GlobalShadowTextInput.setEditorText("plain2")
            compare(plainFieldId.commitChanges(), true)

            validatedFieldId.openEditor()
            GlobalShadowTextInput.setEditorText("a2")
            compare(validatedFieldId.commitChanges(), true)

            plainFieldId.finishedEditting.disconnect(record)
            validatedFieldId.finishedEditting.disconnect(record)

            compare(stillOpen, [false, false],
                    "finishedEditting must run with the editor already closed")
        }
    }
}

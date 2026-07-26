pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtTest
import cavewherelib

// Fields talk to the shared editor through ShadowEditorHost's own verbs,
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

    AppOverlay {
        id: overlayId
    }

    TestCase {
        name: "ShadowEditorHostContract"
        when: windowShown

        //These tests deliberately leave the editor open, and an editor still
        //focused while the fields are torn down blurs onto a dead field.
        function cleanup() {
            if(overlayId.shadowEditor.coreClickInput !== null) {
                overlayId.shadowEditor.coreClickInput.closeEditor()
            }
        }

        function test_rejectedTextKeepsTheEditorOpenAndFocused() {
            validatedFieldId.openEditor()
            tryVerify(() => overlayId.shadowEditor.editorHasFocus(), 1000)

            overlayId.shadowEditor.setEditorText("not a station name")

            compare(validatedFieldId.commitChanges(), false,
                    "a value the validator rejects must not commit")
            compare(overlayId.shadowEditor.coreClickInput, validatedFieldId,
                    "the field keeps the editor so the user can fix the value")
            verify(overlayId.shadowEditor.hasError(),
                   "the reason it was rejected has to be on screen")
            //focus stays true under the Loader's focus scope, so only
            //forceActiveFocus puts the cursor back — hence editorHasFocus()
            tryVerify(() => overlayId.shadowEditor.editorHasFocus(), 1000,
                      "typing should continue where the user left off")
        }

        function test_commitClosesTheEditorBeforeReportingTheNewText() {
            //Both branches, since they used to disagree: a handler moves focus
            //and rewrites the model, and an editor still open for that sees the
            //focus leave and commits a second time.
            let stillOpen = []
            function record() {
                stillOpen.push(overlayId.shadowEditor.coreClickInput !== null)
            }

            plainFieldId.finishedEditting.connect(record)
            validatedFieldId.finishedEditting.connect(record)

            plainFieldId.openEditor()
            overlayId.shadowEditor.setEditorText("plain2")
            compare(plainFieldId.commitChanges(), true)

            validatedFieldId.openEditor()
            overlayId.shadowEditor.setEditorText("a2")
            compare(validatedFieldId.commitChanges(), true)

            plainFieldId.finishedEditting.disconnect(record)
            validatedFieldId.finishedEditting.disconnect(record)

            compare(stillOpen, [false, false],
                    "finishedEditting must run with the editor already closed")
        }
    }
}

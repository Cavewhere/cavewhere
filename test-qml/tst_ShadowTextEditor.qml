pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtTest
import cavewherelib

// The shared editor host loads the editor the field asked for. A field that
// names none gets the plain ShadowTextEditor; a field that names one gets that,
// which is how station-name autocomplete reaches editing without every other
// field carrying it. Fields naming the SAME editor share one instance.
QQ.Item {
    id: rootId
    width: 400
    height: 200

    ClickTextInput {
        id: plainFieldId
        text: "plain"
    }

    ClickTextInput {
        id: stationFieldId
        text: "a1"

        editorComponent: EditorComponents.stationName
    }

    ClickTextInput {
        id: otherStationFieldId
        text: "a2"

        editorComponent: EditorComponents.stationName
    }

    //Deliberately declared after the fields: they resolve their host before it
    //exists, so this also covers a field recovering once the overlay registers.
    AppOverlay {
        id: overlayId
    }

    TestCase {
        name: "ShadowTextEditor"
        when: windowShown

        function init() {
            if(overlayId.shadowEditor.coreClickInput !== null) {
                overlayId.shadowEditor.coreClickInput.closeEditor()
            }
            overlayId.shadowEditor.enabled = false
        }

        function test_0_fieldsResolveTheHostInTheirOwnWindow() {
            verify(overlayId.shadowEditor !== null, "the overlay carries a host")
            //verify, not compare: compare deep-walks two Items when they differ
            //and dies with "Maximum call stack size exceeded" instead of saying so
            verify(plainFieldId._shadowEditor === overlayId.shadowEditor,
                   "a field declared before the overlay must still find it")
        }

        //The station editor carries this; the plain one has no such property.
        function isStationEditor() {
            return overlayId.shadowEditor.currentEditor.stationField !== undefined
        }

        function test_fieldWithoutEditorComponentGetsThePlainEditor() {
            //Seed with the other editor first: the loader already holds the
            //plain one at startup, so opening straight into it would assert
            //nothing about what openEditor() actually loads.
            stationFieldId.openEditor()
            stationFieldId.closeEditor()
            verify(isStationEditor(), "expected the station editor to be loaded first")

            plainFieldId.openEditor()

            verify(overlayId.shadowEditor.currentEditor !== null)
            compare(overlayId.shadowEditor.currentEditor.field, plainFieldId)
            compare(overlayId.shadowEditor.editorText(), "plain")
            compare(overlayId.shadowEditor.enabled, true)

            verify(!isStationEditor(),
                   "the plain editor must not carry any autocomplete machinery")
        }

        function test_stationFieldOpensTheStationEditor() {
            //Same reasoning as above, from the other side.
            plainFieldId.openEditor()
            plainFieldId.closeEditor()
            verify(!isStationEditor(), "expected the plain editor to be loaded first")

            stationFieldId.openEditor()

            compare(overlayId.shadowEditor.currentEditor.field, stationFieldId)
            compare(overlayId.shadowEditor.editorText(), "a1")
            verify(isStationEditor())
        }

        //Two fields of the same kind must hand the host the same component, or
        //it rebuilds the whole editor on every move between them.
        function test_fieldsOfOneKindShareTheEditorInstance() {
            stationFieldId.openEditor()
            let firstEditor = overlayId.shadowEditor.currentEditor
            stationFieldId.closeEditor()

            otherStationFieldId.openEditor()

            compare(overlayId.shadowEditor.currentEditor, firstEditor,
                    "station fields should reuse one editor instance")
            compare(overlayId.shadowEditor.currentEditor.field, otherStationFieldId)
            compare(overlayId.shadowEditor.editorText(), "a2")
        }

        function test_editorSwapsWithTheFieldBeingEdited() {
            stationFieldId.openEditor()
            let stationEditor = overlayId.shadowEditor.currentEditor
            stationFieldId.closeEditor()

            plainFieldId.openEditor()
            verify(overlayId.shadowEditor.currentEditor !== stationEditor)
            verify(!isStationEditor())
        }

        function test_closingReleasesTheField() {
            plainFieldId.openEditor()
            plainFieldId.closeEditor()

            compare(overlayId.shadowEditor.enabled, false)
            compare(overlayId.shadowEditor.coreClickInput, null)
            compare(overlayId.shadowEditor.currentEditor.field, null)
        }
    }
}

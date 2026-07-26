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

    TestCase {
        name: "ShadowTextEditor"
        when: windowShown

        function init() {
            if(GlobalShadowTextInput.coreClickInput !== null) {
                GlobalShadowTextInput.coreClickInput.closeEditor()
            }
            GlobalShadowTextInput.enabled = false
        }

        //The station editor carries this; the plain one has no such property.
        function isStationEditor() {
            return GlobalShadowTextInput.currentEditor.stationField !== undefined
        }

        function test_fieldWithoutEditorComponentGetsThePlainEditor() {
            //Seed with the other editor first: the loader already holds the
            //plain one at startup, so opening straight into it would assert
            //nothing about what openEditor() actually loads.
            stationFieldId.openEditor()
            stationFieldId.closeEditor()
            verify(isStationEditor(), "expected the station editor to be loaded first")

            plainFieldId.openEditor()

            verify(GlobalShadowTextInput.currentEditor !== null)
            compare(GlobalShadowTextInput.currentEditor.field, plainFieldId)
            compare(GlobalShadowTextInput.editorText(), "plain")
            // editor.visible isn't assertable here: the host is only parented
            // into a scene by MainContent, so effective visibility is always
            // false in a bare-component test. enabled is the editing flag.
            compare(GlobalShadowTextInput.enabled, true)

            verify(!isStationEditor(),
                   "the plain editor must not carry any autocomplete machinery")
        }

        function test_stationFieldOpensTheStationEditor() {
            //Same reasoning as above, from the other side.
            plainFieldId.openEditor()
            plainFieldId.closeEditor()
            verify(!isStationEditor(), "expected the plain editor to be loaded first")

            stationFieldId.openEditor()

            compare(GlobalShadowTextInput.currentEditor.field, stationFieldId)
            compare(GlobalShadowTextInput.editorText(), "a1")
            verify(isStationEditor())
        }

        //Two fields of the same kind must hand the host the same component, or
        //it rebuilds the whole editor on every move between them.
        function test_fieldsOfOneKindShareTheEditorInstance() {
            stationFieldId.openEditor()
            let firstEditor = GlobalShadowTextInput.currentEditor
            stationFieldId.closeEditor()

            otherStationFieldId.openEditor()

            compare(GlobalShadowTextInput.currentEditor, firstEditor,
                    "station fields should reuse one editor instance")
            compare(GlobalShadowTextInput.currentEditor.field, otherStationFieldId)
            compare(GlobalShadowTextInput.editorText(), "a2")
        }

        function test_editorSwapsWithTheFieldBeingEdited() {
            stationFieldId.openEditor()
            let stationEditor = GlobalShadowTextInput.currentEditor
            stationFieldId.closeEditor()

            plainFieldId.openEditor()
            verify(GlobalShadowTextInput.currentEditor !== stationEditor)
            verify(!isStationEditor())
        }

        function test_closingReleasesTheField() {
            plainFieldId.openEditor()
            plainFieldId.closeEditor()

            compare(GlobalShadowTextInput.enabled, false)
            compare(GlobalShadowTextInput.coreClickInput, null)
            compare(GlobalShadowTextInput.currentEditor.field, null)
        }
    }
}

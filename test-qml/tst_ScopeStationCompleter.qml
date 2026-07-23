import QtQuick as QQ
import QtTest
import cavewherelib

// Logic tests for the note-entry station autocomplete. The match / ghost /
// navigation behavior over real rows is covered at the C++ level
// (test_cwScopeStationListModel: matchingStations, containsStation), since a
// populated cwScopeStationListModel needs a solved cave lookup that is not
// reachable from pure QML. These cover the pure-QML reactive contract: the
// completer stays inert for every non-station field, and the out-of-scope
// advisory lights up without blocking.
QQ.Item {
    id: rootId
    width: 400
    height: 200

    // Non-null but unpopulated: no rows, so every typed name is out of scope.
    ScopeStationListModel {
        id: emptyModelId
    }

    GlobalTextInputHelper {
        id: inputId
    }

    ScopeStationCompleter {
        id: completerId
        textInput: inputId
    }

    TestCase {
        name: "ScopeStationCompleter"
        when: windowShown

        // active gates on the editor field being shown; force our standalone
        // helper visible (its default binding tracks the shadow-editor singleton,
        // which isn't opened in this bare-component test).
        function openEditor() {
            inputId.visible = true
        }

        function init() {
            completerId.scopeModel = null
            inputId.text = ""
            inputId.visible = false
        }

        function cleanup() {
            inputId.visible = false
        }

        function test_inertWithoutModel() {
            // A field that supplies no scope model (every non-station field)
            // leaves the completer fully inert regardless of what's typed.
            openEditor()
            inputId.text = "a1"
            tryCompare(completerId, "active", false)
            tryCompare(completerId, "dropdownVisible", false)
            tryCompare(completerId, "hasSuggestion", false)
            tryCompare(completerId, "outOfScope", false)
        }

        function test_outOfScopeAdvisory() {
            // A station field whose typed name matches nothing in scope raises
            // the non-blocking advisory (dropdown shows it; no ghost).
            completerId.scopeModel = emptyModelId
            openEditor()
            inputId.text = "zz"
            tryCompare(completerId, "active", true)
            tryCompare(completerId, "typedText", "zz")
            compare(completerId.matches.length, 0)
            tryCompare(completerId, "outOfScope", true)
            tryCompare(completerId, "dropdownVisible", true)
            tryCompare(completerId, "hasSuggestion", false)
        }

        function test_emptyTextIsQuiet() {
            // Nothing typed yet: no advisory, no dropdown.
            completerId.scopeModel = emptyModelId
            openEditor()
            inputId.text = ""
            tryCompare(completerId, "outOfScope", false)
            tryCompare(completerId, "dropdownVisible", false)
        }

        function test_clearingModelReturnsToInert() {
            completerId.scopeModel = emptyModelId
            openEditor()
            inputId.text = "zz"
            tryCompare(completerId, "active", true)

            completerId.scopeModel = null
            tryCompare(completerId, "active", false)
            tryCompare(completerId, "dropdownVisible", false)
        }
    }
}

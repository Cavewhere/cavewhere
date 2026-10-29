import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

// EN.2 — marking, labeling and removing an entrance from the survey table's
// station caret. Every assertion reads back through the cave's cwEntranceModel.
MainWindowTest {
    id: rootId

    SurveyTableTestHelper {
        id: surveyTableId
        mainWindow: rootId.mainWindow
        testWindow: rootId
    }

    TestCase {
        name: "MarkStationEntrance"
        when: windowShown

        function init() {
            surveyTableId.resetToViewPage(this)
        }

        function cleanup() {
            surveyTableId.leaveSurveyTable()
        }

        function gotoSurveyTable() {
            const context = surveyTableId.openSurveyTable(this, "EntranceCave", "EntranceTrip")

            surveyTableId.setStationName(this, context, 0, "A1")
            surveyTableId.setStationName(this, context, 1, "A2")

            return context
        }

        // A station cell, scrolled into view, for reading its badges.
        function stationBox(context, indexInChunk) {
            const row = surveyTableId.stationRow(context, indexInChunk)
            verify(row >= 0, "station " + indexInChunk + " should have a model row")
            return surveyTableId.cell(this, context, row, SurveyChunk.StationNameRole)
        }

        // The station cell for `indexInChunk`, focused — the caret only loads on
        // a focused cell.
        function focusedStationBox(context, indexInChunk) {
            const row = surveyTableId.stationRow(context, indexInChunk)
            context.editorModel.setFocusedCell(
                        context.editorModel.cellIndex(row, SurveyChunk.StationNameRole))

            const box = stationBox(context, indexInChunk)
            tryVerify(() => box.focus, 5000, "station box " + indexInChunk + " should be focused")
            return box
        }

        function entranceBadge(box) {
            const badge = findChild(box, "entranceStationBadge")
            verify(badge !== null, "station cell should carry an entrance badge")
            return badge
        }

        // Opens the station caret menu and returns its fix and entrance items.
        function openStationMenu(box) {
            let caret = null
            tryVerify(() => {
                caret = findChild(box, "fixStationMenuButton")
                return caret !== null
            }, 5000, "focused station cell should offer a caret")

            caret.clicked()

            let items = null
            tryVerify(() => {
                items = {
                    markFixed: findChild(box, "markStationFixedMenuItem"),
                    removeFix: findChild(box, "removeStationFixMenuItem"),
                    markEntrance: findChild(box, "markStationEntranceMenuItem"),
                    removeEntrance: findChild(box, "removeStationEntranceMenuItem")
                }
                return items.markFixed !== null && items.removeFix !== null
                        && items.markEntrance !== null && items.removeEntrance !== null
            }, 5000, "station caret menu should have the fix and entrance items")

            return items
        }

        function surveyEditorPopup(objectName) {
            // A Popup isn't in the survey editor's item tree until it opens.
            const surveyEditor = surveyTableId.surveyEditor()
            verify(surveyEditor !== null, "survey editor should be reachable")

            let popup = null
            tryVerify(() => {
                popup = findChild(surveyEditor, objectName)
                return popup !== null
            }, 5000, "survey editor should host " + objectName)
            return popup
        }

        function entrancePopup() {
            return surveyEditorPopup("entrancePopup")
        }

        function typeText(text) {
            for (let i = 0; i < text.length; ++i) {
                keyClick(text[i])
            }
        }

        function closePopup(popup) {
            popup.close()
            tryCompare(popup, "opened", false)
        }

        function labelAt(entrances, row) {
            return entrances.data(entrances.index(row), EntranceModel.LabelRole)
        }

        // Marks A1 an entrance through the caret, labeled `label` first when it
        // is set, and returns the open popup's label field.
        function openEntranceEditor(context, label) {
            const entrances = context.cave.entrances
            if (label !== "") {
                entrances.addEntrance("A1")
                entrances.setData(entrances.index(0), label, EntranceModel.LabelRole)
            }

            const menu = openStationMenu(focusedStationBox(context, 0))
            menu.markEntrance.triggered()

            const popup = entrancePopup()
            tryCompare(popup, "opened", true)
            compare(popup.stationName, "A1")

            const field = findChild(popup, "entrancePopupLabel")
            verify(field !== null, "popup should offer the label field")
            return field
        }

        // Replaces the field's text the way a user does: select all, then type.
        function retype(field, text) {
            field.forceActiveFocus()
            tryCompare(field, "activeFocus", true)
            field.selectAll()
            if (text === "") {
                keyClick(Qt.Key_Delete)
            } else {
                typeText(text)
            }
            compare(field.text, text)
        }

        function pressDone() {
            const popup = entrancePopup()
            const done = findChild(popup, "entrancePopupDone")
            verify(done !== null, "popup should offer a way to close")
            done.clicked()
            tryCompare(popup, "opened", false)
        }

        function test_markStationEntranceShowsBadge() {
            const context = gotoSurveyTable()
            const entrances = context.cave.entrances
            compare(entrances.count, 0)

            const box = focusedStationBox(context, 0)
            const badge = entranceBadge(box)
            compare(badge.visible, false)

            const menu = openStationMenu(box)
            compare(menu.markEntrance.text, "Mark Station as Entrance")
            compare(menu.markEntrance.enabled, true)
            compare(menu.removeEntrance.enabled, false)

            menu.markEntrance.triggered()

            tryCompare(entrances, "count", 1)
            verify(entrances.isEntrance("A1"))
            compare(context.cave.fixStations.count, 0, "marking an entrance adds no fix")

            tryCompare(badge, "visible", true)
            compare(badge.source.toString(), Theme.kindIconCave.toString())

            const popup = entrancePopup()
            tryCompare(popup, "opened", true)
            compare(popup.stationName, "A1")
            compare(popup.row, 0)
            const title = findChild(popup, "entrancePopupTitle")
            verify(title !== null, "popup should name the station")
            compare(title.text, "Entrance A1")

            // The unmarked station's badge stays hidden and loads no image.
            const otherBadge = entranceBadge(stationBox(context, 1))
            compare(otherBadge.visible, false)
            compare(otherBadge.source.toString(), "")

            closePopup(popup)
        }

        function test_typedLabelLandsInModel() {
            const context = gotoSurveyTable()
            const entrances = context.cave.entrances

            const field = openEntranceEditor(context, "")
            compare(field.text, "EntranceCave")

            retype(field, "Main Entrance")
            pressDone()

            compare(entrances.count, 1)
            tryVerify(() => labelAt(entrances, 0) === "Main Entrance",
                      5000, "the typed label should land in the entrance model")
            compare(entrances.data(entrances.index(0), EntranceModel.StationNameRole), "A1")
        }

        function test_editEntranceReopensWithLabel() {
            const context = gotoSurveyTable()
            const entrances = context.cave.entrances
            entrances.addEntrance("A1")
            entrances.setData(entrances.index(0), "Upper Entrance", EntranceModel.LabelRole)

            const box = focusedStationBox(context, 0)
            tryCompare(entranceBadge(box), "visible", true)

            const menu = openStationMenu(box)
            compare(menu.markEntrance.text, "Edit Entrance...")
            compare(menu.removeEntrance.enabled, true)

            menu.markEntrance.triggered()

            const popup = entrancePopup()
            tryCompare(popup, "opened", true)
            compare(popup.row, 0)
            compare(findChild(popup, "entrancePopupLabel").text, "Upper Entrance")
            compare(entrances.count, 1, "editing an entrance adds no row")

            closePopup(popup)
            compare(labelAt(entrances, 0), "Upper Entrance")
        }

        function test_unlabeledEntranceOpensWithCaveName() {
            const context = gotoSurveyTable()
            const entrances = context.cave.entrances

            const field = openEntranceEditor(context, "")
            compare(field.text, "EntranceCave",
                    "an unlabeled entrance shows the cave name its marker uses")

            pressDone()
            compare(entrances.count, 1)
            compare(labelAt(entrances, 0), "",
                    "closing on the cave name keeps the label empty, so it follows the cave")
        }

        function test_clearedFieldPreviewsCaveName() {
            const context = gotoSurveyTable()

            const field = openEntranceEditor(context, "Upper Entrance")
            compare(field.text, "Upper Entrance")
            compare(field.placeholderText, "EntranceCave",
                    "a cleared field previews the cave name the marker falls back to")

            closePopup(entrancePopup())
        }

        function test_caveNameInOtherCapitalizationStoresEmpty() {
            const context = gotoSurveyTable()
            const entrances = context.cave.entrances

            const field = openEntranceEditor(context, "Upper Entrance")
            retype(field, " entrancecave ")
            pressDone()

            compare(entrances.count, 1)
            compare(labelAt(entrances, 0), "",
                    "the cave name in any capitalization stores no custom label")
        }

        function test_clearingStoresEmpty() {
            const context = gotoSurveyTable()
            const entrances = context.cave.entrances

            const cleared = openEntranceEditor(context, "Upper Entrance")
            retype(cleared, "")
            pressDone()
            compare(labelAt(entrances, 0), "", "a cleared field stores no custom label")

            entrances.setData(entrances.index(0), "Upper Entrance", EntranceModel.LabelRole)
            const blank = openEntranceEditor(context, "")
            compare(blank.text, "Upper Entrance")
            retype(blank, "   ")
            pressDone()
            compare(labelAt(entrances, 0), "", "a field of only spaces stores no custom label")
        }

        function test_otherTextStoresTrimmedLabel() {
            const context = gotoSurveyTable()
            const entrances = context.cave.entrances

            const field = openEntranceEditor(context, "")
            retype(field, "  Main Entrance  ")
            pressDone()

            compare(entrances.count, 1)
            compare(labelAt(entrances, 0), "Main Entrance",
                    "text other than the cave name is stored trimmed as the label")
        }

        function test_removeEntranceHidesBadge() {
            const context = gotoSurveyTable()
            const entrances = context.cave.entrances
            entrances.addEntrance("A1")

            const box = focusedStationBox(context, 0)
            const badge = entranceBadge(box)
            tryCompare(badge, "visible", true)

            const menu = openStationMenu(box)
            compare(menu.removeEntrance.text, "Remove Entrance")
            compare(menu.removeEntrance.enabled, true)
            menu.removeEntrance.triggered()

            tryCompare(entrances, "count", 0)
            tryCompare(badge, "visible", false)
            compare(badge.source.toString(), "")
        }

        function test_markFixedShowsBothBadges() {
            const context = gotoSurveyTable()

            const box = focusedStationBox(context, 0)
            const fixedBadge = findChild(box, "fixedStationBadge")
            verify(fixedBadge !== null, "station cell should carry a Fixed badge")
            const badge = entranceBadge(box)

            const menu = openStationMenu(box)
            menu.markFixed.triggered()

            tryCompare(context.cave.fixStations, "count", 1)
            tryCompare(context.cave.entrances, "count", 1)
            tryCompare(fixedBadge, "visible", true)
            tryCompare(badge, "visible", true)

            closePopup(surveyEditorPopup("fixStationPopup"))
        }

        function test_removeFixHidesBoth() {
            const context = gotoSurveyTable()
            context.cave.addFixedEntrance("A1")
            tryCompare(context.cave.fixStations, "count", 1)
            tryCompare(context.cave.entrances, "count", 1)

            const box = focusedStationBox(context, 0)
            const fixedBadge = findChild(box, "fixedStationBadge")
            const badge = entranceBadge(box)
            tryCompare(fixedBadge, "visible", true)
            tryCompare(badge, "visible", true)

            const menu = openStationMenu(box)
            menu.removeFix.triggered()

            tryCompare(context.cave.fixStations, "count", 0)
            tryCompare(context.cave.entrances, "count", 0)
            tryCompare(fixedBadge, "visible", false)
            tryCompare(badge, "visible", false)
        }
    }
}

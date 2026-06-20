import QtQuick
import QtTest
import QmlTestRecorder
import cavewherelib

Item {
    id: rootId
    objectName: "rootId"

    width: 600
    height: 600

    // A writable palette to edit: fork the read-only shipped default.
    property SymbologyPalette forkPalette: null

    // The manager resolves the default palette through a sketch (defaultPalette()
    // isn't exposed to QML); resolvedPalette is the fork source.
    Sketch {
        id: sketchId
    }

    Component {
        id: editorComponent
        GlyphEditorPage {
            objectName: "glyphEditorPage"
            anchors.fill: parent
        }
    }

    TestCase {
        id: testCaseId
        name: "GlyphEditor"
        when: windowShown

        function drawStroke(item, points) {
            mousePress(item, points[0].x, points[0].y, Qt.LeftButton)
            for (let i = 1; i < points.length; i++) {
                mouseMove(item, points[i].x, points[i].y, 0, Qt.LeftButton)
            }
            const last = points[points.length - 1]
            mouseRelease(item, last.x, last.y, Qt.LeftButton)
        }

        function init() {
            tryVerify(() => sketchId.resolvedPalette !== null, 2000,
                      "the default palette resolves")
            rootId.forkPalette = RootData.paletteManager.duplicatePalette(
                sketchId.resolvedPalette, "Glyph Editor Test")
            verify(rootId.forkPalette !== null, "fork should be created")
            verify(rootId.forkPalette.writable, "fork should be writable")
        }

        function cleanup() {
            // Drop the fork so it doesn't accumulate in the process-wide manager
            // singleton's palette list across test functions.
            if (rootId.forkPalette) {
                RootData.paletteManager.removePalette(rootId.forkPalette)
                rootId.forkPalette = null
            }
        }

        // Draw on a blank draft, name it (which auto-saves), and confirm the
        // glyph round-trips back into a freshly opened editor's canvas.
        function test_draftAutoSaveAndReopen() {
            const editor = editorComponent.createObject(rootId, { palette: rootId.forkPalette })
            verify(editor !== null, "editor page should instantiate")

            // Master-detail structure: list rail + editor pane overlays.
            verify(findChild(editor, "glyphList") !== null, "glyph list")
            verify(findChild(editor, "addGlyphButton") !== null, "add button")
            verify(findChild(editor, "paperExtentOverlay") !== null, "paper extent overlay")
            verify(findChild(editor, "originCrosshair") !== null, "origin crosshair overlay")
            verify(findChild(editor, "tangentAxis") !== null, "tangent axis overlay")

            const canvas = findChild(editor, "glyphCanvas")
            const inputArea = findChild(editor, "glyphInputArea")
            verify(canvas !== null && inputArea !== null)
            compare(canvas.sketch.strokeCount(), 0, "starts blank")

            // Wait for the SplitView to lay out before drawing — a press onto a
            // not-yet-sized input area would map to the wrong place.
            tryVerify(() => inputArea.width > 0 && inputArea.height > 0, 2000,
                      "editor pane laid out")

            const helpBox = findChild(editor, "glyphNameHelpBox")
            verify(helpBox !== null, "name help box exists")
            verify(!helpBox.visible, "help box hidden on a fresh blank draft")

            // Draw one stroke, below the top-left chrome and within the editor
            // pane (the left rail narrows it). With no name yet the draft can't
            // save, so the name field is flagged.
            drawStroke(inputArea, [Qt.point(120, 400), Qt.point(180, 410),
                                   Qt.point(240, 400)])
            tryVerify(() => canvas.sketch.strokeCount() === 1, 2000,
                      "a stroke is captured")
            tryVerify(() => helpBox.visible, 2000,
                      "drawing an unnamed draft flags the name field")

            const glyphList = findChild(editor, "glyphList")
            const baseline = glyphList.count

            // Name it and commit by ending the edit (Enter). Auto-save materializes
            // the glyph; the flag clears and the row appears in the list. The
            // display name ("Zzz…") sorts it last, so a passing selection check
            // can't be a coincidence of the default currentIndex 0.
            const nameField = findChild(editor, "glyphNameField")
            nameField.forceActiveFocus()
            nameField.text = "test-mark"
            findChild(editor, "glyphDisplayNameField").text = "Zzz Test Mark"
            keyClick(Qt.Key_Return)

            tryVerify(() => !helpBox.visible, 2000, "naming clears the flag")
            tryVerify(() => glyphList.count === baseline + 1, 2000,
                      "the saved glyph appears in the list")

            // The model re-sorts on save, so the highlight must re-anchor to the
            // just-named glyph rather than drift onto a neighboring row.
            tryVerify(() => glyphList.currentIndex === glyphList.model.indexOfName("test-mark"),
                      2000, "the new glyph is selected after auto-save")

            editor.destroy()

            // Reopen on the (still-valid) fork and confirm the stroke persisted.
            const editor2 = editorComponent.createObject(rootId, { palette: rootId.forkPalette })
            verify(editor2 !== null)
            const canvas2 = findChild(editor2, "glyphCanvas")
            canvas2.loadGlyphNamed("test-mark")
            tryVerify(() => canvas2.sketch.strokeCount() === 1, 2000,
                      "the saved glyph reloads with its stroke")
            editor2.destroy()
        }

        // Selecting an existing glyph from the list loads it into the editor.
        function test_selectExistingGlyph() {
            const editor = editorComponent.createObject(rootId, { palette: rootId.forkPalette })
            verify(editor !== null)

            const glyphList = findChild(editor, "glyphList")
            tryVerify(() => glyphList.count > 0, 2000, "the fork ships seed glyphs")
            tryVerify(() => glyphList.itemAtIndex(0) !== null, 2000,
                      "first glyph row is realized")
            mouseClick(glyphList.itemAtIndex(0))

            const nameField = findChild(editor, "glyphNameField")
            tryVerify(() => nameField.text.length > 0, 2000,
                      "selecting a row loads its name into the editor")
            compare(editor.currentName, nameField.text,
                    "currentName tracks the selected glyph")

            editor.destroy()
        }

        // Right-click → Duplicate copies a row into "<name>-copy" and selects it.
        function test_duplicateGlyphViaContextMenu() {
            const editor = editorComponent.createObject(rootId, { palette: rootId.forkPalette })
            verify(editor !== null)

            const glyphList = findChild(editor, "glyphList")
            tryVerify(() => glyphList.count > 0, 2000, "the fork ships seed glyphs")
            tryVerify(() => glyphList.itemAtIndex(0) !== null, 2000, "first row realized")

            // Reach a realized row's context menu and read the glyph it carries,
            // so the assertion doesn't depend on which delegate findChild returns.
            const ctxMenu = findChild(glyphList, "glyphContextMenu")
            verify(ctxMenu !== null, "glyphContextMenu must exist")
            const sourceName = ctxMenu.name
            verify(sourceName.length > 0, "context menu carries a glyph name")
            const copyName = sourceName + "-copy"

            const baseline = glyphList.count
            // Item 0 is Duplicate; trigger it directly (popup is flaky offscreen).
            const duplicateItem = ctxMenu.menu.itemAt(0)
            verify(duplicateItem !== null && duplicateItem.text === "Duplicate",
                   "first item is Duplicate")
            duplicateItem.triggered()

            tryVerify(() => glyphList.count === baseline + 1, 2000,
                      "the duplicate appears in the list")
            verify(glyphList.model.indexOfName(copyName) >= 0,
                   "the copy is named <source>-copy")
            tryVerify(() => editor.currentName === copyName, 2000,
                      "the duplicate is selected for editing")
            // The original survives the copy.
            verify(glyphList.model.indexOfName(sourceName) >= 0, "the original remains")

            editor.destroy()
        }

        // Right-click → Remove → confirm drops the glyph from the palette.
        function test_removeGlyphViaContextMenu() {
            const editor = editorComponent.createObject(rootId, { palette: rootId.forkPalette })
            verify(editor !== null)

            const glyphList = findChild(editor, "glyphList")
            tryVerify(() => glyphList.count > 0, 2000, "the fork ships seed glyphs")
            tryVerify(() => glyphList.itemAtIndex(0) !== null, 2000, "first row realized")

            const ctxMenu = findChild(glyphList, "glyphContextMenu")
            verify(ctxMenu !== null, "glyphContextMenu must exist")
            const removeName = ctxMenu.name
            const baseline = glyphList.count

            // Item 1 is "Remove <name>"; triggering it opens the confirmation box.
            const removeItem = ctxMenu.menu.itemAt(1)
            verify(removeItem !== null && removeItem.text.indexOf("Remove") === 0,
                   "second item is Remove")
            removeItem.triggered()

            const removeAskBox = findChild(editor, "removeChallange")
            verify(removeAskBox !== null, "RemoveAskBox must exist")
            tryVerify(() => removeAskBox.visible, 2000, "Remove opens the confirmation")
            compare(removeAskBox.removeName, removeName, "the box names the glyph")

            mouseClick(findChild(removeAskBox, "removeButton"))
            tryVerify(() => !removeAskBox.visible, 2000, "confirming closes the box")
            tryVerify(() => glyphList.count === baseline - 1, 2000, "the glyph is removed")
            compare(glyphList.model.indexOfName(removeName), -1, "its row is gone")

            editor.destroy()
        }

        // Renaming a saved glyph saves the new name and drops the old one, so the
        // palette never accumulates an orphaned copy (task #45).
        function test_renameDoesNotOrphanOldGlyph() {
            const editor = editorComponent.createObject(rootId, { palette: rootId.forkPalette })
            verify(editor !== null)

            const inputArea = findChild(editor, "glyphInputArea")
            const glyphList = findChild(editor, "glyphList")
            tryVerify(() => inputArea.width > 0 && inputArea.height > 0, 2000,
                      "editor pane laid out")

            // Draw and name a fresh glyph so the rename has a deterministic source.
            drawStroke(inputArea, [Qt.point(120, 400), Qt.point(180, 410),
                                   Qt.point(240, 400)])
            tryVerify(() => findChild(editor, "glyphCanvas").sketch.strokeCount() === 1, 2000)

            const nameField = findChild(editor, "glyphNameField")
            nameField.forceActiveFocus()
            nameField.text = "rename-src"
            findChild(editor, "glyphDisplayNameField").text = "Zzz Rename"
            keyClick(Qt.Key_Return)
            tryVerify(() => glyphList.model.indexOfName("rename-src") >= 0, 2000,
                      "the source glyph saved")
            const afterSave = glyphList.count

            // Rename: edit the name field and commit. Save-new + drop-old.
            nameField.forceActiveFocus()
            nameField.text = "rename-dst"
            keyClick(Qt.Key_Return)

            tryVerify(() => glyphList.model.indexOfName("rename-dst") >= 0, 2000,
                      "the renamed glyph exists")
            tryVerify(() => glyphList.model.indexOfName("rename-src") === -1, 2000,
                      "the old name is not orphaned")
            compare(glyphList.count, afterSave, "rename leaves the glyph count unchanged")
            compare(editor.currentName, "rename-dst", "the editor follows the rename")

            editor.destroy()
        }
    }
}

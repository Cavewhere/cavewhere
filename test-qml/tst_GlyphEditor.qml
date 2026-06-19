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

        function test_drawSaveReopen() {
            const editor = editorComponent.createObject(rootId, { palette: rootId.forkPalette })
            verify(editor !== null, "editor page should instantiate")

            // Overlays exist.
            verify(findChild(editor, "paperExtentOverlay") !== null, "paper extent overlay")
            verify(findChild(editor, "originCrosshair") !== null, "origin crosshair overlay")
            verify(findChild(editor, "tangentAxis") !== null, "tangent axis overlay")

            const canvas = findChild(editor, "glyphCanvas")
            const inputArea = findChild(editor, "glyphInputArea")
            verify(canvas !== null && inputArea !== null)
            compare(canvas.sketch.strokeCount(), 0, "starts blank")

            // Draw one stroke, away from the top-left chrome.
            drawStroke(inputArea, [Qt.point(300, 350), Qt.point(360, 360),
                                   Qt.point(420, 350)])
            tryVerify(() => canvas.sketch.strokeCount() === 1, 2000,
                      "a stroke is captured")

            // Name and save.
            findChild(editor, "glyphNameField").text = "test-mark"
            const saveButton = findChild(editor, "glyphSaveButton")
            verify(saveButton.enabled, "Save enabled once a name is entered")
            mouseClick(saveButton)

            editor.destroy()

            // Reopen the same glyph on the (still-valid) fork and confirm it
            // round-tripped through saveGlyph + reload.
            const editor2 = editorComponent.createObject(
                rootId, { palette: rootId.forkPalette, glyphName: "test-mark" })
            verify(editor2 !== null)
            const canvas2 = findChild(editor2, "glyphCanvas")
            tryVerify(() => canvas2.sketch.strokeCount() === 1, 2000,
                      "the saved glyph reloads with its stroke")
            editor2.destroy()
        }
    }
}

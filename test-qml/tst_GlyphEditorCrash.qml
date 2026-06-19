import QtQuick
import QtTest
import QmlTestRecorder
import cavewherelib

// Regression for the glyph-editor navigation crash:
//   ASSERT: dynamic_cast<QQuickItem*>(object) != nullptr  (cwPageView.cpp:231)
// Steps: open a new sketch, click "Duplicate to edit…". The toolbar registers
// GlyphEditorPage and gotoPage() makes cwPageView instantiate it. Originally the
// page component was declared inline in SketchToolbar.qml (which is
// `ComponentBehavior: Bound`), so cwPageView could not instantiate it outside
// the toolbar's creation context and the assert aborted.
Item {
    id: rootId
    objectName: "rootId"

    width: 800
    height: 600

    Sketch {
        id: sketchId
    }

    // A real page view bound to the shared selection model so gotoPage()
    // actually instantiates the page through cwPageView (where the crash was).
    PageView {
        id: pageViewId
        objectName: "pageView"
        anchors.fill: parent
        pageSelectionModel: RootData.pageSelectionModel
    }

    SketchCanvas {
        id: canvasId
        objectName: "canvas"
        anchors.fill: parent
        sketch: sketchId
    }

    SketchToolbar {
        id: toolbarId
        objectName: "sketchToolbar"
        sketch: sketchId
        canvas: canvasId
    }

    TestCase {
        name: "GlyphEditorCrash"
        when: windowShown

        function cleanup() {
            // Drop the page we registered on the shared model so the next test
            // starts clean.
            if (toolbarId._glyphEditorPage) {
                RootData.pageSelectionModel.unregisterPage(toolbarId._glyphEditorPage)
            }
        }

        function test_duplicatePaletteOpensEditor() {
            tryVerify(() => sketchId.resolvedPalette !== null, 2000,
                      "default palette resolves")

            const button = findChild(toolbarId, "duplicatePaletteButton")
            verify(button !== null, "Duplicate to edit… button exists")
            verify(button.visible, "button is visible for a read-only palette")

            // Before the fix this aborts in cwPageView::createChildItemFromComponent.
            mouseClick(button)

            tryVerify(() => pageViewId.currentPageItem !== null
                          && pageViewId.currentPageItem.objectName === "glyphEditorPage",
                      3000, "glyph editor page instantiates and is shown")
        }
    }
}

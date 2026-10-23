import cw.TestLib
import QtTest
import cavewherelib
import QmlTestRecorder

// The 3D view's EDL outline follows the theme: black on the light background,
// the lighter Theme.viewBackground.edlOutline gray on the dark one.
MainWindowTest {
    id: rootId

    TestCase {
        name: "EDLOutlineTheme"
        when: windowShown

        function cleanup() {
            RootData.settings.appearanceSettings.colorScheme = AppearanceSettings.System
        }

        function edl() {
            return RootData.regionSceneManager.scene.edl
        }

        function test_outlineFollowsColorScheme() {
            let glTerrain = ObjectFinder.findObjectByChain(rootId.mainWindow,
                "rootId->viewPage->SplitView->renderer");
            verify(glTerrain, "found the GLTerrainRenderer");

            RootData.settings.appearanceSettings.colorScheme = AppearanceSettings.Dark
            tryVerify(() => Theme.dark)
            tryVerify(() => Qt.colorEqual(edl().outlineColor, "#8a96a3"), 5000,
                      "dark scheme draws the gray outline")

            RootData.settings.appearanceSettings.colorScheme = AppearanceSettings.Light
            tryVerify(() => !Theme.dark)
            tryVerify(() => Qt.colorEqual(edl().outlineColor, "#000000"), 5000,
                      "light scheme draws the black outline")
        }
    }
}

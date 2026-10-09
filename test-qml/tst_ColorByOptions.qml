import QtQuick as QQ
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

MainWindowTest {
    id: rootId

    TestCase {
        name: "ColorByOptions"
        when: windowShown

        readonly property int depthIndex: 1

        function findLegend() {
            TestHelper.loadProjectFromFile(RootData.project,
                TestHelper.testcasesDatasetPath("test_cwProject/Phake Cave 3000.cw"));
            RootData.pageSelectionModel.gotoPageByName(null, "View");

            let legend = null;
            tryVerify(() => {
                legend = ObjectFinder.findObjectByChain(rootId.mainWindow,
                    "rootId->viewPage->SplitView->renderer->colorByLegend");
                return legend !== null;
            }, 5000, "found the color-by legend");
            return legend;
        }

        function findPopup() {
            let popup = findChild(rootId.mainWindow, "colorByPopup");
            verify(popup, "found the color-by popup");
            return popup;
        }

        function cleanup() {
            RootData.regionSceneManager.colorBy.source = ColorBy.None;
            let popup = findChild(rootId.mainWindow, "colorByPopup");
            if (popup) {
                popup.close();
            }
        }

        function test_chipOpensPopupAndPicksDepth() {
            RootData.regionSceneManager.colorBy.source = ColorBy.None;

            let legend = findLegend();
            tryVerify(() => legend.visible, 5000, "legend is visible");

            let chip = findChild(legend, "colorByChip");
            verify(chip, "found the chip");
            tryVerify(() => chip.visible, 5000, "chip shows while color-by is off");

            let popup = findPopup();
            verify(!popup.opened);

            mouseClick(legend);
            tryVerify(() => popup.opened, 5000, "a click opens the popup");

            let sourceCombo = findChild(popup.contentItem, "colorBySourceCombo");
            verify(sourceCombo, "found the source combo");
            sourceCombo.currentIndex = depthIndex;
            sourceCombo.activated(depthIndex);
            tryCompare(RootData.regionSceneManager.colorBy, "source", ColorBy.Depth);

            let bar = findChild(legend, "colorByBar");
            verify(bar, "found the colorbar");
            tryVerify(() => !chip.visible, 5000, "chip hides once color-by is on");
            tryVerify(() => bar.visible, 5000, "colorbar shows once color-by is on");

            let strips = findChild(legend, "colorByStrips");
            verify(strips, "found the strip repeater");
            compare(strips.count, legend.stripCount);

            keyClick(Qt.Key_Escape);
            tryVerify(() => !popup.opened, 5000, "Escape closes the popup");

            mouseClick(legend, legend.width / 2, legend.height / 2, Qt.RightButton);
            tryVerify(() => popup.opened, 5000, "a right-click opens the popup");
        }

        function test_sourceComboListsKeysButHidesIdKeys() {
            let legend = findLegend();
            let popup = findPopup();
            mouseClick(legend);
            tryVerify(() => popup.opened, 5000, "a click opens the popup");

            let sourceCombo = findChild(popup.contentItem, "colorBySourceCombo");
            verify(sourceCombo, "found the source combo");
            tryVerify(() => sourceCombo.model.indexOf("Cave") >= 0, 5000, "the Cave key is listed");
            verify(RootData.keywordFilterPipelineModel.possibleKeys.indexOf("Object id") >= 0,
                   "the project carries the Object id key");
            let hidden = RootData.regionSceneManager.colorBy.hiddenKeywordKeys;
            compare(hidden, ["Object id", "File Name"]);
            for (let key of hidden) {
                compare(sourceCombo.model.indexOf(key), -1, key + " is hidden");
            }
        }
    }
}

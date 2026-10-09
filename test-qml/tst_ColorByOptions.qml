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
        readonly property real feetPerMeter: 3.28084
        readonly property real pivotTolerance: 0.01
        readonly property real edgeTolerance: 1
        property int savedUnitSystem: Units.Metric
        property real savedPivot: 0

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

        function openPopup() {
            let legend = findLegend();
            let popup = findPopup();
            mouseClick(legend);
            tryVerify(() => popup.opened, 5000, "a click opens the popup");
            return popup;
        }

        function selectDepth(popup) {
            let sourceCombo = findChild(popup.contentItem, "colorBySourceCombo");
            verify(sourceCombo, "found the source combo");
            sourceCombo.currentIndex = depthIndex;
            sourceCombo.activated(depthIndex);
            tryCompare(RootData.regionSceneManager.colorBy, "source", ColorBy.Depth);
        }

        function openPopupWithDepth() {
            let popup = openPopup();
            selectDepth(popup);
            return popup;
        }

        function init() {
            savedUnitSystem = RootData.region.unitSystem;
            savedPivot = RootData.regionSceneManager.colorBy.pivot;
        }

        function cleanup() {
            RootData.region.unitSystem = savedUnitSystem;
            RootData.regionSceneManager.colorBy.pivot = savedPivot;
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

            selectDepth(popup);

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
            let popup = openPopup();

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

        function test_pivotIsTypeableInProjectUnits() {
            let popup = openPopupWithDepth();
            let colorBy = RootData.regionSceneManager.colorBy;
            const pivotMeters = 10;
            colorBy.pivot = pivotMeters;
            RootData.region.unitSystem = Units.Imperial;

            let unitLabel = findChild(popup.contentItem, "colorByPivotUnit");
            verify(unitLabel, "found the pivot unit label");
            tryCompare(unitLabel, "text", "ft");

            let spin = findChild(popup.contentItem, "colorByPivotSpin");
            verify(spin, "found the pivot spin");
            verify(spin.editable, "the pivot spin accepts typing");
            tryCompare(spin, "value", Math.round(pivotMeters * feetPerMeter));

            const typedFeet = 150;
            // The grid lays out the depth rows on the next polish
            let sourceCombo = findChild(popup.contentItem, "colorBySourceCombo");
            tryVerify(() => spin.mapToItem(popup.contentItem, 0, 0).y > sourceCombo.y,
                      5000, "the pivot row is laid out below the source row");
            mouseClick(spin.contentItem);
            tryVerify(() => spin.contentItem.activeFocus, 5000, "the spin's text field has focus");
            spin.contentItem.selectAll();
            for (let digit of String(typedFeet)) {
                keyClick(digit);
            }
            keyClick(Qt.Key_Return);

            tryVerify(() => Math.abs(colorBy.pivot - typedFeet / feetPerMeter) < pivotTolerance,
                      5000, "the typed feet land in meters");
            compare(spin.value, typedFeet);
        }

        function test_legendShowsRoundedTicks() {
            let popup = openPopupWithDepth();
            RootData.region.unitSystem = Units.Metric;
            popup.close();

            let legend = findLegend();
            let ticks = findChild(legend, "colorByTicks");
            verify(ticks, "found the tick repeater");
            tryVerify(() => ticks.count >= 3, 5000, "the legend shows at least three ticks");

            const labelEndsInMeters = () => {
                for (let i = 0; i < ticks.count; ++i) {
                    let tick = ticks.itemAt(i);
                    for (let j = 0; j < tick.children.length; ++j) {
                        const text = tick.children[j].text;
                        if (text !== undefined && text.endsWith(" m")) {
                            return true;
                        }
                    }
                }
                return false;
            };
            tryVerify(labelEndsInMeters, 5000, "a tick label reads in meters");
        }

        function clickEditPill(legend, popup) {
            let pill = findChild(legend, "colorByEditButton");
            verify(pill, "found the Edit pill");
            tryVerify(() => pill.visible && pill.width > 0, 5000, "the Edit pill is showing");
            verify(!popup.opened);
            mouseClick(pill);
            tryVerify(() => popup.opened, 5000, "the Edit pill opens the popup");
            keyClick(Qt.Key_Escape);
            tryVerify(() => !popup.opened, 5000, "Escape closes the popup");
        }

        function test_editPillOpensPopup() {
            let colorBy = RootData.regionSceneManager.colorBy;
            colorBy.source = ColorBy.None;

            let legend = findLegend();
            let popup = findPopup();
            let chip = findChild(legend, "colorByChip");
            verify(chip, "found the chip");
            tryVerify(() => chip.visible, 5000, "chip shows while color-by is off");
            clickEditPill(legend, popup);

            colorBy.source = ColorBy.Depth;
            let bar = findChild(legend, "colorByBar");
            verify(bar, "found the colorbar");
            tryVerify(() => bar.visible, 5000, "colorbar shows once color-by is on");
            let title = findChild(legend, "colorByTitle");
            verify(title, "found the legend title");
            tryCompare(title, "text", "Depth");
            clickEditPill(legend, popup);
        }

        function test_legendSitsAboveCompass() {
            let legend = findLegend();
            let renderer = legend.parent;
            let compass = findChild(renderer, "compass");
            verify(compass, "found the compass");
            let compassRow = compass.parent;

            tryVerify(() => {
                const legendBottom = legend.mapToItem(renderer, 0, legend.height).y;
                const compassRowTop = compassRow.mapToItem(renderer, 0, 0).y;
                return legend.height > 0 && legendBottom <= compassRowTop;
            }, 5000, "the legend sits above the compass row");

            const legendRight = legend.mapToItem(renderer, legend.width, 0).x;
            const compassRowRight = compassRow.mapToItem(renderer, compassRow.width, 0).x;
            fuzzyCompare(legendRight, compassRowRight, edgeTolerance);
        }
    }
}

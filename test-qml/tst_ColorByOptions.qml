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
        readonly property int loopErrorIndex: 2
        readonly property int sigmaTickCount: 4
        readonly property real feetPerMeter: 3.28084
        readonly property real pivotTolerance: 0.01
        readonly property real edgeTolerance: 1
        readonly property int bodyClickWait: 200
        property int savedUnitSystem: Units.Metric

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

        function findEditPill(legend) {
            let pill = findChild(legend, "colorByEditButton");
            verify(pill, "found the Edit pill");
            tryVerify(() => pill.visible && pill.width > 0, 5000, "the Edit pill is showing");
            return pill;
        }

        function pillLabel(pill) {
            let label = findChild(pill, "editLinkLabel");
            verify(label, "found the pill label");
            return label;
        }

        function openPopup() {
            let legend = findLegend();
            let popup = findPopup();
            mouseClick(findEditPill(legend));
            tryVerify(() => popup.opened, 5000, "the Edit pill opens the popup");
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
        }

        function cleanup() {
            RootData.region.unitSystem = savedUnitSystem;
            RootData.regionSceneManager.colorBy.clearPivotManual();
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

            mouseClick(findEditPill(legend));
            tryVerify(() => popup.opened, 5000, "the Edit pill opens the popup");

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
            colorBy.pivotManual = pivotMeters;
            RootData.region.unitSystem = Units.Imperial;

            let unitLabel = findChild(popup.contentItem, "colorByPivotUnit");
            verify(unitLabel, "found the pivot unit label");
            tryCompare(unitLabel, "text", "ft");

            let spin = findChild(popup.contentItem, "colorByPivotSpin");
            verify(spin, "found the pivot spin");
            verify(spin.editable, "the pivot spin accepts typing");
            tryCompare(spin, "value", Math.round(pivotMeters * feetPerMeter));

            const typedFeet = 150;
            typeIntoSpin(popup, spin, String(typedFeet));

            tryVerify(() => Math.abs(colorBy.pivotManual - typedFeet / feetPerMeter) < pivotTolerance,
                      5000, "the typed feet land in meters");
            verify(colorBy.hasPivotManual);
            compare(spin.value, typedFeet);
        }

        function typeIntoSpin(popup, spin, text) {
            let sourceCombo = findChild(popup.contentItem, "colorBySourceCombo");
            tryVerify(() => spin.mapToItem(popup.contentItem, 0, 0).y > sourceCombo.y,
                      5000, "the pivot row is laid out below the source row");
            mouseClick(spin.contentItem);
            tryVerify(() => spin.contentItem.activeFocus, 5000, "the spin's text field has focus");
            spin.contentItem.selectAll();
            for (let character of text) {
                keyClick(character);
            }
            keyClick(Qt.Key_Return);
        }

        function test_pivotDefaultsToMiddleAndAutoResets() {
            RootData.region.unitSystem = Units.Metric;
            let colorBy = RootData.regionSceneManager.colorBy;
            colorBy.clearPivotManual();
            let popup = openPopupWithDepth();

            let spin = findChild(popup.contentItem, "colorByPivotSpin");
            verify(spin, "found the pivot spin");
            let autoButton = findChild(popup.contentItem, "colorByPivotAutoButton");
            verify(autoButton, "found the Auto button");

            tryVerify(() => colorBy.rangeMax > colorBy.rangeMin, 5000, "the depth range is known");
            const midpoint = Math.round((colorBy.rangeMin + colorBy.rangeMax) / 2);
            verify(!colorBy.hasPivotManual);
            verify(!autoButton.visible, "Auto hides while the pivot is automatic");
            tryCompare(spin, "value", midpoint);

            const typed = midpoint + 7;
            typeIntoSpin(popup, spin, String(typed));
            tryVerify(() => colorBy.hasPivotManual, 5000, "typing sets a manual pivot");
            fuzzyCompare(colorBy.pivotManual, typed, pivotTolerance);
            tryVerify(() => autoButton.visible, 5000, "Auto shows once the pivot is manual");

            mouseClick(autoButton);
            tryVerify(() => !colorBy.hasPivotManual, 5000, "Auto clears the manual pivot");
            tryCompare(spin, "value", midpoint);
            tryVerify(() => !autoButton.visible, 5000, "Auto hides again");
        }

        function test_pivotRowOnlyForDepth() {
            let colorBy = RootData.regionSceneManager.colorBy;
            let popup = openPopup();
            let spin = findChild(popup.contentItem, "colorByPivotSpin");
            verify(spin, "found the pivot spin");

            colorBy.source = ColorBy.None;
            tryVerify(() => !spin.visible, 5000, "no pivot while color-by is off");
            colorBy.keywordKey = "Cave";
            colorBy.source = ColorBy.Keyword;
            tryVerify(() => !spin.visible, 5000, "no pivot for keyword coloring");
            colorBy.source = ColorBy.Depth;
            tryVerify(() => spin.visible, 5000, "the pivot shows for depth");
        }

        function test_bodyClickDoesNotOpenPopup() {
            let colorBy = RootData.regionSceneManager.colorBy;
            colorBy.source = ColorBy.None;

            let legend = findLegend();
            let popup = findPopup();
            let chip = findChild(legend, "colorByChip");
            verify(chip, "found the chip");
            tryVerify(() => chip.visible && chip.width > 0, 5000, "chip shows while color-by is off");

            mouseClick(chip);
            wait(bodyClickWait);
            verify(!popup.opened, "a click on the chip body leaves the popup closed");

            mouseClick(findEditPill(legend));
            tryVerify(() => popup.opened, 5000, "the Edit pill opens the popup");
        }

        function test_editReadsDoneAndCloses() {
            let legend = findLegend();
            let popup = findPopup();
            let pill = findEditPill(legend);
            let label = pillLabel(pill);
            compare(label.text, "Edit");

            mouseClick(pill);
            tryVerify(() => popup.opened, 5000, "the Edit pill opens the popup");
            tryCompare(label, "text", "Done");

            mouseClick(pill);
            tryVerify(() => !popup.opened, 5000, "the Done pill closes the popup");
            tryCompare(label, "text", "Edit");
        }

        function test_pressOutsideCloses() {
            let popup = openPopup();
            let legend = findLegend();
            let renderer = legend.parent;

            mouseClick(renderer, renderer.width / 2, renderer.height / 2);
            tryVerify(() => !popup.opened, 5000, "a press away from the popup closes it");
        }

        function test_legendShowsRoundedTicks() {
            let ticks = showDepthTicks();

            const labelEndsInMeters = () => {
                for (let i = 0; i < ticks.count; ++i) {
                    if (tickLabel(ticks.itemAt(i)).text.endsWith(" m")) {
                        return true;
                    }
                }
                return false;
            };
            tryVerify(labelEndsInMeters, 5000, "a tick label reads in meters");

            // The range is snapped to the tick interval, so the end ticks label the bar's ends
            let colorBy = RootData.regionSceneManager.colorBy;
            compare(tickLabel(ticks.itemAt(0)).text, colorBy.formatDepth(colorBy.rangeMin));
            compare(tickLabel(ticks.itemAt(colorBy.legendTicks.length - 1)).text,
                    colorBy.formatDepth(colorBy.rangeMax));
        }

        function tickLabel(tick) {
            for (let i = 0; i < tick.children.length; ++i) {
                if (tick.children[i].text !== undefined) {
                    return tick.children[i];
                }
            }
            return null;
        }

        function tickMark(tick) {
            for (let i = 0; i < tick.children.length; ++i) {
                if (tick.children[i].text === undefined) {
                    return tick.children[i];
                }
            }
            return null;
        }

        function showDepthTicks() {
            let popup = openPopupWithDepth();
            RootData.region.unitSystem = Units.Metric;
            popup.close();

            let legend = findLegend();
            let ticks = findChild(legend, "colorByTicks");
            verify(ticks, "found the tick repeater");
            tryVerify(() => ticks.count >= 3, 5000, "the legend shows at least three ticks");
            return ticks;
        }

        function test_tickMarksSpanTheBar() {
            let ticks = showDepthTicks();
            let legend = findLegend();
            for (let i = 0; i < ticks.count; ++i) {
                let mark = tickMark(ticks.itemAt(i));
                verify(mark, "tick " + i + " has a mark");
                compare(mark.x, 0);
                verify(mark.width >= legend.barWidth, "tick " + i + " spans the bar");
            }

            // The end inset centers the end labels on the bar's end ticks
            const centerY = item => item.y + item.height / 2;
            const lastIndex = RootData.regionSceneManager.colorBy.legendTicks.length - 1;
            for (const index of [0, lastIndex]) {
                const tick = ticks.itemAt(index);
                const label = tickLabel(tick);
                verify(label.visible, "end tick " + index + " shows its label");
                verify(Math.abs(centerY(label) - centerY(tickMark(tick))) <= edgeTolerance,
                       "end label " + index + " centers on its mark: label " + centerY(label)
                       + ", mark " + centerY(tickMark(tick)));
                verify(label.y >= 0 && label.y + label.height <= tick.height,
                       "end label " + index + " stays inside the legend");
            }

            // Saved so the ticks can be eyeballed
            const path = OffscreenRenderTester.tempPngPath("colorby_legend_ticks");
            let saved = false;
            legend.grabToImage(result => { saved = result.saveToFile(path); });
            tryVerify(() => saved, 5000, "saved the legend image");
            console.log("color-by legend render: " + path);
        }

        function test_tickLabelsUseDefaultLabelSize() {
            let ticks = showDepthTicks();
            let legend = findLegend();
            let plainLabel = Qt.createQmlObject("import QtQuick.Controls as QC; QC.Label {}", legend);
            verify(plainLabel, "created a plain label");

            compare(tickLabel(ticks.itemAt(0)).font.pixelSize, plainLabel.font.pixelSize);
            let title = findChild(legend, "colorByTitle");
            verify(title, "found the legend title");
            compare(title.font.pixelSize, plainLabel.font.pixelSize);
            plainLabel.destroy();
        }

        function test_pivotLabelHidesNearTick() {
            let ticks = showDepthTicks();
            let colorBy = RootData.regionSceneManager.colorBy;
            const roundedCount = colorBy.legendTicks.length;
            verify(roundedCount >= 3, "at least three rounded ticks");
            const inner = colorBy.legendTicks[1];
            const next = colorBy.legendTicks[2];

            const pivotTick = () => {
                for (let i = 0; i < ticks.count; ++i) {
                    let tick = ticks.itemAt(i);
                    if (tick && Math.abs(tick.modelData - colorBy.pivotManual) < pivotTolerance) {
                        return tick;
                    }
                }
                return null;
            };

            colorBy.pivotManual = inner + 1;
            tryCompare(ticks, "count", roundedCount + 1);
            tryVerify(() => pivotTick() !== null, 5000, "the pivot has its own tick");
            tryVerify(() => !tickLabel(pivotTick()).visible, 5000,
                      "the pivot label hides next to a rounded tick");

            colorBy.pivotManual = (inner + next) / 2;
            tryCompare(ticks, "count", roundedCount + 1);
            tryVerify(() => pivotTick() !== null, 5000, "the pivot moved");
            tryVerify(() => tickLabel(pivotTick()).visible, 5000,
                      "the pivot label shows halfway between ticks");
        }

        function clickEditPill(legend, popup) {
            let pill = findEditPill(legend);
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

        function test_loopErrorHidesDepthRowsAndTicksInSigma() {
            let colorBy = RootData.regionSceneManager.colorBy;
            let popup = openPopupWithDepth();
            let content = popup.contentItem;
            let palette = findChild(content, "colorByRampCombo");
            let spin = findChild(content, "colorByPivotSpin");
            let slider = findChild(content, "colorByStrengthSlider");
            verify(palette && spin && slider, "found the palette, White at and scrap tint rows");
            tryVerify(() => palette.visible && spin.visible && slider.visible, 5000,
                      "depth shows the palette, White at and scrap tint rows");

            let sourceCombo = findChild(content, "colorBySourceCombo");
            verify(sourceCombo, "found the source combo");
            compare(sourceCombo.model[loopErrorIndex], "Loop error");
            sourceCombo.currentIndex = loopErrorIndex;
            sourceCombo.activated(loopErrorIndex);
            tryCompare(colorBy, "source", ColorBy.LoopError);
            tryVerify(() => !palette.visible && !spin.visible && !slider.visible, 5000,
                      "loop error hides the palette, White at and scrap tint rows");
            compare(sourceCombo.currentIndex, loopErrorIndex);
            popup.close();

            let legend = findLegend();
            let title = findChild(legend, "colorByTitle");
            verify(title, "found the legend title");
            tryCompare(title, "text", "Loop error");
            let bar = findChild(legend, "colorByBar");
            verify(bar, "found the colorbar");
            tryVerify(() => bar.visible, 5000, "the colorbar shows for loop error");

            let ticks = findChild(legend, "colorByTicks");
            verify(ticks, "found the tick repeater");
            tryCompare(ticks, "count", sigmaTickCount);
            const expected = ["0 \u03c3", "1 \u03c3", "2 \u03c3", "\u22653 \u03c3"];
            for (let i = 0; i < sigmaTickCount; ++i) {
                let label = tickLabel(ticks.itemAt(i));
                compare(label.text, expected[i]);
                verify(label.visible, "sigma tick " + i + " shows its label");
            }
            // 0 sigma sits at the bottom of the bar, the top tick at the top
            verify(ticks.itemAt(0).markY > ticks.itemAt(sigmaTickCount - 1).markY,
                   "0 sigma sits below the top tick");

            // Saved so the legend can be eyeballed
            const path = OffscreenRenderTester.tempPngPath("colorby_cp17");
            let saved = false;
            legend.grabToImage(result => { saved = result.saveToFile(path); });
            tryVerify(() => saved, 5000, "saved the legend image");
            console.log("color-by loop error legend render: " + path);
        }

        function bandLabel(tick) {
            let label = findChild(tick, "colorByBandLabel");
            verify(label, "found the band label");
            return label;
        }

        function test_loopErrorShowsBandWordsAndNoLoopSwatch() {
            let colorBy = RootData.regionSceneManager.colorBy;
            let popup = openPopupWithDepth();
            popup.close();
            let legend = findLegend();
            let noLoopRow = findChild(legend, "colorByNoLoopRow");
            verify(noLoopRow, "found the No loop row");
            let ticks = findChild(legend, "colorByTicks");
            verify(ticks, "found the tick repeater");
            tryVerify(() => ticks.count >= 3, 5000, "depth shows its ticks");
            verify(!noLoopRow.visible, "depth hides the No loop row");
            verify(!bandLabel(ticks.itemAt(0)).visible, "depth hides the band words");

            colorBy.source = ColorBy.LoopError;
            tryCompare(ticks, "count", sigmaTickCount);
            tryVerify(() => noLoopRow.visible, 5000, "loop error shows the No loop row");

            // Ticks run bottom (0 sigma) to top, and each band word centers on its mark
            const expected = ["Perfect", "Good", "Okay", "Bad"];
            const centerY = item => item.y + item.height / 2;
            for (let i = 0; i < sigmaTickCount; ++i) {
                const tick = ticks.itemAt(i);
                const band = bandLabel(tick);
                compare(band.text, expected[i]);
                verify(band.visible, "band word " + i + " shows");
                verify(Math.abs(centerY(band) - tick.markY) <= edgeTolerance,
                       "band word " + i + " centers on its mark: label " + centerY(band)
                       + ", mark " + tick.markY);
                const sigmaLabel = tickLabel(tick);
                verify(band.x >= sigmaLabel.x + sigmaLabel.width,
                       "band word " + i + " sits right of the sigma label");
                verify(band.x + band.width <= tick.width + edgeTolerance,
                       "band word " + i + " fits inside the legend");
            }
            for (let i = 1; i < sigmaTickCount; ++i) {
                verify(ticks.itemAt(i - 1).markY > ticks.itemAt(i).markY,
                       "band " + expected[i - 1] + " sits below " + expected[i]);
            }

            let swatch = findChild(noLoopRow, "colorByNoLoopSwatch");
            let noLoopLabel = findChild(noLoopRow, "colorByNoLoopLabel");
            verify(swatch && noLoopLabel, "found the No loop swatch and label");
            compare(noLoopLabel.text, "No loop");
            verify(Qt.colorEqual(swatch.color, colorBy.noDataColor), "the swatch is the no-data gray");

            // Saved so the legend can be eyeballed
            tryVerify(() => legend.height > legend.barHeight, 5000, "the legend has laid out the bar");
            const path = OffscreenRenderTester.tempPngPath("colorby_cp18");
            let saved = false;
            legend.grabToImage(result => { saved = result.saveToFile(path); });
            tryVerify(() => saved, 5000, "saved the legend image");
            console.log("color-by loop error legend render: " + path);

            colorBy.source = ColorBy.Depth;
            tryVerify(() => !noLoopRow.visible, 5000, "back on depth, the No loop row hides");
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

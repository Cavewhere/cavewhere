import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder
import "SyncTestHelper.js" as SyncTestHelper

MainWindowTest {
    id: rootId

    TestCase {
        id: testCaseId
        name: "ScrapSync"
        when: windowShown

        function tryVerifyWithDiagnostics(predicate, timeoutMs, label, onPending) {
            SyncTestHelper.tryVerifyWithDiagnostics(testCaseId, predicate, timeoutMs, label, onPending)
        }

        function loadFixtureAndOpenFirstTrip() {
            return SyncTestHelper.loadFixtureAndOpenFirstTrip(testCaseId, RootData, TestHelper)
        }

        function currentTrip() {
            let currentPageItem = RootData.pageView.currentPageItem
            verify(currentPageItem !== null)
            verify(currentPageItem.currentTrip !== null)
            return currentPageItem.currentTrip
        }

        function noteGallery() {
            let gallery = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery")
            verify(gallery !== null)
            return gallery
        }

        function findDescendantWhere(rootObject, predicate) {
            if (rootObject === null || rootObject === undefined) {
                return null
            }
            if (predicate(rootObject)) {
                return rootObject
            }

            let childCollections = []
            if (rootObject.children !== undefined && rootObject.children !== null) {
                childCollections.push(rootObject.children)
            }
            if (rootObject.contentItem !== undefined && rootObject.contentItem !== null) {
                childCollections.push([rootObject.contentItem])
            }

            for (let c = 0; c < childCollections.length; ++c) {
                let children = childCollections[c]
                for (let i = 0; i < children.length; ++i) {
                    let found = findDescendantWhere(children[i], predicate)
                    if (found !== null) {
                        return found
                    }
                }
            }

            return null
        }

        function noteGalleryView() {
            let galleryView = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->galleryView")
            verify(galleryView !== null)
            return galleryView
        }

        function carpetButton() {
            let button = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->carpetButtonId")
            verify(button !== null)
            return button
        }

        function addScrapButton() {
            let button = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->addScrapButton")
            verify(button !== null)
            return button
        }

        function addScrapStationButton() {
            let button = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->addScrapStation")
            verify(button !== null)
            return button
        }

        function addLeadButton() {
            let button = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->addLeads")
            verify(button !== null)
            return button
        }

        function noteArea() {
            let area = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea")
            verify(area !== null)
            return area
        }

        function imageItem() {
            let image = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->imageId")
            verify(image !== null)
            return image
        }

        function scrapView() {
            let view = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->scrapViewId")
            verify(view !== null)
            return view
        }

        function leadEditor() {
            let editor = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->leadEditor")
            verify(editor !== null)
            return editor
        }

        function leadEditorWidthText() {
            let input = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->leadEditor->widthText")
            verify(input !== null)
            return input
        }

        function leadEditorHeightText() {
            let input = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->leadEditor->heightText")
            verify(input !== null)
            return input
        }

        function leadEditorDescription() {
            let input = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->leadEditor->description")
            verify(input !== null)
            return input
        }

        function autoCalculateScrapCheckBox() {
            let checkBox = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->noteTransformEditor->autoCalculate->checkBox")
            verify(checkBox !== null)
            return checkBox
        }

        function noteTransformEditor() {
            let editor = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->noteTransformEditor")
            verify(editor !== null)
            return editor
        }

        function noteTransformTypeComboBox() {
            let comboBox = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->noteTransformEditor->typeComboBox")
            verify(comboBox !== null)
            return comboBox
        }

        function noteTransformUpInput() {
            let input = findDescendantWhere(noteTransformEditor(), (child) => {
                return child !== null
                       && child.northUpHelp !== undefined
                       && child.northUpInteractionActivated !== undefined
            })
            verify(input !== null)
            return input
        }

        function noteTransformUpLabel() {
            let label = findDescendantWhere(noteTransformUpInput(), (child) => {
                return child !== null
                       && child.helpArea !== undefined
                       && child.text !== undefined
            })
            verify(label !== null)
            return label
        }

        function noteTransformUpValueInput() {
            let input = findDescendantWhere(noteTransformUpInput(), (child) => {
                return child !== null
                       && child.readOnly !== undefined
                       && child.text !== undefined
            })
            verify(input !== null)
            return input
        }

        function noteTransformScaleInput() {
            let input = findDescendantWhere(noteTransformEditor(), (child) => {
                return child !== null
                       && child.scaleObject !== undefined
                       && child.onPaperLabel !== undefined
                       && child.inCaveLabel !== undefined
            })
            verify(input !== null)
            return input
        }

        function noteTransformScaleOnPaperInput() {
            let input = findDescendantWhere(noteTransformScaleInput(), (child) => {
                return child !== null && child.objectName === "onPaperLengthInput"
            })
            verify(input !== null)
            return input
        }

        function noteTransformScaleInCaveInput() {
            let input = findDescendantWhere(noteTransformScaleInput(), (child) => {
                return child !== null && child.objectName === "inCaveLengthInput"
            })
            verify(input !== null)
            return input
        }

        function noteTransformScaleOnPaperUnitInput() {
            let input = findDescendantWhere(noteTransformScaleOnPaperInput(), (child) => {
                return child !== null && child.objectName === "unitInput"
            })
            verify(input !== null)
            return input
        }

        function noteTransformScaleInCaveUnitInput() {
            let input = findDescendantWhere(noteTransformScaleInCaveInput(), (child) => {
                return child !== null && child.objectName === "unitInput"
            })
            verify(input !== null)
            return input
        }

        function noteTransformAzimuthRow() {
            let row = noteTransformDirectionComboBox().parent
            verify(row !== null)
            return row
        }

        function noteTransformDirectionComboBox() {
            let comboBox = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->noteTransformEditor->autoCalculate->directionComboBox")
            verify(comboBox !== null)
            return comboBox
        }

        function noteTransformAzimuthTextInput() {
            let input = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->noteTransformEditor->autoCalculate->azimuthTextInput")
            verify(input !== null)
            return input
        }

        function findFirstNoteIndexWithScraps() {
            let model = currentTrip().notes
            verify(model !== null)

            for (let row = 0; row < model.rowCount(); ++row) {
                selectNoteIndex(row, "select note during scrap scan", true)

                if (TestHelper.noteScrapCount(noteGallery().currentNote) > 0) {
                    return row
                }
            }

            return -1
        }

        function findFirstNoteIndexWithoutScraps() {
            let model = currentTrip().notes
            verify(model !== null)

            for (let row = 0; row < model.rowCount(); ++row) {
                selectNoteIndex(row, "select note during empty scrap scan", true)

                if (TestHelper.noteScrapCount(noteGallery().currentNote) === 0) {
                    return row
                }
            }

            return -1
        }

        function findFirstNoteAndScrapIndexWithLeads() {
            let model = currentTrip().notes
            verify(model !== null)

            for (let row = 0; row < model.rowCount(); ++row) {
                selectNoteIndex(row, "select note during lead scan", true)
                enterCarpetMode()

                let currentScrapView = scrapView()
                tryVerifyWithDiagnostics(() => {
                    return currentScrapView.note === noteGallery().currentNote
                           && currentScrapView.count > 0
                }, 5000, "bind scrap view during lead scan")

                for (let scrapIndex = 0; scrapIndex < currentScrapView.count; ++scrapIndex) {
                    selectScrapIndex(scrapIndex, "select scrap during lead scan")

                    if (currentScrapView.selectedScrapItem.scrap.numberOfLeads() > 0) {
                        return {
                            noteIndex: row,
                            scrapIndex: scrapIndex
                        }
                    }
                }
            }

            return null
        }

        function selectNoteIndex(noteIndex, label, forceRebind) {
            let gallery = noteGallery()
            let galleryView = noteGalleryView()

            tryVerifyWithDiagnostics(() => {
                return RootData.pageView.currentPageItem !== null
                       && RootData.pageView.currentPageItem.objectName === "tripPage"
                       && galleryView.count > noteIndex
            }, 5000, label + " wait for gallery row")

            let shouldForceRebind = forceRebind === true

            if (shouldForceRebind && galleryView.currentIndex === noteIndex) {
                galleryView.currentIndex = -1
                wait(50)
            }
            if (galleryView.currentIndex !== noteIndex) {
                galleryView.currentIndex = noteIndex
            }

            tryVerifyWithDiagnostics(() => {
                if (galleryView.currentIndex !== noteIndex) {
                    galleryView.currentIndex = noteIndex
                }

                if (shouldForceRebind
                        && gallery.currentNote === null
                        && galleryView.currentIndex === noteIndex
                        && gallery.state === "NO_NOTES") {
                    galleryView.currentIndex = -1
                    wait(50)
                    galleryView.currentIndex = noteIndex
                }

                return gallery.state !== "NO_NOTES"
                       && gallery.currentNote !== null
                       && galleryView.currentIndex === noteIndex
            }, 5000, label)
        }

        function enterCarpetMode() {
            let gallery = noteGallery()
            if (gallery.mode !== "CARPET") {
                mouseClick(carpetButton())
            }

            tryVerifyWithDiagnostics(() => {
                return gallery.mode === "CARPET"
            }, 5000, "enter carpet mode")
        }

        function waitForNoteCanvasReady(label) {
            let gallery = noteGallery()
            let area = noteArea()
            let image = imageItem()
            let galleryView = noteGalleryView()

            tryVerifyWithDiagnostics(() => {
                return RootData.pageView.currentPageItem !== null
                       && RootData.pageView.currentPageItem.objectName === "tripPage"
                       && gallery.state !== "NO_NOTES"
                       && gallery.currentNote !== null
                       && galleryView.currentIndex >= 0
                       && area.visible === true
                       && image.visible === true
                       && image.width > 0
                       && image.height > 0
            }, 5000, label)
        }

        function disableNoteLoadUi() {
            let toolbarLoadButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->toolbarLoadNotesButton")
            if (toolbarLoadButton !== null) {
                if (toolbarLoadButton.visible !== undefined) {
                    toolbarLoadButton.visible = false
                }
                if (toolbarLoadButton.enabled !== undefined) {
                    toolbarLoadButton.enabled = false
                }
            }

            let emptyStateLoadButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->emptyStateLoadNotesButton")
            if (emptyStateLoadButton !== null) {
                if (emptyStateLoadButton.visible !== undefined) {
                    emptyStateLoadButton.visible = false
                }
                if (emptyStateLoadButton.enabled !== undefined) {
                    emptyStateLoadButton.enabled = false
                }
            }
        }

        function prepareScrapOutlineUi(stage) {
            let scrapNoteIndex = findFirstNoteIndexWithScraps()
            verify(scrapNoteIndex >= 0)

            let gallery = noteGallery()
            let galleryView = noteGalleryView()
            galleryView.currentIndex = scrapNoteIndex

            tryVerifyWithDiagnostics(() => {
                return gallery.currentNote !== null
            }, 5000, "select note with scraps")

            let requiresScrapSelection = stage === "before-set"
                                         || stage === "after-set"
                                         || stage === "after-set-ui"

            if (!requiresScrapSelection) {
                return
            }

            if (gallery.mode !== "CARPET") {
                mouseClick(carpetButton())
            }

            tryVerifyWithDiagnostics(() => {
                return gallery.mode === "CARPET"
            }, 5000, "enter carpet mode")

            let currentScrapView = scrapView()
            tryVerifyWithDiagnostics(() => {
                return currentScrapView.note === gallery.currentNote
            }, 5000, "bind scrap view to selected note")
            tryVerifyWithDiagnostics(() => {
                return currentScrapView.count > 0
            }, 5000, "wait for scraps in note area")

            if (currentScrapView.selectScrapIndex !== 0) {
                currentScrapView.selectScrapIndex = 0
            }

            tryVerifyWithDiagnostics(() => {
                return currentScrapView.selectedScrapItem !== null
                       && currentScrapView.selectedScrapItem.scrap !== null
            }, 5000, "select first scrap")
        }

        function restoreTripPage(tripPageAddress) {
            RootData.pageSelectionModel.currentPageAddress = tripPageAddress
            tryVerifyWithDiagnostics(() => {
                return RootData.pageView.currentPageItem !== null
                       && RootData.pageView.currentPageItem.objectName === "tripPage"
            }, 10000, "restore trip page")
        }

        function selectedScrapItem() {
            let view = scrapView()
            let item = view.selectedScrapItem
            verify(item !== null)
            return item
        }

        function selectScrapIndex(scrapIndex, label) {
            let view = scrapView()

            if (view.selectScrapIndex !== scrapIndex) {
                view.selectScrapIndex = scrapIndex
            }

            tryVerifyWithDiagnostics(() => {
                return view.selectScrapIndex === scrapIndex
                       && view.selectedScrapItem !== null
                       && view.selectedScrapItem.scrap !== null
            }, 5000, label)
        }

        function snapshotSelectedScrapOutlineState() {
            let gallery = noteGallery()
            verify(gallery.currentNote !== null)
            let state = TestHelper.scrapOutlineState(gallery.currentNote, 0)
            verify(state !== null && state !== undefined)
            return state
        }

        function mapOutlineStateToRenderedState(state) {
            let note = noteGallery().currentNote
            verify(note !== null)

            let renderSize = note.renderSize()
            let renderedState = {
                pointCount: state.pointCount,
                points: []
            }

            for (let i = 0; i < state.points.length; ++i) {
                let point = state.points[i]
                renderedState.points.push({
                    index: point.index,
                    x: point.x * renderSize.width,
                    y: (1.0 - point.y) * renderSize.height
                })
            }

            return renderedState
        }

        function roundToDigits(value, digits) {
            let scale = Math.pow(10, digits)
            return Math.round(Number(value) * scale) / scale
        }

        function leadDimensionText(value) {
            let numericValue = Number(value)
            return !isFinite(numericValue) || numericValue < 0 ? "?" : String(numericValue)
        }

        function normalizedLeadDimensionValue(value) {
            let numericValue = Number(value)
            return !isFinite(numericValue) || numericValue < 0 ? -1 : numericValue
        }

        function projectedProfileDirectionText(direction) {
            switch (direction) {
            case ProjectedProfileScrapViewMatrix.LookingAt:
                return "looking at"
            case ProjectedProfileScrapViewMatrix.LeftToRight:
                return "left → right"
            case ProjectedProfileScrapViewMatrix.RightToLeft:
                return "left ← right"
            default:
                return ""
            }
        }

        function convertLengthValue(value, fromUnit, toUnit) {
            let factorsToMeters = {}
            factorsToMeters[Units.Inches] = 0.0254
            factorsToMeters[Units.Feet] = 0.3048
            factorsToMeters[Units.Yards] = 0.9144
            factorsToMeters[Units.Meters] = 1.0
            factorsToMeters[Units.Millimeters] = 0.001
            factorsToMeters[Units.Centimeters] = 0.01
            factorsToMeters[Units.Kilometers] = 1000.0
            factorsToMeters[Units.LengthUnitless] = 1.0
            factorsToMeters[Units.Miles] = 1609.34

            let fromFactor = factorsToMeters[fromUnit]
            let toFactor = factorsToMeters[toUnit]
            if (fromFactor === undefined || toFactor === undefined) {
                return value
            }
            return Number(value) * fromFactor / toFactor
        }

        function snapshotSelectedScrapTransformState() {
            let scrap = selectedScrapItem().scrap
            verify(scrap !== null)
            verify(scrap.noteTransformation !== null)

            let scaleObject = scrap.noteTransformation.scaleObject
            verify(scaleObject !== null)
            verify(scaleObject.scaleNumerator !== null)
            verify(scaleObject.scaleDenominator !== null)

            let state = {
                type: scrap.type,
                calculateNoteTransform: scrap.calculateNoteTransform,
                northUp: roundToDigits(scrap.noteTransformation.northUp, 2),
                scaleNumeratorValue: roundToDigits(scaleObject.scaleNumerator.value, 2),
                scaleNumeratorUnit: scaleObject.scaleNumerator.unit,
                scaleDenominatorValue: roundToDigits(scaleObject.scaleDenominator.value, 2),
                scaleDenominatorUnit: scaleObject.scaleDenominator.unit,
                scale: roundToDigits(scaleObject.scale, 6),
                direction: null,
                azimuth: null
            }

            if (scrap.type === Scrap.ProjectedProfile) {
                state.direction = scrap.viewMatrix.direction
                state.azimuth = roundToDigits(scrap.viewMatrix.azimuth, 1)
            }

            return state
        }

        function selectedScrapTransformUiState() {
            let scaleOnPaperInput = noteTransformScaleOnPaperInput()
            let scaleInCaveInput = noteTransformScaleInCaveInput()
            let scaleOnPaperUnitInput = noteTransformScaleOnPaperUnitInput()
            let scaleInCaveUnitInput = noteTransformScaleInCaveUnitInput()
            let azimuthRow = noteTransformAzimuthRow()
            let state = {
                type: noteTransformTypeComboBox().currentIndex,
                autoCalculate: autoCalculateScrapCheckBox().checked,
                upLabel: noteTransformUpLabel().text,
                upValue: Number(noteTransformUpValueInput().text),
                upReadOnly: noteTransformUpValueInput().readOnly,
                scaleOnPaperValue: roundToDigits(scaleOnPaperInput.unitValue.value, 2),
                scaleOnPaperUnit: scaleOnPaperInput.unitValue.unit,
                scaleOnPaperVisible: scaleOnPaperInput.valueVisible,
                scaleOnPaperReadOnly: scaleOnPaperInput.valueReadOnly,
                scaleInCaveValue: roundToDigits(scaleInCaveInput.unitValue.value, 2),
                scaleInCaveUnit: scaleInCaveInput.unitValue.unit,
                scaleInCaveVisible: scaleInCaveInput.valueVisible,
                scaleInCaveReadOnly: scaleInCaveInput.valueReadOnly,
                azimuthVisible: azimuthRow.visible,
                direction: null,
                directionText: null,
                azimuth: null,
                azimuthReadOnly: null
            }

            if (state.azimuthVisible) {
                state.direction = noteTransformDirectionComboBox().currentIndex
                state.directionText = noteTransformDirectionComboBox().currentText
                state.azimuth = Number(noteTransformAzimuthTextInput().text)
                state.azimuthReadOnly = noteTransformAzimuthTextInput().readOnly
            }

            return state
        }

        function expectedScrapTransformUiState(state) {
            let autoScale = state.calculateNoteTransform
            let typeIsPlan = state.type === Scrap.Plan
            let typeIsProjected = state.type === Scrap.ProjectedProfile
            return {
                type: state.type,
                autoCalculate: autoScale,
                upLabel: typeIsPlan ? "North" : "Up",
                upValue: state.northUp,
                upReadOnly: autoScale,
                scaleOnPaperValue: state.scaleNumeratorValue,
                scaleOnPaperUnit: state.scaleNumeratorUnit,
                scaleOnPaperVisible: !autoScale || state.scaleNumeratorUnit !== Units.LengthUnitless,
                scaleOnPaperReadOnly: autoScale,
                scaleInCaveValue: state.scaleDenominatorValue,
                scaleInCaveUnit: state.scaleDenominatorUnit,
                scaleInCaveVisible: !autoScale || state.scaleDenominatorUnit !== Units.LengthUnitless,
                scaleInCaveReadOnly: autoScale,
                azimuthVisible: typeIsProjected,
                direction: typeIsProjected ? state.direction : null,
                directionText: typeIsProjected ? projectedProfileDirectionText(state.direction) : null,
                azimuth: typeIsProjected ? state.azimuth : null,
                azimuthReadOnly: typeIsProjected ? autoScale : null
            }
        }

        function selectedScrapTransformTypeUiState() {
            return {
                type: noteTransformTypeComboBox().currentIndex,
                autoCalculate: autoCalculateScrapCheckBox().checked,
                upLabel: noteTransformUpLabel().text,
                upReadOnly: noteTransformUpValueInput().readOnly,
                azimuthVisible: noteTransformAzimuthRow().visible
            }
        }

        function expectedScrapTransformTypeUiState(state) {
            return {
                type: state.type,
                autoCalculate: state.calculateNoteTransform,
                upLabel: state.type === Scrap.Plan ? "North" : "Up",
                upReadOnly: state.calculateNoteTransform,
                azimuthVisible: state.type === Scrap.ProjectedProfile
            }
        }

        function applySelectedScrapTransformState(state) {
            let scrap = selectedScrapItem().scrap
            verify(scrap !== null)
            verify(scrap.noteTransformation !== null)
            verify(scrap.noteTransformation.scaleObject !== null)

            if (scrap.type !== state.type) {
                scrap.type = state.type
            }

            tryVerifyWithDiagnostics(() => {
                return scrap.type === state.type
            }, 5000, "wait for scrap type change")

            if (scrap.calculateNoteTransform !== state.calculateNoteTransform) {
                scrap.calculateNoteTransform = state.calculateNoteTransform
            }

            if (roundToDigits(scrap.noteTransformation.northUp, 2) !== state.northUp) {
                scrap.noteTransformation.northUp = state.northUp
            }

            let scaleObject = scrap.noteTransformation.scaleObject
            if (scaleObject.scaleNumerator.unit !== state.scaleNumeratorUnit) {
                scaleObject.scaleNumerator.unit = state.scaleNumeratorUnit
            }
            if (roundToDigits(scaleObject.scaleNumerator.value, 2) !== state.scaleNumeratorValue) {
                scaleObject.scaleNumerator.value = state.scaleNumeratorValue
            }
            if (scaleObject.scaleDenominator.unit !== state.scaleDenominatorUnit) {
                scaleObject.scaleDenominator.unit = state.scaleDenominatorUnit
            }
            if (roundToDigits(scaleObject.scaleDenominator.value, 2) !== state.scaleDenominatorValue) {
                scaleObject.scaleDenominator.value = state.scaleDenominatorValue
            }

            if (state.type === Scrap.ProjectedProfile) {
                verify(scrap.viewMatrix !== null)
                if (scrap.viewMatrix.direction !== state.direction) {
                    scrap.viewMatrix.direction = state.direction
                }
                if (roundToDigits(scrap.viewMatrix.azimuth, 1) !== state.azimuth) {
                    scrap.viewMatrix.azimuth = state.azimuth
                }
            }

            tryVerifyWithDiagnostics(() => {
                return SyncTestHelper.deepEqual(snapshotSelectedScrapTransformState(), state)
            }, 5000, "wait for applied scrap transform state")
        }

        function nextTransformStateWithType(state, type) {
            let nextState = {
                type: type,
                calculateNoteTransform: state.calculateNoteTransform,
                northUp: state.northUp,
                scaleNumeratorValue: state.scaleNumeratorValue,
                scaleNumeratorUnit: state.scaleNumeratorUnit,
                scaleDenominatorValue: state.scaleDenominatorValue,
                scaleDenominatorUnit: state.scaleDenominatorUnit,
                scale: state.scale,
                direction: null,
                azimuth: null
            }

            if (type === Scrap.ProjectedProfile) {
                nextState.direction = state.direction !== null
                                      ? state.direction
                                      : ProjectedProfileScrapViewMatrix.LookingAt
                nextState.azimuth = state.azimuth !== null ? state.azimuth : 0.0
            }

            return nextState
        }

        function nextPlanManualTransformState(state) {
            let nextState = nextTransformStateWithType(state, Scrap.Plan)
            nextState.calculateNoteTransform = false
            nextState.northUp = state.northUp === 27.5 ? 42.25 : 27.5
            nextState.scaleNumeratorUnit = Units.Centimeters
            nextState.scaleNumeratorValue = 1.0
            nextState.scaleDenominatorUnit = Units.Meters
            nextState.scaleDenominatorValue = state.scaleDenominatorValue === 2.5 ? 4.0 : 2.5
            nextState.scale = roundToDigits(convertLengthValue(nextState.scaleNumeratorValue,
                                                               nextState.scaleNumeratorUnit,
                                                               Units.Meters)
                                            / convertLengthValue(nextState.scaleDenominatorValue,
                                                                 nextState.scaleDenominatorUnit,
                                                                 Units.Meters), 6)
            return nextState
        }

        function nextProjectedManualTransformState(state) {
            let nextState = nextTransformStateWithType(state, Scrap.ProjectedProfile)
            nextState.calculateNoteTransform = false
            nextState.northUp = state.northUp === 91.5 ? 123.75 : 91.5
            nextState.scaleNumeratorUnit = Units.Centimeters
            nextState.scaleNumeratorValue = 1.0
            nextState.scaleDenominatorUnit = Units.Meters
            nextState.scaleDenominatorValue = state.scaleDenominatorValue === 3.0 ? 5.5 : 3.0
            nextState.scale = roundToDigits(convertLengthValue(nextState.scaleNumeratorValue,
                                                               nextState.scaleNumeratorUnit,
                                                               Units.Meters)
                                            / convertLengthValue(nextState.scaleDenominatorValue,
                                                                 nextState.scaleDenominatorUnit,
                                                                 Units.Meters), 6)
            nextState.direction = state.direction === ProjectedProfileScrapViewMatrix.LeftToRight
                                  ? ProjectedProfileScrapViewMatrix.RightToLeft
                                  : ProjectedProfileScrapViewMatrix.LeftToRight
            nextState.azimuth = state.azimuth === 37.5 ? 148.0 : 37.5
            return nextState
        }

        function snapshotSelectedScrapStationState() {
            let scrap = selectedScrapItem().scrap
            verify(scrap !== null)

            let state = {
                stationCount: scrap.numberOfStations(),
                stations: []
            }

            for (let i = 0; i < scrap.numberOfStations(); ++i) {
                let position = scrap.stationData(Scrap.StationPosition, i)
                state.stations.push({
                    index: i,
                    name: String(scrap.stationData(Scrap.StationName, i)),
                    x: Number(position.x.toFixed(4)),
                    y: Number(position.y.toFixed(4))
                })
            }

            return state
        }

        function selectedLeadItem(leadIndex) {
            let scrapItem = selectedScrapItem()
            let item = null
            tryVerifyWithDiagnostics(() => {
                item = findDescendantWhere(noteArea(), (child) => {
                    return child !== null
                           && child.objectName === "noteLead" + leadIndex
                           && child.scrapItem !== undefined
                           && child.scrapItem === scrapItem
                })
                return item !== null
            }, 5000, "find rendered lead item")
            verify(item !== null)
            return item
        }

        function cloneLeadState(state) {
            return {
                x: state.x,
                y: state.y,
                width: state.width,
                height: state.height,
                description: state.description
            }
        }

        function snapshotSelectedScrapLeadsState() {
            let scrap = selectedScrapItem().scrap
            verify(scrap !== null)

            let leads = []
            for (let i = 0; i < scrap.numberOfLeads(); ++i) {
                let size = scrap.leadData(Scrap.LeadSize, i)
                let position = scrap.leadData(Scrap.LeadPositionOnNote, i)
                leads.push({
                    x: Number(position.x.toFixed(4)),
                    y: Number(position.y.toFixed(4)),
                    width: normalizedLeadDimensionValue(size.width),
                    height: normalizedLeadDimensionValue(size.height),
                    description: String(scrap.leadData(Scrap.LeadDesciption, i))
                })
            }

            return {
                leadCount: leads.length,
                leads: leads
            }
        }

        function selectedScrapLeadUiState() {
            let note = noteGallery().currentNote
            verify(note !== null)
            let leadView = selectedScrapItem().leadView
            let hasLead = leadView.count > 0
            let state = {
                leadCount: leadView.count,
                selectedLeadIndex: leadView.selectedItemIndex,
                editorVisible: leadEditor().visible,
                leadItemX: null,
                leadItemY: null,
                width: null,
                height: null,
                description: null
            }

            if (hasLead && leadView.selectedItemIndex >= 0) {
                let leadItem = selectedLeadItem(leadView.selectedItemIndex)
                state.leadItemX = Math.floor(leadItem.x)
                state.leadItemY = Math.floor(leadItem.y)
                state.width = String(leadEditorWidthText().text)
                state.height = String(leadEditorHeightText().text)
                state.description = String(leadEditorDescription().text)
            }

            return state
        }

        function expectedScrapLeadUiStateForSelection(state, selectedLeadIndex) {
            let note = noteGallery().currentNote
            verify(note !== null)
            let renderSize = note.renderSize()
            let hasLead = state.leadCount > 0
            let effectiveSelectedIndex = hasLead ? selectedLeadIndex : -1
            let selectedLead = hasLead ? state.leads[effectiveSelectedIndex] : null
            return {
                leadCount: state.leadCount,
                selectedLeadIndex: effectiveSelectedIndex,
                editorVisible: hasLead,
                leadItemX: hasLead ? Math.floor(selectedLead.x * renderSize.width) : null,
                leadItemY: hasLead ? Math.floor((1.0 - selectedLead.y) * renderSize.height) : null,
                width: hasLead ? leadDimensionText(selectedLead.width) : null,
                height: hasLead ? leadDimensionText(selectedLead.height) : null,
                description: hasLead ? selectedLead.description : null
            }
        }

        function expectedScrapLeadUiState(state) {
            return expectedScrapLeadUiStateForSelection(state, state.leadCount > 0 ? 0 : -1)
        }

        function applySelectedScrapLeadsState(state) {
            let scrap = selectedScrapItem().scrap
            verify(scrap !== null)

            while (scrap.numberOfLeads() > state.leadCount) {
                scrap.removeLead(scrap.numberOfLeads() - 1)
            }

            while (scrap.numberOfLeads() < state.leadCount) {
                let leadIndex = scrap.numberOfLeads()
                let added = TestHelper.addScrapLead(noteGallery().currentNote,
                                                    scrapView().selectScrapIndex,
                                                    Qt.point(state.leads[leadIndex].x,
                                                             state.leads[leadIndex].y),
                                                    Qt.size(state.leads[leadIndex].width,
                                                            state.leads[leadIndex].height),
                                                    state.leads[leadIndex].description)
                verify(added)
            }

            for (let i = 0; i < state.leadCount; ++i) {
                let leadState = state.leads[i]
                let currentPosition = scrap.leadData(Scrap.LeadPositionOnNote, i)
                if (Number(currentPosition.x.toFixed(4)) !== leadState.x
                        || Number(currentPosition.y.toFixed(4)) !== leadState.y) {
                    scrap.setLeadData(Scrap.LeadPositionOnNote, i, Qt.point(leadState.x, leadState.y))
                }

                let currentSize = scrap.leadData(Scrap.LeadSize, i)
                if (Number(currentSize.width.toFixed(2)) !== leadState.width
                        || Number(currentSize.height.toFixed(2)) !== leadState.height) {
                    scrap.setLeadData(Scrap.LeadSize, i, Qt.size(leadState.width, leadState.height))
                }

                let currentDescription = String(scrap.leadData(Scrap.LeadDesciption, i))
                if (currentDescription !== leadState.description) {
                    scrap.setLeadData(Scrap.LeadDesciption, i, leadState.description)
                }
            }

            tryVerifyWithDiagnostics(() => {
                return SyncTestHelper.deepEqual(snapshotSelectedScrapLeadsState(), state)
            }, 5000, "wait for applied scrap leads state")

            TestHelper.waitForProjectSaveToFinish(RootData.project)
        }

        function nextLeadState(state) {
            verify(state.leadCount > 0)
            let nextState = {
                leadCount: state.leadCount,
                leads: []
            }

            for (let i = 0; i < state.leads.length; ++i) {
                nextState.leads.push(cloneLeadState(state.leads[i]))
            }

            nextState.leads[0] = {
                x: state.leads[0].x === 0.63 ? 0.58 : 0.63,
                y: state.leads[0].y === 0.44 ? 0.49 : 0.44,
                width: state.leads[0].width === 7 ? 5 : 7,
                height: state.leads[0].height === 6 ? 4 : 6,
                description: state.leads[0].description === "Sync lead updated"
                           ? "Sync lead updated again"
                           : "Sync lead updated"
            }

            return nextState
        }

        function nextLeadAddedState(state) {
            return {
                leadCount: state.leadCount + 1,
                leads: state.leads.concat([{
                    x: 0.63,
                    y: 0.44,
                    width: -1,
                    height: -1,
                    description: "Sync lead added"
                }])
            }
        }

        function nextLeadRemovedState(state) {
            verify(state.leadCount > 0)

            let nextLeads = []
            for (let i = 0; i < state.leads.length - 1; ++i) {
                nextLeads.push(cloneLeadState(state.leads[i]))
            }

            return {
                leadCount: state.leadCount - 1,
                leads: nextLeads
            }
        }

        function snapshotSelectedScrapRenderedOutlineState() {
            let state = selectedScrapItem().renderedOutlineState()
            verify(state !== null && state !== undefined)
            return state
        }

        function cloneOutlineState(state) {
            let clonedState = {
                pointCount: state.pointCount,
                points: []
            }

            for (let i = 0; i < state.points.length; ++i) {
                let point = state.points[i]
                clonedState.points.push({
                    index: point.index,
                    x: point.x,
                    y: point.y
                })
            }

            return clonedState
        }

        function isClosedOutlineState(state) {
            if (state.points.length < 2) {
                return false
            }

            let firstPoint = state.points[0]
            let lastPoint = state.points[state.points.length - 1]
            return firstPoint.x === lastPoint.x && firstPoint.y === lastPoint.y
        }

        function applySelectedScrapOutlineState(state) {
            let scrapItem = selectedScrapItem()
            let scrap = scrapItem.scrap
            verify(scrap !== null)

            while (scrap.numberOfPoints() < state.points.length) {
                let insertIndex = isClosedOutlineState(state)
                                ? scrap.numberOfPoints() - 1
                                : scrap.numberOfPoints()
                let insertedPoint = state.points[insertIndex]
                scrap.insertPoint(insertIndex, Qt.point(insertedPoint.x, insertedPoint.y))
            }

            while (scrap.numberOfPoints() > state.points.length) {
                let removeIndex = scrap.isClosed()
                                ? scrap.numberOfPoints() - 2
                                : scrap.numberOfPoints() - 1
                scrap.removePoint(removeIndex)
            }

            compare(scrap.numberOfPoints(), state.points.length)

            for (let i = 0; i < state.points.length; ++i) {
                let point = state.points[i]
                scrap.setPoint(i, Qt.point(point.x, point.y))
            }
        }

        function nextOutlineState(state) {
            verify(state.points.length > 1)

            let nextState = cloneOutlineState(state)

            nextState.points[1].x += 0.01
            nextState.points[1].y += 0.01
            return nextState
        }

        function addThreeOutlinePointsState(state) {
            verify(state.points.length > 2)

            let nextState = cloneOutlineState(state)
            let insertionBaseIndex = isClosedOutlineState(nextState)
                                   ? nextState.points.length - 1
                                   : nextState.points.length

            let anchorA = nextState.points[0]
            let anchorB = nextState.points[1]
            let deltaX = anchorB.x - anchorA.x
            let deltaY = anchorB.y - anchorA.y
            let offsetScale = 0.005

            let insertedPoints = [
                { x: anchorA.x + deltaX * 0.25 - deltaY * offsetScale, y: anchorA.y + deltaY * 0.25 + deltaX * offsetScale },
                { x: anchorA.x + deltaX * 0.50 - deltaY * offsetScale, y: anchorA.y + deltaY * 0.50 + deltaX * offsetScale },
                { x: anchorA.x + deltaX * 0.75 - deltaY * offsetScale, y: anchorA.y + deltaY * 0.75 + deltaX * offsetScale }
            ]

            for (let i = 0; i < insertedPoints.length; ++i) {
                nextState.points.splice(insertionBaseIndex + i, 0, {
                    index: insertionBaseIndex + i,
                    x: insertedPoints[i].x,
                    y: insertedPoints[i].y
                })
            }

            for (let pointIndex = 0; pointIndex < nextState.points.length; ++pointIndex) {
                nextState.points[pointIndex].index = pointIndex
            }
            nextState.pointCount = nextState.points.length
            return nextState
        }

        function removeLastThreeOutlinePointsState(state) {
            verify(state.points.length >= 6)

            let nextState = cloneOutlineState(state)
            let removeStartIndex = isClosedOutlineState(nextState)
                                 ? nextState.points.length - 4
                                 : nextState.points.length - 3
            nextState.points.splice(removeStartIndex, 3)

            for (let pointIndex = 0; pointIndex < nextState.points.length; ++pointIndex) {
                nextState.points[pointIndex].index = pointIndex
            }
            nextState.pointCount = nextState.points.length
            return nextState
        }

        function nextStationStateWithAddedStation(state) {
            let nextState = {
                stationCount: state.stationCount + 1,
                stations: []
            }

            for (let i = 0; i < state.stations.length; ++i) {
                let station = state.stations[i]
                nextState.stations.push({
                    index: station.index,
                    name: station.name,
                    x: station.x,
                    y: station.y
                })
            }

            nextState.stations.push({
                index: nextState.stations.length,
                name: "sync-added-station",
                x: 0.52,
                y: 0.58
            })

            return nextState
        }

        function nextStationStateWithRemovedStation(state) {
            verify(state.stationCount > 0)

            let nextState = {
                stationCount: state.stationCount - 1,
                stations: []
            }

            for (let i = 0; i < state.stations.length - 1; ++i) {
                let station = state.stations[i]
                nextState.stations.push({
                    index: i,
                    name: station.name,
                    x: station.x,
                    y: station.y
                })
            }

            return nextState
        }

        function nextStationStateWithRenamedStation(state) {
            verify(state.stationCount > 0)

            let nextState = {
                stationCount: state.stationCount,
                stations: []
            }

            for (let i = 0; i < state.stations.length; ++i) {
                let station = state.stations[i]
                nextState.stations.push({
                    index: i,
                    name: station.name,
                    x: station.x,
                    y: station.y
                })
            }

            let targetIndex = nextState.stations.length - 1
            let baselineName = nextState.stations[targetIndex].name
            nextState.stations[targetIndex].name =
                    baselineName === "syncrenamedstation"
                    ? "syncrenamedstation2"
                    : "syncrenamedstation"

            return nextState
        }

        function renderedSelectedScrapStationNames() {
            let stationView = selectedScrapItem().stationView
            let names = []
            let objectNames = stationView.itemObjectNames()
            for (let i = 0; i < objectNames.length; ++i) {
                let objectName = String(objectNames[i])
                if (objectName.startsWith("station")) {
                    names.push(objectName.slice("station".length))
                }
            }
            names.sort()
            return names
        }

        function applySelectedScrapStationState(state) {
            let scrap = selectedScrapItem().scrap
            verify(scrap !== null)
            let currentCount = scrap.numberOfStations()

            if (state.stations.length === currentCount + 1) {
                let note = noteGallery().currentNote
                verify(note !== null)
                let addedStation = state.stations[state.stations.length - 1]

                verify(TestHelper.addScrapStation(note, 0, addedStation.name, Qt.point(addedStation.x, addedStation.y)))

                tryVerifyWithDiagnostics(() => {
                    return scrap.numberOfStations() === state.stations.length
                }, 5000, "wait for added scrap station")
            } else if (state.stations.length === currentCount - 1) {
                scrap.removeStation(currentCount - 1)

                tryVerifyWithDiagnostics(() => {
                    return scrap.numberOfStations() === state.stations.length
                }, 5000, "wait for removed scrap station")
            } else {
                compare(state.stations.length, currentCount)
            }

            compare(scrap.numberOfStations(), state.stations.length)

            for (let i = 0; i < state.stations.length; ++i) {
                let expectedStation = state.stations[i]
                let currentName = String(scrap.stationData(Scrap.StationName, i))
                let currentPosition = scrap.stationData(Scrap.StationPosition, i)
                if (currentName !== expectedStation.name) {
                    scrap.setStationData(Scrap.StationName, i, expectedStation.name)
                }
                if (Number(currentPosition.x.toFixed(4)) !== expectedStation.x
                        || Number(currentPosition.y.toFixed(4)) !== expectedStation.y) {
                    scrap.setStationData(Scrap.StationPosition, i, Qt.point(expectedStation.x, expectedStation.y))
                }
            }

            tryVerifyWithDiagnostics(() => {
                let currentState = snapshotSelectedScrapStationState()
                return SyncTestHelper.deepEqual(currentState, state)
            }, 5000, "wait for applied scrap station state")
        }

        function createNewOpenScrapWithThreePoints(existingScrapCount) {
            disableNoteLoadUi()
            waitForNoteCanvasReady("wait for note canvas before creating scrap")

            let currentScrapView = scrapView()
            compare(currentScrapView.count, existingScrapCount)
            if (currentScrapView.selectScrapIndex !== -1) {
                currentScrapView.selectScrapIndex = -1
            }

            mouseClick(addScrapButton())
            noteGallery().state = "ADD-SCRAP"
            wait(100)

            let image = imageItem()
            let points = [
                { x: image.width * 0.30, y: image.height * 0.35 },
                { x: image.width * 0.55, y: image.height * 0.36 },
                { x: image.width * 0.48, y: image.height * 0.62 }
            ]

            mouseMove(image, points[0].x, points[0].y)
            mouseClick(image, points[0].x, points[0].y)
            wait(50)
            tryVerifyWithDiagnostics(() => {
                return currentScrapView.count === existingScrapCount + 1
            }, 5000, "wait for new scrap row")
            mouseClick(autoCalculateScrapCheckBox())
            wait(50)

            for (let i = 1; i < points.length; ++i) {
                mouseMove(image, points[i].x, points[i].y)
                mouseClick(image, points[i].x, points[i].y)
                wait(50)
            }

            tryVerifyWithDiagnostics(() => {
                return currentScrapView.selectedScrapItem !== null
                       && currentScrapView.selectedScrapItem.scrap !== null
                       && currentScrapView.selectedScrapItem.scrap.numberOfPoints() === 3
            }, 5000, "wait for three-point scrap")
        }

        function prepareScrapTransformUi(stage) {
            prepareScrapOutlineUi(stage)

            let needsTransformEditor = stage === "baseline-ui"
                                     || stage === "before-set"
                                     || stage === "after-set"
                                     || stage === "after-set-ui"
                                     || stage === "after-checkout"
                                     || stage === "after-checkout-ui"
                                     || stage === "after-resync"
                                     || stage === "after-resync-ui"
            if (!needsTransformEditor) {
                return
            }

            enterCarpetMode()

            let currentScrapView = scrapView()
            tryVerifyWithDiagnostics(() => {
                return currentScrapView.note === noteGallery().currentNote
                       && currentScrapView.count > 0
            }, 5000, "bind scrap view for transform editor")

            if (currentScrapView.selectScrapIndex !== 0) {
                currentScrapView.selectScrapIndex = 0
            }

            let editor = noteTransformEditor()
            tryVerifyWithDiagnostics(() => {
                return editor.visible === true
                       && editor.scrap !== null
                       && editor.noteTransform !== null
            }, 5000, "wait for note transform editor")
        }

        function verifyLiveScrapTransformUi(label) {
            let gallery = noteGallery()
            let currentScrapView = scrapView()
            let editor = noteTransformEditor()

            tryVerifyWithDiagnostics(() => {
                return RootData.pageView.currentPageItem !== null
                       && RootData.pageView.currentPageItem.objectName === "tripPage"
                       && gallery.currentNote !== null
                       && gallery.mode === "CARPET"
                       && currentScrapView.note === gallery.currentNote
                       && currentScrapView.selectedScrapItem !== null
                       && currentScrapView.selectedScrapItem.scrap !== null
                       && editor.visible === true
                       && editor.scrap !== null
                       && editor.noteTransform !== null
                       && editor.scrap === currentScrapView.selectedScrapItem.scrap
            }, 5000, label)
        }

        function prepareScrapTransformUiWithoutRefresh(stage) {
            let needsLiveUiOnly = stage === "after-checkout"
                               || stage === "after-checkout-ui"
                               || stage === "after-resync"
                               || stage === "after-resync-ui"

            if (!needsLiveUiOnly) {
                prepareScrapTransformUi(stage)
                return
            }

            disableNoteLoadUi()
            waitForNoteCanvasReady("wait for note canvas for live transform ui")
            verifyLiveScrapTransformUi("verify live scrap transform ui")
        }

        function test_existingScrapOutlineSyncAndCheckout() {
            let context = loadFixtureAndOpenFirstTrip()

            SyncTestHelper.runProjectSyncRoundTrip(testCaseId, RootData, TestHelper, {
                tripPageAddress: context.tripPageAddress,
                prepare: prepareScrapOutlineUi,
                restorePage: () => restoreTripPage(context.tripPageAddress),
                getter: snapshotSelectedScrapOutlineState,
                uiExpectedFromValue: mapOutlineStateToRenderedState,
                uiGetter: () => {
                    return snapshotSelectedScrapRenderedOutlineState()
                },
                verifyBaselineUi: false,
                verifyEditedUi: false,
                verifyCheckoutUi: true,
                verifyResyncUi: true,
                setter: applySelectedScrapOutlineState,
                nextValue: nextOutlineState
            })
        }

        function test_scrapOutlineAddThenRemovePointsSyncAndCheckout() {
            let context = loadFixtureAndOpenFirstTrip()

            SyncTestHelper.runProjectSyncRoundTrip(testCaseId, RootData, TestHelper, {
                tripPageAddress: context.tripPageAddress,
                prepare: prepareScrapOutlineUi,
                restorePage: () => restoreTripPage(context.tripPageAddress),
                getter: snapshotSelectedScrapOutlineState,
                uiExpectedFromValue: mapOutlineStateToRenderedState,
                uiGetter: () => {
                    return snapshotSelectedScrapRenderedOutlineState()
                },
                verifyBaselineUi: false,
                verifyEditedUi: false,
                verifyCheckoutUi: true,
                verifyResyncUi: true,
                setter: applySelectedScrapOutlineState,
                nextValue: addThreeOutlinePointsState
            })

            SyncTestHelper.runProjectSyncRoundTrip(testCaseId, RootData, TestHelper, {
                tripPageAddress: context.tripPageAddress,
                prepare: prepareScrapOutlineUi,
                restorePage: () => restoreTripPage(context.tripPageAddress),
                getter: snapshotSelectedScrapOutlineState,
                uiExpectedFromValue: mapOutlineStateToRenderedState,
                uiGetter: () => {
                    return snapshotSelectedScrapRenderedOutlineState()
                },
                verifyBaselineUi: false,
                verifyEditedUi: false,
                verifyCheckoutUi: true,
                verifyResyncUi: true,
                setter: applySelectedScrapOutlineState,
                nextValue: removeLastThreeOutlinePointsState
            })
        }

        function test_newScrapCreateThenRemoveSyncAndCheckout() {
            let context = loadFixtureAndOpenFirstTrip()
            let newScrapNoteIndex = findFirstNoteIndexWithScraps()
            verify(newScrapNoteIndex >= 0)
            selectNoteIndex(newScrapNoteIndex, "select note for new scrap baseline")
            let baselineExistingScrapCount = TestHelper.noteScrapCount(noteGallery().currentNote)
            verify(baselineExistingScrapCount > 0)

            let prepareNewScrapUi = function(stage) {
                disableNoteLoadUi()
                selectNoteIndex(newScrapNoteIndex, "select note for new scrap test")
                waitForNoteCanvasReady("wait for note canvas in new scrap test")

                let requiresCarpetMode = stage === "before-set"
                                       || stage === "after-set"
                                       || stage === "after-set-ui"
                if (!requiresCarpetMode) {
                    return
                }

                enterCarpetMode()

                let currentScrapView = scrapView()
                tryVerifyWithDiagnostics(() => {
                    return currentScrapView.note === noteGallery().currentNote
                }, 5000, "bind scrap view to empty note")

                if (currentScrapView.count > baselineExistingScrapCount) {
                    let createdScrapIndex = currentScrapView.count - 1
                    if (currentScrapView.selectScrapIndex !== createdScrapIndex) {
                        currentScrapView.selectScrapIndex = createdScrapIndex
                    }
                } else if (currentScrapView.count > 0 && currentScrapView.selectScrapIndex !== 0) {
                    currentScrapView.selectScrapIndex = 0
                }

                if (currentScrapView.count > baselineExistingScrapCount) {
                    tryVerifyWithDiagnostics(() => {
                        return currentScrapView.selectedScrapItem !== null
                               && currentScrapView.selectedScrapItem.scrap !== null
                    }, 5000, "select new scrap item")
                }
            }

            let snapshotNewScrapState = function() {
                disableNoteLoadUi()
                selectNoteIndex(newScrapNoteIndex, "select note for new scrap snapshot")
                waitForNoteCanvasReady("wait for note canvas before new scrap snapshot")
                let note = noteGallery().currentNote
                verify(note !== null)

                let scrapCount = TestHelper.noteScrapCount(note)
                let state = {
                    scrapCount: scrapCount,
                    outlineState: null
                }

                if (scrapCount > baselineExistingScrapCount) {
                    state.outlineState = TestHelper.scrapOutlineState(note, scrapCount - 1)
                }

                return state
            }

            let applyNewScrapState = function(state) {
                disableNoteLoadUi()
                waitForNoteCanvasReady("wait for note canvas before applying new scrap state")
                enterCarpetMode()

                let currentScrapView = scrapView()
                tryVerifyWithDiagnostics(() => {
                    return currentScrapView.note === noteGallery().currentNote
                }, 5000, "bind scrap view while applying new scrap state")

                if (state.scrapCount === baselineExistingScrapCount) {
                    if (currentScrapView.count === baselineExistingScrapCount) {
                        return
                    }

                    let createdScrapIndex = currentScrapView.count - 1
                    if (currentScrapView.selectScrapIndex !== createdScrapIndex) {
                        currentScrapView.selectScrapIndex = createdScrapIndex
                    }

                    tryVerifyWithDiagnostics(() => {
                        return currentScrapView.selectedScrapItem !== null
                               && currentScrapView.selectedScrapItem.scrap !== null
                    }, 5000, "select scrap before deleting")

                    let scrap = currentScrapView.selectedScrapItem.scrap
                    while (scrap !== null && currentScrapView.count > 0 && scrap.numberOfPoints() > 0) {
                        scrap.removePoint(scrap.numberOfPoints() - 1)
                    }

                    tryVerifyWithDiagnostics(() => {
                        return currentScrapView.count === baselineExistingScrapCount
                    }, 5000, "wait for deleted scrap")
                    return
                }

                if (currentScrapView.count === baselineExistingScrapCount) {
                    createNewOpenScrapWithThreePoints(baselineExistingScrapCount)
                }

                let createdScrapIndex = currentScrapView.count - 1
                if (currentScrapView.selectScrapIndex !== createdScrapIndex) {
                    currentScrapView.selectScrapIndex = createdScrapIndex
                }

                tryVerifyWithDiagnostics(() => {
                    return currentScrapView.selectedScrapItem !== null
                           && currentScrapView.selectedScrapItem.scrap !== null
                }, 5000, "select created scrap")

                applySelectedScrapOutlineState(state.outlineState)
            }

            let createThreePointScrapState = function() {
                return {
                    scrapCount: baselineExistingScrapCount + 1,
                    outlineState: {
                        pointCount: 3,
                        points: [
                            { index: 0, x: 0.30, y: 0.35 },
                            { index: 1, x: 0.55, y: 0.36 },
                            { index: 2, x: 0.48, y: 0.62 }
                        ]
                    }
                }
            }

            let removeCreatedScrapState = function() {
                return {
                    scrapCount: baselineExistingScrapCount,
                    outlineState: null
                }
            }

            SyncTestHelper.runProjectSyncRoundTrip(testCaseId, RootData, TestHelper, {
                tripPageAddress: context.tripPageAddress,
                prepare: prepareNewScrapUi,
                restorePage: () => restoreTripPage(context.tripPageAddress),
                getter: snapshotNewScrapState,
                verifyBaselineUi: false,
                verifyEditedUi: false,
                verifyCheckoutUi: false,
                verifyResyncUi: false,
                setter: applyNewScrapState,
                nextValue: createThreePointScrapState
            })

            SyncTestHelper.runProjectSyncRoundTrip(testCaseId, RootData, TestHelper, {
                tripPageAddress: context.tripPageAddress,
                prepare: prepareNewScrapUi,
                restorePage: () => restoreTripPage(context.tripPageAddress),
                getter: snapshotNewScrapState,
                verifyBaselineUi: false,
                verifyEditedUi: false,
                verifyCheckoutUi: false,
                verifyResyncUi: false,
                setter: applyNewScrapState,
                nextValue: removeCreatedScrapState
            })
        }

        function test_existingScrapSwitchPlanAndRunningProfileSyncAndCheckout() {
            let context = loadFixtureAndOpenFirstTrip()

            SyncTestHelper.runProjectSyncRoundTrip(testCaseId, RootData, TestHelper, {
                tripPageAddress: context.tripPageAddress,
                prepare: prepareScrapTransformUi,
                restorePage: () => restoreTripPage(context.tripPageAddress),
                getter: snapshotSelectedScrapTransformState,
                uiExpectedFromValue: expectedScrapTransformTypeUiState,
                uiGetter: selectedScrapTransformTypeUiState,
                verifyEditedUi: false,
                verifyBaselineAfterCheckoutTimeoutMs: 10000,
                verifyResyncedValueTimeoutMs: 10000,
                setter: applySelectedScrapTransformState,
                nextValue: (state) => {
                    return nextTransformStateWithType(state, Scrap.Plan)
                }
            })

            SyncTestHelper.runProjectSyncRoundTrip(testCaseId, RootData, TestHelper, {
                tripPageAddress: context.tripPageAddress,
                prepare: prepareScrapTransformUi,
                restorePage: () => restoreTripPage(context.tripPageAddress),
                getter: snapshotSelectedScrapTransformState,
                uiExpectedFromValue: expectedScrapTransformTypeUiState,
                uiGetter: selectedScrapTransformTypeUiState,
                verifyEditedUi: false,
                verifyBaselineAfterCheckoutTimeoutMs: 10000,
                verifyResyncedValueTimeoutMs: 10000,
                setter: applySelectedScrapTransformState,
                nextValue: (state) => {
                    return nextTransformStateWithType(state, Scrap.RunningProfile)
                }
            })
        }

        function test_existingScrapSwitchPlanAndProjectedProfileSyncAndCheckout() {
            let context = loadFixtureAndOpenFirstTrip()

            SyncTestHelper.runProjectSyncRoundTrip(testCaseId, RootData, TestHelper, {
                tripPageAddress: context.tripPageAddress,
                prepare: prepareScrapTransformUiWithoutRefresh,
                restorePage: () => restoreTripPage(context.tripPageAddress),
                getter: snapshotSelectedScrapTransformState,
                uiExpectedFromValue: expectedScrapTransformTypeUiState,
                uiGetter: selectedScrapTransformTypeUiState,
                verifyEditedUi: false,
                verifyBaselineAfterCheckoutTimeoutMs: 10000,
                verifyResyncedValueTimeoutMs: 10000,
                setter: applySelectedScrapTransformState,
                nextValue: (state) => {
                    return nextTransformStateWithType(state, Scrap.Plan)
                }
            })

            SyncTestHelper.runProjectSyncRoundTrip(testCaseId, RootData, TestHelper, {
                tripPageAddress: context.tripPageAddress,
                prepare: prepareScrapTransformUiWithoutRefresh,
                restorePage: () => restoreTripPage(context.tripPageAddress),
                getter: snapshotSelectedScrapTransformState,
                uiExpectedFromValue: expectedScrapTransformTypeUiState,
                uiGetter: selectedScrapTransformTypeUiState,
                verifyEditedUi: false,
                verifyBaselineAfterCheckoutTimeoutMs: 10000,
                verifyResyncedValueTimeoutMs: 10000,
                setter: applySelectedScrapTransformState,
                nextValue: (state) => {
                    return nextTransformStateWithType(state, Scrap.ProjectedProfile)
                }
            })
        }

        function test_existingScrapManualPlanTransformSyncAndCheckout() {
            let context = loadFixtureAndOpenFirstTrip()

            SyncTestHelper.runProjectSyncRoundTrip(testCaseId, RootData, TestHelper, {
                tripPageAddress: context.tripPageAddress,
                prepare: prepareScrapTransformUi,
                restorePage: () => restoreTripPage(context.tripPageAddress),
                getter: snapshotSelectedScrapTransformState,
                uiExpectedFromValue: expectedScrapTransformUiState,
                uiGetter: selectedScrapTransformUiState,
                verifyEditedUi: false,
                verifyBaselineAfterCheckoutTimeoutMs: 10000,
                verifyResyncedValueTimeoutMs: 10000,
                setter: applySelectedScrapTransformState,
                nextValue: nextPlanManualTransformState
            })
        }

        function test_existingScrapManualProjectedProfileTransformSyncAndCheckout() {
            let context = loadFixtureAndOpenFirstTrip()

            SyncTestHelper.runProjectSyncRoundTrip(testCaseId, RootData, TestHelper, {
                tripPageAddress: context.tripPageAddress,
                prepare: prepareScrapTransformUiWithoutRefresh,
                restorePage: () => restoreTripPage(context.tripPageAddress),
                getter: snapshotSelectedScrapTransformState,
                uiExpectedFromValue: expectedScrapTransformUiState,
                uiGetter: selectedScrapTransformUiState,
                verifyEditedUi: false,
                verifyBaselineAfterCheckoutTimeoutMs: 10000,
                verifyResyncedValueTimeoutMs: 10000,
                setter: applySelectedScrapTransformState,
                nextValue: nextProjectedManualTransformState
            })
        }

        function test_existingScrapAddStationSyncAndCheckout() {
            let context = loadFixtureAndOpenFirstTrip()
            let addedStationName = "syncaddedstation"
            let prepareStationUi = function(stage) {
                prepareScrapOutlineUi(stage)

                let needsSelectionForUi = stage === "after-set-ui"
                                       || stage === "after-checkout-ui"
                                       || stage === "after-resync-ui"
                if (!needsSelectionForUi) {
                    return
                }

                let currentScrapView = scrapView()
                if (currentScrapView.selectScrapIndex !== 0) {
                    currentScrapView.selectScrapIndex = 0
                }
                tryVerifyWithDiagnostics(() => {
                    return currentScrapView.selectedScrapItem !== null
                           && currentScrapView.selectedScrapItem.scrap !== null
                }, 5000, "select scrap for station UI assertions")
            }

            SyncTestHelper.runProjectSyncRoundTrip(testCaseId, RootData, TestHelper, {
                tripPageAddress: context.tripPageAddress,
                prepare: prepareStationUi,
                restorePage: () => restoreTripPage(context.tripPageAddress),
                getter: snapshotSelectedScrapStationState,
                uiExpectedFromValue: (state) => {
                    return state.stationCount
                },
                uiGetter: () => {
                    return selectedScrapItem().stationView.count
                },
                verifyBaselineUi: true,
                verifyEditedUi: true,
                verifyCheckoutUi: true,
                verifyResyncUi: true,
                setter: applySelectedScrapStationState,
                nextValue: (state) => {
                    let nextState = nextStationStateWithAddedStation(state)
                    nextState.stations[nextState.stations.length - 1].name = addedStationName
                    return nextState
                }
            })
        }

        function test_existingScrapRemoveStationSyncAndCheckout() {
            let context = loadFixtureAndOpenFirstTrip()
            let prepareStationUi = function(stage) {
                prepareScrapOutlineUi(stage)

                let needsSelectionForUi = stage === "after-set-ui"
                                       || stage === "after-checkout-ui"
                                       || stage === "after-resync-ui"
                if (!needsSelectionForUi) {
                    return
                }

                let currentScrapView = scrapView()
                if (currentScrapView.selectScrapIndex !== 0) {
                    currentScrapView.selectScrapIndex = 0
                }
                tryVerifyWithDiagnostics(() => {
                    return currentScrapView.selectedScrapItem !== null
                           && currentScrapView.selectedScrapItem.scrap !== null
                }, 5000, "select scrap for station UI assertions")
            }

            SyncTestHelper.runProjectSyncRoundTrip(testCaseId, RootData, TestHelper, {
                tripPageAddress: context.tripPageAddress,
                prepare: prepareStationUi,
                restorePage: () => restoreTripPage(context.tripPageAddress),
                getter: snapshotSelectedScrapStationState,
                uiExpectedFromValue: (state) => {
                    return state.stationCount
                },
                uiGetter: () => {
                    return selectedScrapItem().stationView.count
                },
                verifyBaselineUi: true,
                verifyEditedUi: true,
                verifyCheckoutUi: true,
                verifyResyncUi: true,
                setter: applySelectedScrapStationState,
                nextValue: nextStationStateWithRemovedStation
            })
        }

        function test_existingScrapRenameStationSyncAndCheckout() {
            let context = loadFixtureAndOpenFirstTrip()
            let prepareStationUi = function(stage) {
                prepareScrapOutlineUi(stage)

                let needsSelectionForUi = stage === "after-set-ui"
                                       || stage === "after-checkout-ui"
                                       || stage === "after-resync-ui"
                if (!needsSelectionForUi) {
                    return
                }

                let currentScrapView = scrapView()
                if (currentScrapView.selectScrapIndex !== 0) {
                    currentScrapView.selectScrapIndex = 0
                }
                tryVerifyWithDiagnostics(() => {
                    return currentScrapView.selectedScrapItem !== null
                           && currentScrapView.selectedScrapItem.scrap !== null
                }, 5000, "select scrap for station rename UI assertions")
            }

            SyncTestHelper.runProjectSyncRoundTrip(testCaseId, RootData, TestHelper, {
                tripPageAddress: context.tripPageAddress,
                prepare: prepareStationUi,
                restorePage: () => restoreTripPage(context.tripPageAddress),
                getter: snapshotSelectedScrapStationState,
                uiExpectedFromValue: (state) => {
                    let names = []
                    for (let i = 0; i < state.stations.length; ++i) {
                        names.push(state.stations[i].name)
                    }
                    names.sort()
                    return names
                },
                uiGetter: () => {
                    return renderedSelectedScrapStationNames()
                },
                verifyBaselineUi: true,
                verifyEditedUi: true,
                verifyCheckoutUi: true,
                verifyResyncUi: true,
                setter: applySelectedScrapStationState,
                nextValue: nextStationStateWithRenamedStation
            })
        }

        function test_existingScrapModifyLeadSyncAndCheckout() {
            let context = loadFixtureAndOpenFirstTrip()
            let leadContext = findFirstNoteAndScrapIndexWithLeads()
            verify(leadContext !== null)

            let prepareLeadUi = function(stage) {
                disableNoteLoadUi()
                selectNoteIndex(leadContext.noteIndex, "select note for lead test")
                waitForNoteCanvasReady("wait for note canvas in lead test")
                enterCarpetMode()

                let currentScrapView = scrapView()
                tryVerifyWithDiagnostics(() => {
                    return currentScrapView.note === noteGallery().currentNote
                           && currentScrapView.count > leadContext.scrapIndex
                }, 5000, "bind scrap view for lead test")

                selectScrapIndex(leadContext.scrapIndex, "select scrap for lead test")
                let needsLeadSelection = stage === "baseline-ui"
                                      || stage === "after-set-ui"
                                      || stage === "after-checkout-ui"
                                      || stage === "after-resync-ui"
                if (!needsLeadSelection) {
                    return
                }

                let leadView = currentScrapView.selectedScrapItem.leadView
                tryVerifyWithDiagnostics(() => {
                    return leadView.count > 0
                }, 5000, "wait for leads in selected scrap")

                if (leadView.selectedItemIndex !== 0) {
                    leadView.selectedItemIndex = 0
                }

                tryVerifyWithDiagnostics(() => {
                    return leadView.count > 0
                           && leadView.selectedItemIndex === 0
                           && selectedLeadItem(0) !== null
                           && leadEditor().visible === true
                }, 5000, "select lead for ui test")
            }

            SyncTestHelper.runProjectSyncRoundTrip(testCaseId, RootData, TestHelper, {
                tripPageAddress: context.tripPageAddress,
                prepare: prepareLeadUi,
                restorePage: () => restoreTripPage(context.tripPageAddress),
                getter: snapshotSelectedScrapLeadsState,
                uiExpectedFromValue: expectedScrapLeadUiState,
                uiGetter: selectedScrapLeadUiState,
                verifyEditedUi: false,
                verifyBaselineAfterCheckoutTimeoutMs: 10000,
                verifyResyncedValueTimeoutMs: 10000,
                setter: applySelectedScrapLeadsState,
                nextValue: nextLeadState
            })
        }

        function test_existingScrapAddLeadSyncAndCheckout() {
            let context = loadFixtureAndOpenFirstTrip()
            let leadContext = findFirstNoteAndScrapIndexWithLeads()
            verify(leadContext !== null)

            let prepareLeadUi = function(stage) {
                disableNoteLoadUi()
                selectNoteIndex(leadContext.noteIndex, "select note for add lead test")
                waitForNoteCanvasReady("wait for note canvas in add lead test")
                enterCarpetMode()

                let currentScrapView = scrapView()
                tryVerifyWithDiagnostics(() => {
                    return currentScrapView.note === noteGallery().currentNote
                           && currentScrapView.count > leadContext.scrapIndex
                }, 5000, "bind scrap view for add lead test")

                selectScrapIndex(leadContext.scrapIndex, "select scrap for add lead test")

                let needsLeadSelection = stage === "baseline-ui"
                                      || stage === "after-set-ui"
                                      || stage === "after-checkout-ui"
                                      || stage === "after-resync-ui"
                if (!needsLeadSelection) {
                    return
                }

                let leadView = currentScrapView.selectedScrapItem.leadView
                let desiredLeadIndex = leadView.count > 0 ? leadView.count - 1 : -1
                if (leadView.selectedItemIndex !== desiredLeadIndex) {
                    leadView.selectedItemIndex = desiredLeadIndex
                }

                tryVerifyWithDiagnostics(() => {
                    if (leadView.count === 0) {
                        return leadView.selectedItemIndex < 0
                               && leadEditor().visible === false
                    }
                    return leadView.selectedItemIndex === desiredLeadIndex
                           && selectedLeadItem(desiredLeadIndex) !== null
                           && leadEditor().visible === true
                }, 5000, "select lead for add lead ui test")
            }

            SyncTestHelper.runProjectSyncRoundTrip(testCaseId, RootData, TestHelper, {
                tripPageAddress: context.tripPageAddress,
                prepare: prepareLeadUi,
                restorePage: () => restoreTripPage(context.tripPageAddress),
                getter: snapshotSelectedScrapLeadsState,
                uiExpectedFromValue: (state) => {
                    return expectedScrapLeadUiStateForSelection(state,
                                                                state.leadCount > 0 ? state.leadCount - 1 : -1)
                },
                uiGetter: selectedScrapLeadUiState,
                verifyEditedUi: true,
                verifyBaselineAfterCheckoutTimeoutMs: 10000,
                verifyResyncedValueTimeoutMs: 10000,
                setter: applySelectedScrapLeadsState,
                nextValue: nextLeadAddedState
            })
        }

        function test_existingScrapRemoveLeadSyncAndCheckout() {
            let context = loadFixtureAndOpenFirstTrip()
            let leadContext = findFirstNoteAndScrapIndexWithLeads()
            verify(leadContext !== null)

            let prepareLeadUi = function(stage) {
                disableNoteLoadUi()
                selectNoteIndex(leadContext.noteIndex, "select note for remove lead test")
                waitForNoteCanvasReady("wait for note canvas in remove lead test")
                enterCarpetMode()

                let currentScrapView = scrapView()
                tryVerifyWithDiagnostics(() => {
                    return currentScrapView.note === noteGallery().currentNote
                           && currentScrapView.count > leadContext.scrapIndex
                }, 5000, "bind scrap view for remove lead test")

                selectScrapIndex(leadContext.scrapIndex, "select scrap for remove lead test")

                let needsLeadSelection = stage === "baseline-ui"
                                      || stage === "after-set-ui"
                                      || stage === "after-checkout-ui"
                                      || stage === "after-resync-ui"
                if (!needsLeadSelection) {
                    return
                }

                let leadView = currentScrapView.selectedScrapItem.leadView
                if (leadView.count > 0 && leadView.selectedItemIndex !== 0) {
                    leadView.selectedItemIndex = 0
                }

                tryVerifyWithDiagnostics(() => {
                    if (leadView.count === 0) {
                        return leadView.selectedItemIndex < 0
                               && leadEditor().visible === false
                    }
                    return leadView.selectedItemIndex === 0
                           && selectedLeadItem(0) !== null
                           && leadEditor().visible === true
                }, 5000, "select lead for remove lead ui test")
            }

            SyncTestHelper.runProjectSyncRoundTrip(testCaseId, RootData, TestHelper, {
                tripPageAddress: context.tripPageAddress,
                prepare: prepareLeadUi,
                restorePage: () => restoreTripPage(context.tripPageAddress),
                getter: snapshotSelectedScrapLeadsState,
                uiExpectedFromValue: expectedScrapLeadUiState,
                uiGetter: selectedScrapLeadUiState,
                verifyEditedUi: true,
                verifyBaselineAfterCheckoutTimeoutMs: 10000,
                verifyResyncedValueTimeoutMs: 10000,
                setter: applySelectedScrapLeadsState,
                nextValue: nextLeadRemovedState
            })
        }
    }
}

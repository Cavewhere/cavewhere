import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

MainWindowTest {
    id: rootId

    TestCase {
        id: leadsTestCase
        name: "Leads"
        when: windowShown

        // Generous wait for click -> selection / popup-open round trips. The 3D
        // lead path runs offscreen with no QRhi, and the nightly suite runs
        // several test processes in parallel, so CPU starvation can stretch a
        // single click->select cycle well past the usual 2-5s budgets.
        readonly property int leadSelectTimeout: 10000

        function init() {
            // Dismiss any leftover text editor from a previous test
            if (GlobalShadowTextInput.coreClickInput !== null) {
                GlobalShadowTextInput.coreClickInput.closeEditor()
            }
            GlobalShadowTextInput.enabled = false

            RootData.futureManagerModel.waitForFinished()
            RootData.project.newProject()
            RootData.pageSelectionModel.currentPageAddress = "View"
            tryVerify(() => {
                return RootData.pageView.currentPageItem !== null
                       && RootData.pageView.currentPageItem.objectName === "viewPage"
            }, 5000, "should be on view page at start of test")
        }

        function cleanup() {
            RootData.pageSelectionModel.currentPageAddress = "View"
            RootData.newProject();
        }

        function clickToEdit(item) {
            mouseClick(item)
            tryVerify(() => GlobalShadowTextInput.textInput.activeFocus, 1000,
                      "Shadow text input should have focus after clicking " + item.objectName)
        }

        function setNoteOverlaysCollapsed(collapse) {
            let noteRes = ObjectFinder.findObjectByChain(
                mainWindow,
                "rootId->tripPage->noteGallery->noteArea->noteResolution")
            if(noteRes) noteRes.collapsed = collapse

            let transformEditor = ObjectFinder.findObjectByChain(
                mainWindow,
                "rootId->tripPage->noteGallery->noteArea->noteTransformEditor")
            if(transformEditor) transformEditor.collapsed = collapse
        }

        // Reliably open a 3D lead popup. A single tap on the marker can miss while
        // the offscreen 3D view is still settling (no QRhi) or while several test
        // processes contend for CPU, so retry the tap until the lead selects, then
        // wait for the async QuoteBox Loader. The retry is guarded at the top so a
        // tap that has already selected the lead is never repeated (which would
        // toggle it back closed). Returns {leadPoint, quoteBox}.
        function openLeadPopup(leadPointChain) {
            let leadPoint = null;
            tryVerify(() => {
                leadPoint = ObjectFinder.findObjectByChain(mainWindow, leadPointChain);
                return leadPoint !== null;
            }, leadsTestCase.leadSelectTimeout, "lead marker should exist: " + leadPointChain);

            tryVerify(() => {
                if (leadPoint.selected) {
                    return true
                }
                mouseClick(leadPoint)
                return leadPoint.selected
            }, leadsTestCase.leadSelectTimeout, "lead should select after tapping the marker");

            let quoteBox = null;
            tryVerify(() => {
                quoteBox = findChild(leadPoint, "leadQuoteBox")
                return quoteBox !== null;
            }, leadsTestCase.leadSelectTimeout, "lead quote box should open after selecting the marker");

            return {leadPoint: leadPoint, quoteBox: quoteBox};
        }

        function test_leads() {
            TestHelper.loadProjectFromFile(RootData.project, TestHelper.testcasesDatasetPath("test_cwProject/Phake Cave 3000.cw"));
            RootData.futureManagerModel.waitForFinished();

            //Click on the lead
            let {leadPoint: leadPoint0, quoteBox} = openLeadPopup("rootId->viewPage->SplitView->renderer->leadPoint2_1")

            verify(leadPoint0.scrap !== null )
            compare(leadPoint0.scrapId, 2)
            compare(leadPoint0.pointIndex, 1)

            let description_obj1 = findChild(quoteBox, "description")
            tryVerify(() => {return description_obj1.text === "Walking"})

            // View mode shows the size as a single read-only label ("2.5 × 2 m").
            let sizeText = findChild(quoteBox, "sizeText")
            tryVerify(() => sizeText !== null
                            && sizeText.text.indexOf("2.5") >= 0
                            && sizeText.text.indexOf("× 2") >= 0,
                      5000, "size label should read the lead's width × height")

            //Make sure there's leads in the lead table
            let dataButton_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->mainSideBar->dataButton")
            mouseClick(dataButton_obj1)

            tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "dataMainPage" });

            let caveButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->dataMainPage->caveDelegate0->caveLink")
            mouseClick(caveButton)

            tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "cavePage" });

            // Wait for the wide-layout LayoutItemProxy to position leadsLink before
            // clicking; otherwise the click lands on a stale (0,0) position over the
            // trip table and navigates to the trip page instead of the lead page.
            waitForRendering(rootId)
            let leadsButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->cavePage->leadsLink")
            mouseClick(leadsButton)

            tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "leadPage" });

            //Make sure the helpbox is not visible
            let noLeadsHelpbox = ObjectFinder.findObjectByChain(mainWindow, "rootId->leadPage->noLeadsHelpBox")
            tryVerify(()=>{ return !noLeadsHelpbox.visible });

            //Make sure all the leads are listed
            let tableView = ObjectFinder.findObjectByChain(mainWindow, "rootId->leadPage->leadTableView")
            compare(tableView.count, 5);
        }

        function test_navigateCavePageThenLeads() {
            // Regression: LayoutItemProxy reparenting on wide/narrow flip
            // crashed in QQuickWindowPrivate::polishItems on native macOS.
            TestHelper.loadProjectFromFile(RootData.project, TestHelper.testcasesDatasetPath("test_cwProject/Phake Cave 3000.cw"));
            RootData.futureManagerModel.waitForFinished();

            let cave = RootData.region.cave(0)
            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Cave=" + cave.name
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "cavePage",
                      5000, "should land on cavePage");

            let leadsLink = ObjectFinder.findObjectByChain(mainWindow, "rootId->cavePage->leadsLink")
            tryVerify(() => leadsLink !== null && leadsLink.visible,
                      5000, "leadsLink should be present on cavePage");
            waitForRendering(mainWindow)
            tryVerify(() => {
                if (RootData.pageView.currentPageItem.objectName !== "leadPage") {
                    mouseClick(leadsLink)
                    return false
                }
                return true
            }, 5000, "should land on leadPage after clicking leadsLink");

            let tableView = ObjectFinder.findObjectByChain(mainWindow, "rootId->leadPage->leadTableView")
            tryCompare(tableView, "count", 5);
        }

        function test_leadTable() {
            // Focused CaveLeadPage check that doesn't depend on the 3D
            // renderer (which is not functional under offscreen platform).
            TestHelper.loadProjectFromFile(RootData.project, TestHelper.testcasesDatasetPath("test_cwProject/Phake Cave 3000.cw"));
            RootData.futureManagerModel.waitForFinished();

            let cave = RootData.region.cave(0)
            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Cave=" + cave.name + "/Leads"

            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "leadPage",
                      5000, "should land on leadPage");

            let noLeadsHelpbox = ObjectFinder.findObjectByChain(mainWindow, "rootId->leadPage->noLeadsHelpBox")
            tryVerify(() => noLeadsHelpbox !== null && !noLeadsHelpbox.visible,
                      5000, "no-leads helpbox should be hidden when leads exist");

            let tableView = ObjectFinder.findObjectByChain(mainWindow, "rootId->leadPage->leadTableView")
            tryCompare(tableView, "count", 5);
        }

        function test_leadPositionCrash() {
            TestHelper.loadProjectFromFile(RootData.project, TestHelper.testcasesDatasetPath("test_cwProject/Phake Cave 3000.cw"));

            RootData.project.newProject();

            let _obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->SplitView->renderer->turnTableInteraction")
            mouseDrag(_obj1, 495.773, 291.668, 20, 20, Qt.LeftButton, Qt.NoModifier, 50)
        }

        function test_noLeadsMessage() {
            // Start from a freshly added cave (no leads) and navigate directly
            // to its Leads page via the page selection model — avoids flaky
            // mouse-click navigation under offscreen rendering.
            RootData.region.addCave()
            let cave = RootData.region.cave(RootData.region.caveCount - 1)
            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Cave=" + cave.name + "/Leads"

            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "leadPage",
                      5000, "should land on leadPage");

            //Make sure the helpbox is visible
            let noLeadsHelpbox = ObjectFinder.findObjectByChain(mainWindow, "rootId->leadPage->noLeadsHelpBox")
            tryVerify(() => noLeadsHelpbox !== null && noLeadsHelpbox.visible,
                      5000, "no-leads helpbox should be visible for an empty cave");
        }

        function addNewLeadToScrap() {
            // Returns {scrap, leadIndex, widthText, heightText} after adding a new lead
            TestHelper.loadProjectFromFile(RootData.project, TestHelper.testcasesDatasetPath("test_cwProject/Phake Cave 3000.cw"));
            // Wait for the load + scrap morph to settle; otherwise the retry-click
            // below hammers a scrap that isn't selectable yet and times out.
            RootData.futureManagerModel.waitForFinished();

            let cave = RootData.region.cave(0);
            let trip = cave.trip(0);
            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Cave=" + cave.name + "/Trip=" + trip.name;
            tryVerify(() => RootData.pageView.currentPageItem.objectName === "tripPage");

            let carpetButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->mainButtonArea->carpetButtonId");
            mouseClick(carpetButton);

            // wait() needed — the "" → "SELECT" transition includes PropertyAnimations
            // that reposition the toolbar; clicks miss during the animation
            wait(300)

            setNoteOverlaysCollapsed(true)

            let imageId_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->imageId")
            let noteArea = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea")
            let scrapViewEarly = findChild(noteArea, "scrapViewId")

            // Retry the click — under heavy CI load the carpet animation
            // may not have fully settled, causing the first click to miss
            mouseClick(imageId_obj1, 1854.2, 1091.57)
            tryVerify(() => {
                          if (scrapViewEarly.selectedScrapItem === null) {
                              mouseClick(imageId_obj1, 1854.2, 1091.57)
                              return false
                          }
                          return true
                      }, leadsTestCase.leadSelectTimeout, "scrap should be selected after image click")

            // Click addLeads — retry if the click doesn't register
            // (can happen when QML is still processing the SELECT state transition)
            let addLeads = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->addLeads")
            mouseClick(addLeads)
            tryVerify(() => {
                if (noteArea.state !== "ADD-LEAD") {
                    mouseClick(addLeads)
                    return false
                }
                return true
            }, leadsTestCase.leadSelectTimeout, "noteArea should be in ADD-LEAD state")

            // The ADD-LEAD transition uses ScriptAction which is async (fires
            // next frame after the state changes). Wait for the lead
            // interaction to be fully ready — enabled with a valid scrapView
            // on the handler — before clicking.
            let addLeadInteraction = findChild(noteArea, "addLeadInteraction")
            tryVerify(() => addLeadInteraction !== null
                            && addLeadInteraction.enabled
                            && addLeadInteraction.scrapView !== null,
                      leadsTestCase.leadSelectTimeout, "lead interaction should be enabled with valid scrapView")

            let scrapView = findChild(noteArea, "scrapViewId");
            verify(scrapView)
            tryVerify(() => scrapView.selectedScrapItem !== null,
                      leadsTestCase.leadSelectTimeout, "a scrap should be selected before placing the lead");

            let scrap = scrapView.selectedScrapItem.scrap;
            let leadCountBefore = scrap.numberOfLeads();

            // Place the lead. Don't retry-click: every click in ADD-LEAD mode adds
            // another lead, so just wait for the async add to register.
            let imageId_obj2 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->imageId")
            mouseClick(imageId_obj2, 2429.22, 732.923)
            tryVerify(() => scrap.numberOfLeads() === leadCountBefore + 1,
                      leadsTestCase.leadSelectTimeout, "clicking the note in ADD-LEAD mode should add a lead")

            let leadIndex = scrap.numberOfLeads() - 1;
            let widthText = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->leadEditor->widthText");
            let heightText = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->leadEditor->heightText");
            return {scrap, leadIndex, widthText, heightText};
        }

        // Regression test for: clicking "?" dim without changing it should not change display to "-1"
        function test_clickingQuestionMarkPreservesUnsetDim() {
            let {scrap, leadIndex, widthText} = addNewLeadToScrap();

            compare(widthText.text, "?")

            // Click "?" and commit without changing anything
            clickToEdit(widthText);
            keyClick(Qt.Key_Return);

            compare(widthText.text, "?")
            compare(scrap.leadData(Scrap.LeadSize, leadIndex).width, -1)
        }

        // Negative numbers should never be stored in the lead dim fields
        function test_negativeDimNotStored() {
            let {scrap, leadIndex, widthText} = addNewLeadToScrap();

            // Validator must be attached
            verify(widthText.validator !== null)

            // Try typing "-2": the '-' character is Invalid on its own so the TextInput
            // blocks it; '2' then replaces the selected "?". Either way, no negative
            // value should be committed.
            clickToEdit(widthText);
            keyClick(Qt.Key_Minus);
            keyClick(Qt.Key_2);
            keyClick(Qt.Key_Return);

            let storedWidth = scrap.leadData(Scrap.LeadSize, leadIndex).width;
            verify(storedWidth >= 0 || storedWidth === -1,
                   "Width must never be a user-entered negative, got: " + storedWidth)
            verify(widthText.text !== "-2", "Display must not show a negative value")
        }

        // Regression test for: clearing a dim field to empty should reset to "?" (unset), not show "" or "0"
        function test_clearingDimFieldResetsToUnset() {
            let {scrap, leadIndex, widthText, heightText} = addNewLeadToScrap();

            // First set a valid width
            clickToEdit(widthText);
            keyClick(51, 0); // '3'
            keyClick(Qt.Key_Return);
            compare(widthText.text, "3")

            // Clear the width field — should revert to "?" not "" or "0"
            clickToEdit(widthText);
            keyClick(Qt.Key_A, Qt.ControlModifier); // select all
            keyClick(Qt.Key_Delete);
            keyClick(Qt.Key_Return);

            compare(widthText.text, "?")
            compare(scrap.leadData(Scrap.LeadSize, leadIndex).width, -1)
        }

        function test_addLeadAndViewDetails() {
            let {widthText, heightText} = addNewLeadToScrap();

            // Fill in lead details
            clickToEdit(widthText);
            keyClick(51, 0); // '3'

            clickToEdit(heightText);
            keyClick(50, 0); // '2'

            let description = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->leadEditor->description");
            mouseClick(description);
            tryVerify(() => description.activeFocus, 1000,
                      "Description should have focus after click");
            keyClick("N");
            keyClick("e");
            keyClick("w");
            keyClick(" ");
            keyClick("l");
            keyClick("e");
            keyClick("a");
            keyClick("d");

            // Switch to 3D view
            let viewButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->mainSideBar->viewButton");
            mouseClick(viewButton);
            tryVerify(() => RootData.pageView.currentPageItem.objectName === "viewPage");

            let {quoteBox} = openLeadPopup("rootId->viewPage->SplitView->renderer->leadPoint1_2")
            let sizeDisplay = findChild(quoteBox, "sizeText");
            let descriptionDisplay = findChild(quoteBox, "description");

            verify(sizeDisplay.text.indexOf("3 ×") >= 0,
                   "size label should show width 3, got: " + sizeDisplay.text);
            verify(sizeDisplay.text.indexOf("× 2") >= 0,
                   "size label should show height 2, got: " + sizeDisplay.text);
            compare(descriptionDisplay.text, "New lead");
        }

        // #516: the 3D lead popup is a read-only summary (size label + status pill)
        // with an Edit button that flips it into the editable LeadInfoForm. The
        // billboard/popup is not rendered under the offscreen platform, so this
        // covers the view-mode chrome and the edit toggle; the field-editing and
        // commit path is the shared LeadInfoForm, exercised in the 2D notes tests.
        function test_editLeadInView3D() {
            TestHelper.loadProjectFromFile(RootData.project, TestHelper.testcasesDatasetPath("test_cwProject/Phake Cave 3000.cw"));
            RootData.futureManagerModel.waitForFinished();

            let {quoteBox} = openLeadPopup("rootId->viewPage->SplitView->renderer->leadPoint2_1")

            // View mode: read-only summary — size label, status pill, Notes link,
            // and no editable controls.
            verify(!quoteBox.editMode, "popup should start in view mode")
            verify(findChild(quoteBox, "sizeText") !== null, "size label should show")
            verify(findChild(quoteBox, "completedStatus") !== null, "status pill should show")
            verify(findChild(quoteBox, "gotoNotes") !== null, "Notes link should show")
            verify(findChild(quoteBox, "completed") === null,
                   "no editable checkbox should exist in view mode")

            // Flip into edit mode with the Edit button: the read-only summary is
            // replaced by the editable LeadInfoForm controls.
            let editButton = findChild(quoteBox, "leadEditButton")
            verify(editButton !== null, "edit button should exist")
            mouseClick(editButton)
            tryVerify(() => quoteBox.editMode, 1000, "edit button should enter edit mode")

            tryVerify(() => {
                let completed = findChild(quoteBox, "completed")
                return completed !== null && completed.enabled
            }, 1000, "completed checkbox should appear and be editable in edit mode")
            verify(findChild(quoteBox, "widthText") !== null, "width input should appear")
            verify(findChild(quoteBox, "heightText") !== null, "height input should appear")
        }

        // #516: editing a lead's fields through the shared LeadInfoForm commits
        // back to the scrap. Driven through the 2D notes editor, which renders as
        // normal QML (the 3D popup uses the same form but needs the RHI billboard).
        function test_editLeadFieldsCommit() {
            let {scrap, leadIndex, widthText, heightText} = addNewLeadToScrap();

            // Width and height.
            clickToEdit(widthText);
            keyClick(53, 0); // '5'
            keyClick(Qt.Key_Return);
            tryCompare(scrap.leadData(Scrap.LeadSize, leadIndex), "width", 5);

            clickToEdit(heightText);
            keyClick(55, 0); // '7'
            keyClick(Qt.Key_Return);
            tryCompare(scrap.leadData(Scrap.LeadSize, leadIndex), "height", 7);

            // Description.
            let description = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->leadEditor->description");
            mouseClick(description);
            tryVerify(() => description.activeFocus, 1000, "description should focus on click");
            keyClick("G"); keyClick("o"); keyClick("e"); keyClick("s");
            tryVerify(() => scrap.leadData(Scrap.LeadDesciption, leadIndex) === "Goes",
                      1000, "description edit should commit to the scrap");

            // Completed checkbox (new in the shared form).
            let completed = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->leadEditor->completed");
            verify(completed !== null, "completed checkbox should exist in the lead editor");
            verify(!scrap.leadData(Scrap.LeadCompleted, leadIndex), "lead should start not completed");
            mouseClick(completed);
            tryVerify(() => scrap.leadData(Scrap.LeadCompleted, leadIndex) === true,
                      1000, "checking the box should commit completed to the scrap");
        }

        // #516: the lead editor stays in two-way sync with the scrap — editing a
        // field commits to the model (view -> model), and changing the model
        // updates the field (model -> view). The model -> view checks run AFTER a
        // UI edit to confirm the declarative bindings keep tracking the lead once
        // the user has interacted with the controls. Driven through the 2D notes
        // editor, which renders as normal QML offscreen.
        function test_leadEditorTwoWaySync() {
            let {scrap, leadIndex, widthText, heightText} = addNewLeadToScrap();

            let description = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->leadEditor->description");
            let completed = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->leadEditor->completed");
            verify(description !== null, "description field should exist");
            verify(completed !== null, "completed checkbox should exist");

            // view -> model: edits commit to the scrap (and break the bindings).
            mouseClick(description);
            tryVerify(() => description.activeFocus, 1000, "description should focus on click");
            keyClick("G"); keyClick("o");
            tryVerify(() => scrap.leadData(Scrap.LeadDesciption, leadIndex) === "Go",
                      1000, "typing should commit to the scrap");

            verify(!scrap.leadData(Scrap.LeadCompleted, leadIndex), "lead should start not completed");
            mouseClick(completed);
            tryVerify(() => scrap.leadData(Scrap.LeadCompleted, leadIndex) === true,
                      1000, "checking the box should commit to the scrap");

            clickToEdit(widthText);
            keyClick(53, 0); // '5'
            keyClick(Qt.Key_Return);
            tryCompare(scrap.leadData(Scrap.LeadSize, leadIndex), "width", 5);

            // model -> view: changing the scrap directly updates the fields, even
            // though the UI edits above broke their declarative bindings.
            scrap.setLeadData(Scrap.LeadDesciption, leadIndex, "External edit");
            tryCompare(description, "text", "External edit", 5000,
                       "description should follow an external model change");

            scrap.setLeadData(Scrap.LeadCompleted, leadIndex, false);
            tryCompare(completed, "checked", false, 5000,
                       "completed should follow an external model change");

            scrap.setLeadData(Scrap.LeadSize, leadIndex, Qt.size(8, 9));
            tryCompare(widthText, "text", "8", 5000,
                       "width should follow an external model change");
            tryCompare(heightText, "text", "9", 5000,
                       "height should follow an external model change");
        }

        // Lead popups act like a dialog: tapping empty space closes the open popup.
        function test_clickAwayClosesLeadPopup() {
            TestHelper.loadProjectFromFile(RootData.project, TestHelper.testcasesDatasetPath("test_cwProject/Phake Cave 3000.cw"));
            RootData.futureManagerModel.waitForFinished();

            let {leadPoint} = openLeadPopup("rootId->viewPage->SplitView->renderer->leadPoint2_1")

            // Tap empty space — click the renderer corner opposite the lead so the
            // tap can't land on this (or, most likely, any) lead.
            let renderer = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->SplitView->renderer")
            let leadPos = leadPoint.mapToItem(renderer, 0, 0)
            let cornerX = (leadPos.x > renderer.width * 0.5) ? 5 : renderer.width - 5
            let cornerY = (leadPos.y > renderer.height * 0.5) ? 5 : renderer.height - 5
            mouseClick(renderer, cornerX, cornerY)

            tryVerify(() => !leadPoint.selected, 2000, "lead should deselect after tapping empty space")
            tryVerify(() => {
                return ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->SplitView->renderer->leadPoint2_1->leadQuoteBox") === null;
            }, 2000, "lead quote box should close after tapping away")
        }

        // The empty-space dismiss TapHandler must NOT also fire on a lead tap
        // (it uses the default DragThreshold/passive grab). If it did, tapping a
        // selected lead would re-clear-then-reselect (or never stick). Tapping a
        // selected lead a second time must toggle it closed.
        function test_tapSelectedLeadTogglesClosed() {
            TestHelper.loadProjectFromFile(RootData.project, TestHelper.testcasesDatasetPath("test_cwProject/Phake Cave 3000.cw"));
            RootData.futureManagerModel.waitForFinished();

            let {leadPoint} = openLeadPopup("rootId->viewPage->SplitView->renderer->leadPoint2_1")

            // Second tap on the now-selected lead must toggle it closed. This is the
            // behaviour under test, so it stays a single tap (no retry).
            mouseClick(leadPoint)
            tryVerify(() => !leadPoint.selected, leadsTestCase.leadSelectTimeout,
                      "tapping the selected lead again should close it")
            tryVerify(() => {
                return ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->SplitView->renderer->leadPoint2_1->leadQuoteBox") === null;
            }, leadsTestCase.leadSelectTimeout, "popup should close on second tap")
        }

        // 4a: a click on a lead hidden behind cave geometry must be ignored (the
        // click must not pass through walls). cwLeadView::isOccluded gates the
        // tap; here we drive it directly. A visible lead (the one the other tests
        // click through) is not occluded; a point far down the same eye->lead ray,
        // well behind the surface the lead rests on, is.
        function test_occludedLeadIsNotClickable() {
            TestHelper.loadProjectFromFile(RootData.project, TestHelper.testcasesDatasetPath("test_cwProject/Phake Cave 3000.cw"));
            RootData.futureManagerModel.waitForFinished();

            let leadPoint = null;
            tryVerify(() => {
                leadPoint = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->SplitView->renderer->leadPoint2_1");
                return leadPoint !== null;
            });

            let leadView = leadPoint.leadView;
            verify(leadView !== null, "leadView should be injected into LeadPoint");
            verify(leadView.camera !== null, "leadView needs a camera for the occlusion ray");

            let leadPos = leadPoint.position3D;

            // isOccluded now tests the tapped viewport pixel. The lead item is
            // positioned with its origin at the projected marker centre, so map
            // that into the leadView (viewport) space for the tap point.
            let tap = leadPoint.mapToItem(leadView, 0, 0);
            verify(!leadView.isOccluded(leadPos, tap), "a visible lead must not be occluded");

            // A point deep along the view direction (behind the wall the lead
            // sits on) projects to the same pixel but its billboard plane is far
            // behind the cave surface, so the surface occludes it. Using the view
            // forward keeps this correct under both ortho and perspective.
            let vm = leadView.camera.viewMatrix;
            let forward = Qt.vector3d(-vm.m31, -vm.m32, -vm.m33).normalized();
            let deepPoint = leadPos.plus(forward.times(1000));
            verify(leadView.isOccluded(deepPoint, tap), "a point behind cave geometry must be occluded");
        }

        // 4b: open the layers tab, set the first filter row to the Type key, and
        // return the keyword-list checkbox for the given Type value (or null).
        function typeValueCheckbox(value) {
            let layersTab = findChild(rootId.mainWindow, "layersTabButton");
            verify(layersTab !== null);
            mouseClick(layersTab);

            let delegate = null;
            tryVerify(() => {
                delegate = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->SplitView->renderingSidePanel->keyword->groupListView->andListView_0->delegate_0");
                return delegate !== null;
            });

            let keyCombo = findChild(delegate, "keyCombo");
            verify(keyCombo);
            keyCombo.currentIndex = keyCombo.model.indexOf("Type");
            delegate.filterModelObjectRole.key = keyCombo.currentText;
            verify(delegate.filterModelObjectRole.key === "Type");

            let filterModel = delegate.filterModelObjectRole;
            tryVerify(() => filterModel.rowCount() > 0);

            let rowIndex = -1;
            for(let i = 0; i < filterModel.rowCount(); ++i) {
                let rowValue = filterModel.data(filterModel.index(i, 0), KeywordGroupByKeyModel.ValueRole);
                if(rowValue === value) {
                    rowIndex = i;
                    break;
                }
            }
            verify(rowIndex >= 0, "there should be a '" + value + "' Type row");

            let checkbox = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->SplitView->renderingSidePanel->keyword->groupListView->andListView_0->delegate_0->keywordList->row" + rowIndex + "->checkbox");
            verify(checkbox);
            return checkbox;
        }

        // 4b: unchecking the "Lead" Type keyword hides every lead; rechecking it
        // brings them back. Leads register a Type=Lead keyword item per scrap, so
        // they filter just like scraps.
        function test_leadKeywordVisibilityTogglesLeads() {
            TestHelper.loadProjectFromFile(RootData.project, TestHelper.testcasesDatasetPath("test_cwProject/Phake Cave 3000.cw"));
            RootData.futureManagerModel.waitForFinished();

            RootData.pageSelectionModel.gotoPageByName(null, "View");

            let leadPoint = null;
            tryVerify(() => {
                leadPoint = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->SplitView->renderer->leadPoint2_1");
                return leadPoint !== null;
            });
            tryVerify(() => leadPoint.visible, 5000, "lead should start visible");

            let leadCheckbox = typeValueCheckbox("Lead");

            mouseClick(leadCheckbox);
            tryVerify(() => !leadPoint.visible, 5000, "lead should hide when the Lead Type is filtered out");

            mouseClick(leadCheckbox);
            tryVerify(() => leadPoint.visible, 5000, "lead should return when the Lead Type is re-enabled");
        }

        // 4b: hiding a scrap's leads via keyword while one is selected closes the
        // popup (the QuoteBox is a sibling overlay that would otherwise linger
        // pointing at a now-hidden marker).
        function test_hidingLeadKeywordClosesSelectedPopup() {
            TestHelper.loadProjectFromFile(RootData.project, TestHelper.testcasesDatasetPath("test_cwProject/Phake Cave 3000.cw"));
            RootData.futureManagerModel.waitForFinished();

            RootData.pageSelectionModel.gotoPageByName(null, "View");

            let {leadPoint} = openLeadPopup("rootId->viewPage->SplitView->renderer->leadPoint2_1")

            let leadCheckbox = typeValueCheckbox("Lead");
            mouseClick(leadCheckbox);

            tryVerify(() => leadPoint.selectionManager.selectedItem === null, 5000,
                      "selection should clear when the selected lead's scrap is hidden");
            tryVerify(() => {
                return ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->SplitView->renderer->leadPoint2_1->leadQuoteBox") === null;
            }, 5000, "popup should close when the lead is keyword-hidden");
        }

        // Regression test for GitHub issue #368:
        // When a lead and a scrap outline point overlap, clicking selects both
        // via their TapHandlers. The outline point's deselection cascade
        // (through cwSelectionManager) incorrectly clears the lead's selection,
        // so the lead info editor never appears.
        function test_clickingLeadInCarpetShowsInfo() {
            TestHelper.loadProjectFromFile(RootData.project, TestHelper.testcasesDatasetPath("test_cwProject/Phake Cave 3000.cw"));
            RootData.futureManagerModel.waitForFinished();

            // Navigate to the trip page
            let cave = RootData.region.cave(0);
            let trip = cave.trip(0);
            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Cave=" + cave.name + "/Trip=" + trip.name;
            tryVerify(() => RootData.pageView.currentPageItem.objectName === "tripPage");

            // Enter carpet mode
            let carpetButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->mainButtonArea->carpetButtonId");
            mouseClick(carpetButton);

            // wait() needed — the "" → "SELECT" transition includes PropertyAnimations
            // that reposition the toolbar; clicks miss during the animation
            wait(300);

            let noteArea = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea");
            let scrapView = findChild(noteArea, "scrapViewId");
            verify(scrapView !== null);

            // Find a scrap with leads
            let scrapWithLeads = -1;
            for (let i = 0; i < scrapView.count; i++) {
                scrapView.selectScrapIndex = i;
                let item = scrapView.selectedScrapItem;
                if (item && item.scrap && item.scrap.numberOfLeads() > 0) {
                    scrapWithLeads = i;
                    break;
                }
            }
            verify(scrapWithLeads >= 0, "Should find a scrap with leads");

            let selectedScrap = scrapView.selectedScrapItem;
            let leadView = selectedScrap.leadView;
            let outlinePointView = selectedScrap.outlinePointView;
            verify(leadView.count > 0);
            verify(outlinePointView.count > 0);

            // Get references to a lead and an outline point
            leadView.selectedItemIndex = 0;
            let noteLead = leadView.selectedItem();
            verify(noteLead !== null);
            leadView.selectedItemIndex = -1;

            outlinePointView.selectedItemIndex = 0;
            let outlinePoint = outlinePointView.selectedItem();
            verify(outlinePoint !== null);
            outlinePointView.selectedItemIndex = -1;

            let editor = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->leadEditor");
            verify(!editor.visible, "Lead editor should start hidden");

            // Simulate overlapping click: both TapHandlers fire on the same click.
            // The outline point's select() fires first, then the lead's select().
            // Issue #368: the lead's selection is cleared by the outline point's
            // deselection cascade through cwSelectionManager.
            outlinePoint.select();
            noteLead.select();

            verify(noteLead.selected,
                   "Lead should be selected after overlapping select (issue #368)");
            verify(leadView.selectedItemIndex >= 0,
                   "leadView.selectedItemIndex should be >= 0 after overlapping select (issue #368)");
            tryVerify(() => editor.visible);
        }
    }
}

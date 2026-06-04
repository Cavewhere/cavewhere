import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

MainWindowTest {
    id: rootId

    TestCase {
        name: "Leads"
        when: windowShown

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

        function test_leads() {
            TestHelper.loadProjectFromFile(RootData.project, TestHelper.testcasesDatasetPath("test_cwProject/Phake Cave 3000.cw"));
            RootData.futureManagerModel.waitForFinished();

            //Click on the lead
            let leadPoint0 = null;
            tryVerify(() => {
                leadPoint0 = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderer->leadPoint2_1");
                return leadPoint0 !== null;
            });
            mouseClick(leadPoint0)

            verify(leadPoint0.scrap !== null )
            compare(leadPoint0.scrapId, 2)
            compare(leadPoint0.pointIndex, 1)

            //This doesn't work be leadPoint0 doesn't have a consistant name between runs,

            let quoteBox = null;
                    tryVerify(() => {
                                    quoteBox = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderer->leadPoint2_1->leadQuoteBox")
                                    return quoteBox !== null;
                        })

            let description_obj1 = findChild(quoteBox, "description")
            tryVerify(() => {return description_obj1.text === "Walking"})

            let width = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderer->leadPoint2_1->leadQuoteBox->widthText")
            tryCompare(width, "text", "2.5")

            let height = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderer->leadPoint2_1->leadQuoteBox->heightText")
            tryCompare(height, "text", "2")

            //Make sure there's leads in the lead table
            let dataButton_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->mainSideBar->dataButton")
            mouseClick(dataButton_obj1)

            tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "dataMainPage" });

            let caveButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->dataMainPage->caveDelegate0->caveLink")
            mouseClick(caveButton)

            tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "cavePage" });

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

            let _obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderer->turnTableInteraction")
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
                      }, 5000, "scrap should be selected after image click")

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
            }, 5000, "noteArea should be in ADD-LEAD state")

            // The ADD-LEAD transition uses ScriptAction which is async (fires
            // next frame after the state changes). Wait for the lead
            // interaction to be fully ready — enabled with a valid scrapView
            // on the handler — before clicking.
            let addLeadInteraction = findChild(noteArea, "addLeadInteraction")
            tryVerify(() => addLeadInteraction !== null
                            && addLeadInteraction.enabled
                            && addLeadInteraction.scrapView !== null,
                      5000, "lead interaction should be enabled with valid scrapView")

            let imageId_obj2 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->imageId")
            mouseClick(imageId_obj2, 2429.22, 732.923)

            let scrapView = findChild(noteArea, "scrapViewId");
            verify(scrapView)
            tryVerify(() => scrapView.selectedScrapItem !== null);

            let scrap = scrapView.selectedScrapItem.scrap;
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

            let leadObj = null
            tryVerify(() => {
                          leadObj = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderer->leadPoint1_2")
                          return leadObj !== null
                      })
            mouseClick(leadObj)

            let quoteBox = null
            tryVerify(() => {
                          quoteBox = findChild(leadObj, "leadQuoteBox")
                          return quoteBox !== null
                      })
            let widthDisplay = findChild(quoteBox, "widthText");
            let heightDisplay = findChild(quoteBox, "heightText");
            let descriptionDisplay = findChild(quoteBox, "description");

            compare(widthDisplay.text, "3");
            compare(heightDisplay.text, "2");
            compare(descriptionDisplay.text, "New lead");
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

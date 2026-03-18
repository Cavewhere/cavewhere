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

        function cleanup() {
            RootData.newProject();
        }

        function test_leads() {
            TestHelper.loadProjectFromFile(RootData.project, "://datasets/test_cwProject/Phake Cave 3000.cw");
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
            compare(width.text, "2.5")

            let height = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderer->leadPoint2_1->leadQuoteBox->heightText")
            compare(height.text, "2")

            //Make sure there's leads in the lead table
            let dataButton_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->mainSideBar->dataButton")
            mouseClick(dataButton_obj1)

            tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "dataMainPage" });

            let caveButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->dataMainPage->caveDelegate0->caveLink")
            mouseClick(caveButton)

            tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "cavePage" });

            let leadsButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->cavePage->leadsButton->label")
            mouseClick(leadsButton)

            tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "leadPage" });

            //Make sure the helpbox is not visible
            let noLeadsHelpbox = ObjectFinder.findObjectByChain(mainWindow, "rootId->leadPage->noLeadsHelpBox")
            tryVerify(()=>{ return !noLeadsHelpbox.visible });

            //Make sure all the leads are listed
            let tableView = ObjectFinder.findObjectByChain(mainWindow, "rootId->leadPage->leadTableView")
            compare(tableView.count, 5);
        }

        function test_leadPositionCrash() {
            TestHelper.loadProjectFromFile(RootData.project, "://datasets/test_cwProject/Phake Cave 3000.cw");

            RootData.project.newProject();

            let _obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderer->turnTableInteraction")
            mouseDrag(_obj1, 495.773, 291.668, 20, 20, Qt.LeftButton, Qt.NoModifier, 50)
        }

        function test_noLeadsMessage() {
            let dataButton_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->mainSideBar->dataButton")
            mouseClick(dataButton_obj1)

            tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "dataMainPage" });

            let addCave = ObjectFinder.findObjectByChain(mainWindow, "rootId->dataMainPage->addButton")
            mouseClick(addCave)

            tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "cavePage" });

            let leadsButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->cavePage->leadsButton")
            mouseClick(leadsButton)

            tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "leadPage" });

            //Make sure the helpbox is visible
            let noLeadsHelpbox = ObjectFinder.findObjectByChain(mainWindow, "rootId->leadPage->noLeadsHelpBox")
            tryVerify(()=>{ return noLeadsHelpbox.visible });
        }

        function addNewLeadToScrap() {
            // Returns {scrap, leadIndex, widthText, heightText} after adding a new lead
            TestHelper.loadProjectFromFile(RootData.project, "://datasets/test_cwProject/Phake Cave 3000.cw");

            let cave = RootData.region.cave(0);
            let trip = cave.trip(0);
            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Cave=" + cave.name + "/Trip=" + trip.name;
            tryVerify(() => RootData.pageView.currentPageItem.objectName === "tripPage");

            let carpetButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->carpetButtonId");
            mouseClick(carpetButton);
            wait(300)

            let imageId_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->imageId")
            mouseClick(imageId_obj1, 1854.2, 1091.57)

            let addLeads = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->addLeads")
            mouseClick(addLeads)

            let imageId_obj2 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->imageId")
            mouseClick(imageId_obj2, 2429.22, 732.923)
            wait(200)

            let noteArea = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea")
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
            mouseClick(widthText, 2, 2);
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
            mouseClick(widthText, 2, 2);
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
            mouseClick(widthText, 2, 2);
            keyClick(51, 0); // '3'
            keyClick(Qt.Key_Return);
            compare(widthText.text, "3")

            // Clear the width field — should revert to "?" not "" or "0"
            mouseClick(widthText, 2, 2);
            keyClick(Qt.Key_A, Qt.ControlModifier); // select all
            keyClick(Qt.Key_Delete);
            keyClick(Qt.Key_Return);

            compare(widthText.text, "?")
            compare(scrap.leadData(Scrap.LeadSize, leadIndex).width, -1)
        }

        function test_addLeadAndViewDetails() {
            let {widthText, heightText} = addNewLeadToScrap();

            // Fill in lead details
            mouseClick(widthText, 2, 2);
            keyClick(51, 0); // '3'

            mouseClick(heightText, 2, 2);
            keyClick(50, 0); // '2'

            let description = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->leadEditor->description");
            mouseClick(description, 2, 2);
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

            wait(300)
            let leadObj = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderer->leadPoint1_2")
            tryVerify(() => leadObj !== null)
            mouseClick(leadObj)

            wait(100);

            let quoteBox = findChild(leadObj, "leadQuoteBox");
            verify(quoteBox)
            let widthDisplay = findChild(quoteBox, "widthText");
            let heightDisplay = findChild(quoteBox, "heightText");
            let descriptionDisplay = findChild(quoteBox, "description");

            compare(widthDisplay.text, "3");
            compare(heightDisplay.text, "2");
            compare(descriptionDisplay.text, "New lead");
        }
    }
}

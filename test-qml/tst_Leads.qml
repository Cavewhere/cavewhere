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
            RootData.project.newProject();
            RootData.pageSelectionModel.currentPageAddress = "Data"
        }

        function test_leads() {
            TestHelper.loadProjectFromFile(RootData.project, "://datasets/test_cwProject/Phake Cave 3000.cw");

            //Click on the lead
            let leadPoint0 = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderer->leadPoint2_1")
            mouseClick(leadPoint0)

            verify(leadPoint0.scrap !== null )
            compare(leadPoint0.scrapId, 2)
            compare(leadPoint0.pointIndex, 1)

            wait(100);

            //This doesn't work be leadPoint0 doesn't have a consistant name between runs,
            let quoteBox = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderer->leadPoint2_1->leadQuoteBox")


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
    }
}

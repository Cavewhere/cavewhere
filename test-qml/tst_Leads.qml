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

        function test_leads() {
            TestHelper.loadProjectFromFile(RootData.project, "://datasets/test_cwProject/Phake Cave 3000.cw");

            //Click on the lead
            let leadPoint0 = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderer->leadPoint0")
            mouseClick(leadPoint0)

            verify(leadPoint0.scrap !== null )
            verify(leadPoint0.pointIndex >= 0 )

            wait(100);

            let quoteBox = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderer->leadPoint0->leadQuoteBox0")

            let description_obj1 = findChild(quoteBox, "description")
            tryVerify(() => {return description_obj1.text === "Pit"})

        }
    }
}

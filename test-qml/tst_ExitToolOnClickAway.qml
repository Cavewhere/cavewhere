import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

// Reproduces bug #342: clicking away from the carpet area
// (page change or image switch) must exit the active carpeting tool.
MainWindowTest {
    id: rootId

    TestCase {
        name: "ExitToolOnClickAway"
        when: windowShown

        function init() {
            RootData.futureManagerModel.waitForFinished()
            RootData.project.newProject()
            RootData.pageSelectionModel.currentPageAddress = "View"
            // newProject() triggers async cleanup (futures, scrap manager,
            // page reloads); a single tryVerify is too narrow to cover it.
            wait(1000)
        }

        function loadProjectAndEnterCarpet() {
            TestHelper.loadProjectFromFile(
                RootData.project,
                TestHelper.testcasesDatasetPath("test_cwScrapManager/ProjectProfile-test-v3.cw"))
            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Cave=Cave 1/Trip=Trip 1"
            tryVerify(() => { return RootData.pageView.currentPageItem.objectName === "tripPage" })

            let carpetButton = ObjectFinder.findObjectByChain(
                rootId.mainWindow, "rootId->tripPage->noteGallery->carpetButtonId")
            mouseClick(carpetButton)

            // The "" → "SELECT" transition runs PropertyAnimations that
            // reposition the toolbar; subsequent clicks miss during the
            // animation.
            wait(500)
        }

        function activateAddScrap() {
            let addScrapButton = ObjectFinder.findObjectByChain(
                rootId.mainWindow, "rootId->tripPage->noteGallery->addScrapButton")
            mouseClick(addScrapButton)
        }

        function activateAddStation() {
            let addStationButton = ObjectFinder.findObjectByChain(
                rootId.mainWindow, "rootId->tripPage->noteGallery->addScrapStation")
            mouseClick(addStationButton)
        }

        function activateAddLead() {
            let addLeadButton = ObjectFinder.findObjectByChain(
                rootId.mainWindow, "rootId->tripPage->noteGallery->addLeads")
            mouseClick(addLeadButton)
        }

        function noteArea() {
            return ObjectFinder.findObjectByChain(
                rootId.mainWindow, "rootId->tripPage->noteGallery->noteArea")
        }

        function noteGallery() {
            return ObjectFinder.findObjectByChain(
                rootId.mainWindow, "rootId->tripPage->noteGallery")
        }

        function verifyToolExited(noteAreaItem) {
            tryVerify(() => { return noteAreaItem.state === "" || noteAreaItem.state === "SELECT" },
                      5000,
                      `noteArea.state should be "" or "SELECT" but is "${noteAreaItem.state}"`)

            let addLead = ObjectFinder.findObjectByChain(rootId.mainWindow,
                "rootId->tripPage->noteGallery->noteArea->addLeadInteraction")
            if (addLead !== null) {
                verify(addLead.enabled === false,
                       "addLeadInteraction should be disabled after click-away")
            }
        }

        function checkToolExitsOnPageChange(activateFn, expectedState) {
            loadProjectAndEnterCarpet()
            activateFn()

            let na = noteArea()
            tryVerify(() => { return na.state === expectedState })

            let viewButton = ObjectFinder.findObjectByChain(
                rootId.mainWindow, "rootId->mainSideBar->viewButton")
            mouseClick(viewButton)
            tryVerify(() => { return RootData.pageView.currentPageItem.objectName === "viewPage" })

            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Cave=Cave 1/Trip=Trip 1"
            tryVerify(() => { return RootData.pageView.currentPageItem.objectName === "tripPage" })

            verifyToolExited(noteArea())
        }

        function test_addScrapExitsOnPageChange() {
            checkToolExitsOnPageChange(activateAddScrap, "ADD-SCRAP")
        }

        function test_addStationExitsOnPageChange() {
            checkToolExitsOnPageChange(activateAddStation, "ADD-STATION")
        }

        function test_addLeadExitsOnPageChange() {
            checkToolExitsOnPageChange(activateAddLead, "ADD-LEAD")
        }

        function test_addScrapExitsOnImageChange() {
            TestHelper.loadProjectFromFile(
                RootData.project,
                TestHelper.testcasesDatasetPath("test_cwScrapManager/ProjectProfile-test-v3.cw"))
            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Cave=Cave 1/Trip=Trip 1"
            tryVerify(() => { return RootData.pageView.currentPageItem.objectName === "tripPage" })

            let gallery = noteGallery()
            let galleryView = ObjectFinder.findObjectByChain(
                rootId.mainWindow, "rootId->tripPage->noteGallery->galleryView")
            verify(galleryView.count === 1)

            let secondImagePath = TestHelper.copyToTempDirUrl(
                TestHelper.testcasesDatasetPath("test_cwTextureUploadTask/PhakeCave.PNG"))
            gallery.imagesAdded([secondImagePath])
            tryVerify(() => { return galleryView.count === 2 })

            // Adding a note auto-selects the new image (index 1); move
            // back to index 0 before activating the tool so the click-away
            // afterwards is unambiguous.
            galleryView.currentIndex = 0
            tryCompare(galleryView, "currentIndex", 0)

            let carpetButton = ObjectFinder.findObjectByChain(
                rootId.mainWindow, "rootId->tripPage->noteGallery->carpetButtonId")
            mouseClick(carpetButton)
            wait(500)
            activateAddScrap()

            let na = noteArea()
            tryVerify(() => { return na.state === "ADD-SCRAP" })

            galleryView.currentIndex = 1
            tryCompare(galleryView, "currentIndex", 1)

            verifyToolExited(noteArea())
        }
    }
}

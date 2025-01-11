import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

MainWindowTest {
    id: rootId

    TestCase {
        name: "NoteNorthInteraction"
        when: windowShown

        function test_arrowInteraction() {

            TestHelper.loadProjectFromFile(RootData.project, "://datasets/test_cwScrapManager/ProjectProfile-test-v3.cw");
            RootData.pageSelectionModel.currentPageAddress = "Data/Cave=Cave 1/Trip=Trip 1"

            tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "tripPage" });

            // wait(1000)

            let _obj1 = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->noteGallery->carpetButtonId")
            mouseClick(_obj1)

            wait(200)

            //Select scrap
            let noteItem = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->noteGallery->noteArea->imageId")
            mouseClick(noteItem, 464.938, 829.673)

            wait(200)

            let typeComboBox = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->noteGallery->noteArea->typeComboBox")
            verify(typeComboBox.currentText === "Project Profile")

            //Select North arrow up
            let setNorthButton_obj1 = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->noteGallery->noteArea->autoCalculate->setNorthButton")
            mouseClick(setNorthButton_obj1)

            let imageId_obj1 = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->noteGallery->noteArea->imageId")
            mouseClick(imageId_obj1, 695, 1141)
            mouseClick(imageId_obj1, 695, 739)

            //Check current scrap
            //0.0
            let noteArea = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->noteGallery->noteArea");
            let scrapView = findChild(noteArea, "scrapViewId")
            let scrap = scrapView.selectedScrapItem.scrap as Scrap
            fuzzyCompare(scrap.noteTransformation.northUp, 0.0, 0.001, `${scrap.noteTransformation.northUp} === 0.0`);

            //90
            mouseClick(setNorthButton_obj1)
            mouseClick(imageId_obj1, 695, 739)
            mouseClick(imageId_obj1, 947, 739)
            fuzzyCompare(scrap.noteTransformation.northUp, 90.0, 0.001, `${scrap.noteTransformation.northUp} === 90.0`);

            //180
            mouseClick(setNorthButton_obj1)
            mouseClick(imageId_obj1, 695, 739)
            mouseClick(imageId_obj1, 695, 1141)
            fuzzyCompare(scrap.noteTransformation.northUp, 180.0, 0.001, `${scrap.noteTransformation.northUp} === 180.0`);

            //270
            mouseClick(setNorthButton_obj1)
            mouseClick(imageId_obj1, 947, 739)
            mouseClick(imageId_obj1, 695, 739)
            fuzzyCompare(scrap.noteTransformation.northUp, 270.0, 0.001, `${scrap.noteTransformation.northUp} === 270.0`);
        }
    }
}

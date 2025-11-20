import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

MainWindowTest {
    id: rootId

    CWTestCase {
        name: "NoteScaleInteraction"
        when: windowShown


        function test_noteScaleInteraction() {
            TestHelper.loadProjectFromFile(RootData.project, "://datasets/test_cwScrapManager/ProjectProfile-test-v3.cw");
            RootData.pageSelectionModel.currentPageAddress = "Data/Cave=Cave 1/Trip=Trip 1"

            tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "tripPage" });

            //Select carpet
            let _obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->carpetButtonId")
            mouseClick(_obj1);

            //Wait until the carpet is Selectd
            let noteGallery = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery")
            let carpetTransition = findChild(noteGallery, "toCarpetTransition")
            tryVerify(()=>{ return !carpetTransition.running });

            wait(1000);

            //Select scrap
            let imageId_obj2 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->imageId")
            mouseClick(imageId_obj2, 475.801, 600.855)
            wait(200)

            let setLengthButton_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->noteTransformEditor->autoCalculate->setLengthButton")
            mouseClick(setLengthButton_obj1)

            // let imageId_obj2 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->imageId")
            mouseClick(imageId_obj2, 645.111, 738.692)
            mouseClick(imageId_obj2, 777.563, 738.692)

            //Click on the text label
            let lengthText = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->noteScaleInteraction->lengthUnitValue->coreTextInput")
            mouseClick(lengthText, 0.867188, 12.7031)

            //Type 10
            keyClick(49, 0) //1
            keyClick(48, 0) //0
            keyClick(16777220, 0) //Return

            //Check current scrap value
            let noteArea = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->noteGallery->noteArea");
            let scrapView = findChild(noteArea, "scrapViewId")
            let scrap = scrapView.selectedScrapItem.scrap as Scrap
            verify(scrap !== null)
            fuzzyCompare(1.0 / scrap.noteTransformation.scale, 256.252914, 0.001, `1.0 / ${scrap.noteTransformation.scale} === 256.252914`);

            //press done button
            let done_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->noteScaleInteraction->doneButton->label")
            mouseClick(done_obj1)

            //Make sure the scale updates correctly
            tryFuzzyCompare(1.0 / scrap.noteTransformation.scale, 567.4924, 0.001, `1.0 / ${scrap.noteTransformation.scale} === 567.4924`);
        }
    }
}

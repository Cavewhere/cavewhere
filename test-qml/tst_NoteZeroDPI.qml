import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

MainWindowTest {
    id: rootId

    TestCase {
        name: "NoteZeroDPI"
        when: windowShown


        /**
         * Test: NoteZeroDPI
         *
         * Given:
         * - A project is loaded with the current page set to "Trip 1".
         * - The tripPage is displayed.
         *
         * When:
         * - The user interacts with the noteDPI input, attempting invalid and valid values.
         * - The setResolution tool is used to select points and validate DPI changes.
         *
         * Then:
         * - The noteDPI remains unchanged for invalid input (e.g., 0 DPI).
         * - The noteDPI updates correctly for valid input (e.g., 500 DPI).
         * - The doneButton prevents invalid values and updates DPI appropriately for valid inputs.
         */
        function test_noteScaleInteraction() {
            TestHelper.loadProjectFromFile(RootData.project, "://datasets/tst_NoteZeroDPI/test.cw");
            RootData.pageSelectionModel.currentPageAddress = "Data/Cave=Cave 1/Trip=Trip 1"

            tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "tripPage" });

            //Select carpet
            let _obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->carpetButtonId")
            mouseClick(_obj1);

            wait(500);

            //Text DPI should be correct
            let text = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->noteDPI->coreTextInput")
            tryCompare(text, "text", "300.96", 100);
            mouseClick(text)
            wait(100);

            //User shouldn't be allowed to set the DPI to zero because this will prevent scraps from
            //warping correctly
            keyClick(48, 0) //0
            keyClick(16777220, 0) //Return
            wait(100);
            tryCompare(text, "text", "300.96", 100);

            //Make sure we can change the DPI to something Validator
            mouseClick(text)
            wait(100);
            keyClick(53, 0) //5
            keyClick(48, 0) //0
            keyClick(48, 0) //0
            keyClick(16777220, 0) //Return
            wait(100);
            tryCompare(text, "text", "500", 100);

            //Use the DPI tool, and prevent the user from accepting invalid DPI
            let setResolution_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->setResolution")
            mouseClick(setResolution_obj1)

            let imageId_obj2 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->imageId")
            mouseClick(imageId_obj2, 738.779, 446.844)
            mouseClick(imageId_obj2, 1040.35, 447.425)

            //Done button should be disabled
            let done = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->doneButton")
            compare(done.enabled, false);

            //Done button should do nothing if clicked
            mouseClick(done)
            tryCompare(text, "text", "500", 100);

            //Put a reasonable value in like 1 inch
            let lengthUnitValue = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->lengthUnitValue->coreTextInput")
            mouseClick(lengthUnitValue)
            keyClick(49, 0) //1
            keyClick(16777220, 0) //Return
            let label_obj3 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->doneButton->label")
            mouseClick(done)

            //Make sure the value updates
            fuzzyCompare(Number(text.text), 302.87, 1.0)
        }
    }
}

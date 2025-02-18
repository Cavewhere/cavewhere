import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

MainWindowTest {
    id: rootId

    TestCase {
        name: "ImportCSVData"
        when: windowShown


        function test_importCSVDataFromFile() {

            RootData.pageSelectionModel.currentPageAddress = "Data"
            tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "dataMainPage" });

            let importButton_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->dataMainPage->importButton")
            mouseClick(importButton_obj1)

            let csvMenuItem = ObjectFinder.findObjectByChain(mainWindow, "csvMenuItem")
            mouseClick(csvMenuItem)

            tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "csvImporterPage" });

            let csvManager = findChild(RootData.pageView.currentPageItem, "csvManager")
            verify(csvManager !== null)
            csvManager.filename = "://datasets/test_cwCSVImporterManager/allCustomColumns.txt"

            //Move the columns to the right order
            let availableColumns = ObjectFinder.findObjectByChain(mainWindow, "rootId->csvImporterPage->GroupBox->GroupBox->availableColumns");
            let usedColumns = ObjectFinder.findObjectByChain(mainWindow, "rootId->csvImporterPage->GroupBox->GroupBox->usedColumns");

            let length = ObjectFinder.findObjectByChain(mainWindow, "rootId->csvImporterPage->GroupBox->GroupBox->usedColumns->columnNameDelegate_Length")
            mouseDrag(length, 1, 5, -50, 0, Qt.LeftButton, Qt.NoModifier, 100)
            tryVerify(()=>{ return usedColumns.model.get(0).name === "Length" });

            let compassBack = ObjectFinder.findObjectByChain(mainWindow, "rootId->csvImporterPage->GroupBox->GroupBox->availableColumns->columnNameDelegate_Compass Backsight")
            mouseDrag(compassBack, 1, 10, 38, 60, Qt.LeftButton, Qt.NoModifier, 50)
            tryVerify(()=>{ return usedColumns.model.get(1).name === "Compass Backsight" });

            let clinoBack = ObjectFinder.findObjectByChain(mainWindow, "rootId->csvImporterPage->GroupBox->GroupBox->availableColumns->columnNameDelegate_Clino Backsight")
            mouseDrag(clinoBack, 1, 10, 141, 60, Qt.LeftButton, Qt.NoModifier, 50)
            tryVerify(()=>{ return usedColumns.model.get(2).name === "Clino Backsight" });

            let left = ObjectFinder.findObjectByChain(mainWindow, "rootId->csvImporterPage->GroupBox->GroupBox->availableColumns->columnNameDelegate_Left")
            mouseDrag(left, 1, 10, 339, 60, Qt.LeftButton, Qt.NoModifier, 50)
            tryVerify(()=>{ return usedColumns.model.get(7).name === "Left" });

            let right = ObjectFinder.findObjectByChain(mainWindow, "rootId->csvImporterPage->GroupBox->GroupBox->availableColumns->columnNameDelegate_Right")
            mouseDrag(right, 1, 10, 368, 60, Qt.LeftButton, Qt.NoModifier, 50)
            tryVerify(()=>{ return usedColumns.model.get(8).name === "Right" });

            let up = ObjectFinder.findObjectByChain(mainWindow, "rootId->csvImporterPage->GroupBox->GroupBox->availableColumns->columnNameDelegate_Up")
            mouseDrag(up, 1, 10, 399, 60, Qt.LeftButton, Qt.NoModifier, 50)
            tryVerify(()=>{ return usedColumns.model.get(9).name === "Up" });

            let down = ObjectFinder.findObjectByChain(mainWindow, "rootId->csvImporterPage->GroupBox->GroupBox->availableColumns->columnNameDelegate_Down")
            mouseDrag(down, 1, 10, 420, 60, Qt.LeftButton, Qt.NoModifier, 50)
            tryVerify(()=>{ return usedColumns.model.get(10).name === "Down" });

            compare(availableColumns.model.count, 1);
            compare(availableColumns.model.get(0).name, "Skip");
            compare(usedColumns.model.count, 11);

            //Scoll to the bottom
            let importPageScrollBar = ObjectFinder.findObjectByChain(mainWindow, "rootId->csvImporterPage->verticalScrollBar")
            importPageScrollBar.position = 1.0

            wait(50);

            //Import
            let importButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->csvImporterPage->import")
            mouseClick(importButton)

            tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "dataMainPage" });

            wait(200);

            let caveLength = ObjectFinder.findObjectByChain(mainWindow, "rootId->dataMainPage->caveDelegate0->length->coreTextInput");
            tryCompare(caveLength, "text", "55.6")
            let caveDepth = ObjectFinder.findObjectByChain(mainWindow, "rootId->dataMainPage->caveDelegate0->depth->coreTextInput");
            tryCompare(caveDepth, "text", "19.54")
        }

    }
}

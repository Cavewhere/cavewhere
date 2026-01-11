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

            RootData.pageSelectionModel.currentPageAddress = "Source/Data"
            tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "dataMainPage" });

            let importButton_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->dataMainPage->importButton")
            mouseClick(importButton_obj1)

            // wait(100000)

            let csvMenuItem = findChild(mainWindow, "csvMenuItem")
            mouseClick(csvMenuItem)

            tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "csvImporterPage" });

            let csvManager = findChild(RootData.pageView.currentPageItem, "csvManager")
            verify(csvManager !== null)
            csvManager.filename = "://datasets/test_cwCSVImporterManager/allCustomColumns.txt"

            //Move the columns to the right order
            let availableColumns = ObjectFinder.findObjectByChain(mainWindow, "rootId->csvImporterPage->GroupBox->GroupBox->availableColumns");
            let usedColumns = ObjectFinder.findObjectByChain(mainWindow, "rootId->csvImporterPage->GroupBox->GroupBox->usedColumns");

            let from = ObjectFinder.findObjectByChain(mainWindow, "rootId->csvImporterPage->GroupBox->GroupBox->usedColumns->columnNameDelegate_From")
            let clino = ObjectFinder.findObjectByChain(mainWindow, "rootId->csvImporterPage->GroupBox->GroupBox->usedColumns->columnNameDelegate_Clino")
            let length = ObjectFinder.findObjectByChain(mainWindow, "rootId->csvImporterPage->GroupBox->GroupBox->usedColumns->columnNameDelegate_Length")
            let delta = length.mapToItem(from, 5, 5);
            mouseDrag(length, 1, 5, -delta.x, -delta.y, Qt.LeftButton, Qt.NoModifier, 100)
            tryVerify(()=>{ return usedColumns.model.get(0).name === "Length" });

            wait(100)

            let compassBack = ObjectFinder.findObjectByChain(mainWindow, "rootId->csvImporterPage->GroupBox->GroupBox->availableColumns->columnNameDelegate_Compass Backsight")
            delta = compassBack.mapToItem(from, 5, -5);
            mouseDrag(compassBack, 1, 10, -delta.x, -delta.y, Qt.LeftButton, Qt.NoModifier, 50)
            tryVerify(()=>{ return usedColumns.model.get(1).name === "Compass Backsight" });

            wait(100)

            let clinoBack = ObjectFinder.findObjectByChain(mainWindow, "rootId->csvImporterPage->GroupBox->GroupBox->availableColumns->columnNameDelegate_Clino Backsight")
            delta = clinoBack.mapToItem(from, 5, -5);
            mouseDrag(clinoBack, 1, 10, -delta.x, -delta.y, Qt.LeftButton, Qt.NoModifier, 50)
            tryVerify(()=>{ return usedColumns.model.get(2).name === "Clino Backsight" });

            wait(100)

            let left = ObjectFinder.findObjectByChain(mainWindow, "rootId->csvImporterPage->GroupBox->GroupBox->availableColumns->columnNameDelegate_Left")
            delta = left.mapToItem(clino, -clino.width, -5);
            mouseDrag(left, 1, 10, -delta.x, -delta.y, Qt.LeftButton, Qt.NoModifier, 50)
            tryVerify(()=>{ return usedColumns.model.get(7).name === "Left" });

            wait(100)

            left = ObjectFinder.findObjectByChain(mainWindow, "rootId->csvImporterPage->GroupBox->GroupBox->usedColumns->columnNameDelegate_Left")
            let right = ObjectFinder.findObjectByChain(mainWindow, "rootId->csvImporterPage->GroupBox->GroupBox->availableColumns->columnNameDelegate_Right")
            delta = right.mapToItem(left, -left.width, -5);
            mouseDrag(right, 1, 10, -delta.x, -delta.y, Qt.LeftButton, Qt.NoModifier, 50)
            tryVerify(()=>{ return usedColumns.model.get(8).name === "Right" });

            wait(100)

            right = ObjectFinder.findObjectByChain(mainWindow, "rootId->csvImporterPage->GroupBox->GroupBox->usedColumns->columnNameDelegate_Right")
            let up = ObjectFinder.findObjectByChain(mainWindow, "rootId->csvImporterPage->GroupBox->GroupBox->availableColumns->columnNameDelegate_Up")
            delta = up.mapToItem(right, -right.width, -5);
            mouseDrag(up, 1, 10, -delta.x, -delta.y, Qt.LeftButton, Qt.NoModifier, 50)
            tryVerify(()=>{ return usedColumns.model.get(9).name === "Up" });

            wait(100)

            up = ObjectFinder.findObjectByChain(mainWindow, "rootId->csvImporterPage->GroupBox->GroupBox->usedColumns->columnNameDelegate_Up")
            let down = ObjectFinder.findObjectByChain(mainWindow, "rootId->csvImporterPage->GroupBox->GroupBox->availableColumns->columnNameDelegate_Down")
            delta = down.mapToItem(up, -up.width - 3, -5);
            mouseDrag(down, 1, 10, -delta.x, -delta.y, Qt.LeftButton, Qt.NoModifier, 50)
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

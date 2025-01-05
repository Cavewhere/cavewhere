import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

MainWindowTest {
    id: rootId

    TestCase {
        name: "SurveyDataEntry"
        when: windowShown

        function test_enterSurveyData() {
            TestHelper.loadProjectFromFile(RootData.project, "://datasets/test_cwScrapManager/ProjectProfile-test-v3.cw");
            RootData.pageSelectionModel.currentPageAddress = "Data/Cave=Cave 1"

            tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "cavePage" });

            let addCaveButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->cavePage->addTrip->addButton")
            mouseClick(addCaveButton)

            tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "tripPage" });

            let addSuveyButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->addSurveyData")
            mouseClick(addSuveyButton)

            wait(200);

            //Start adding survey data
            keyClick("b")
            keyClick(49, 0) //1
            keyClick(16777220, 0) //Return

            //Make sure that we go to the next station entry
            keyClick(16777217, 0) //Tab got to the next cell
            keyClick(16777217, 0) //Tab add the next station B2

            //Check that we have two stations
            let trip = RootData.pageView.currentPageItem.currentTrip as Trip;
            verify(trip !== null);

            let firstChunk = trip.chunk(0);
            verify(firstChunk.stationCount() === 3)
            verify(firstChunk.data(SurveyChunk.StationNameRole, 0) === "b1")
            verify(firstChunk.data(SurveyChunk.StationNameRole, 1) === "b2")
            verify(firstChunk.data(SurveyChunk.StationNameRole, 2) === "")


            //Enter distance of 10
            // verify(firstChunk.data(SurveyChunk.ShotCompassRole, 0) === "")
            keyClick(49, 0) //1
            keyClick(48, 0) //0
            keyClick(16777217, 0) //Tab
            verify(firstChunk.data(SurveyChunk.ShotDistanceRole, 0) === "10")
            wait(50);

            //Compass of 0
            // verify(firstChunk.data(SurveyChunk.ShotCompassRole, 0) === "")
            keyClick(48, 0) //0
            keyClick(16777217, 0) //Tab
            verify(firstChunk.data(SurveyChunk.ShotCompassRole, 0) === "0")
            wait(50);

            //Skip back compass
            // verify(firstChunk.data(SurveyChunk.ShotBackCompassRole, 0) === "")
            keyClick(16777217, 0) //Tab
            // verify(firstChunk.data(SurveyChunk.ShotBackCompassRole, 0) === "")

            //Enter 11 for clino
            // verify(firstChunk.data(SurveyChunk.ShotClinoRole, 0) === "")
            keyClick(49, 0) //1
            keyClick(49, 0) //1
            keyClick(16777217, 0) //Tab
            verify(firstChunk.data(SurveyChunk.ShotClinoRole, 0) === "11")
            wait(50);

            //Skip the LRUD
            // keyClick(16777217, 0) //Tab
            keyClick(16777217, 0) //Tab
            keyClick(16777217, 0) //Tab
            keyClick(16777217, 0) //Tab
            keyClick(16777217, 0) //Tab
            keyClick(16777217, 0) //Tab
            keyClick(16777217, 0) //Tab
            keyClick(16777217, 0) //Tab
            keyClick(16777217, 0) //Tab
            keyClick(16777217, 0) //Tab

            //Connect to a1
            keyClick("a")
            keyClick(49, 0) //1
            keyClick(16777217, 0) //Tab
            verify(firstChunk.data(SurveyChunk.StationNameRole, 2) === "a1")
            wait(50);

            //Distance of 1
            keyClick(49, 0) //1
            keyClick(16777217, 0) //Tab
            verify(firstChunk.data(SurveyChunk.ShotDistanceRole, 1) === "1")
            wait(50);

            //Compasss of 10
            keyClick(49, 0) //1
            keyClick(48, 0) //0
            keyClick(16777217, 0) //Tab
            verify(firstChunk.data(SurveyChunk.ShotCompassRole, 1) === "10")
            wait(50);

            //Skip backcompass
            keyClick(16777217, 0) //Tab

            //Clino of 0
            keyClick(48, 0) //0
            keyClick(16777217, 0) //Tab
            verify(firstChunk.data(SurveyChunk.ShotClinoRole, 1) === "0")
            wait(50);
        }
    }
}

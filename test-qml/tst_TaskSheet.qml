import QtQuick as QQ
import QtTest
import QmlTestRecorder
import cavewherelib

QQ.Item {
    id: rootId
    objectName: "rootId"

    width: 400
    height: 500

    // Stands in for the combined task model, which cannot be given a job from
    // QML. The sheet takes its model as a QtObject for exactly this reason.
    QQ.ListModel {
        id: fakeJobsId
    }

    TaskSheet {
        id: sheetId
        objectName: "taskSheet"
        anchors.fill: parent

        model: fakeJobsId
    }

    TestCase {
        name: "TaskSheet"
        when: windowShown

        // Automatic Update is a real persisted setting rather than an injected
        // property, so this file writes the one the whole binary shares. Put
        // back what it found, or every test file after this one inherits it.
        property bool originalAutomaticUpdate: false

        function initTestCase() {
            originalAutomaticUpdate = RootData.updateCoordinator.automaticUpdate
        }

        function cleanupTestCase() {
            RootData.updateCoordinator.automaticUpdate = originalAutomaticUpdate
        }

        function init() {
            fakeJobsId.clear()
            sheetId.opened = false
            RootData.updateCoordinator.automaticUpdate = false
        }

        // Everything below the header is inside the card, which is named, so it
        // is part of the chain to every one of them.
        function find(name) {
            return ObjectFinder.findObjectByChain(
                rootId, "rootId->taskSheet->taskSheetCard->" + name)
        }

        function addJob(name) {
            fakeJobsId.append({"nameRole": name, "progressRole": 0, "numberOfStepsRole": 0})
        }

        function test_closedWithTasksRunningStaysOffScreen() {
            addJob("Saving")
            verify(!sheetId.shown, "running is not by itself a reason to be on screen")
        }

        function test_openingListsTheRunningJobs() {
            addJob("Loading entrance.laz")
            addJob("Saving")

            sheetId.toggle()
            verify(sheetId.shown, "the sheet opened")
            waitForRendering(sheetId)

            let list = find("taskSheetList")
            verify(list !== null, "taskSheetList not found")
            compare(list.count, 2, "both jobs are listed")

            compare(find("flyoutCardTitle").text, "2 Tasks")
        }

        // Nothing to open onto, so a tap must not bank an open that fires later
        // on an unrelated job.
        function test_openingWithNothingToListDoesNothing() {
            sheetId.toggle()
            verify(!sheetId.opened, "the open was not banked")

            addJob("Syncing")
            verify(!sheetId.shown, "so an unrelated job does not bring it up")
        }

        function test_itFallsAwayWhenTheLastJobFinishes() {
            addJob("Saving")
            sheetId.toggle()
            verify(sheetId.shown, "open with a job listed")

            fakeJobsId.clear()
            verify(!sheetId.shown, "gone with the last job")
            verify(!sheetId.opened, "and it did not stay latched open")
        }

        // The scrim is how a phone closes something like this, and it is the only
        // close that does not require hitting a small ×.
        function test_tappingOutsideClosesIt() {
            addJob("Saving")
            sheetId.toggle()
            waitForRendering(sheetId)

            mouseClick(rootId, rootId.width / 2, 10)
            verify(!sheetId.shown, "tapping above the sheet closed it")
        }

        // The other half of the scrim: a tap aimed at the sheet must not be
        // taken as a tap past it. The header is the case that bites — it is
        // plain chrome, so nothing there consumes the press on its own.
        function test_tappingTheSheetItselfLeavesItOpen() {
            addJob("Saving")
            sheetId.toggle()
            waitForRendering(sheetId)

            let title = find("flyoutCardTitle")
            verify(title !== null, "flyoutCardTitle not found")

            mouseClick(title)
            verify(sheetId.shown, "tapping the title did not close it")

            let card = ObjectFinder.findObjectByChain(rootId, "rootId->taskSheet->taskSheetCard")
            mouseClick(card, 2, card.height - 2)
            verify(sheetId.shown, "nor did tapping the card's own padding")
        }

        // The chip that opens this is a toggle, so the second tap has to close
        // it — the × and the scrim go through a different path.
        function test_togglingAgainClosesIt() {
            addJob("Saving")

            sheetId.toggle()
            verify(sheetId.shown, "the first toggle opened it")

            sheetId.toggle()
            verify(!sheetId.shown, "and the second closed it")
        }

        function test_theCloseButtonClosesIt() {
            addJob("Saving")
            sheetId.toggle()
            waitForRendering(sheetId)

            let closeButton = find("flyoutCardCloseButton")
            verify(closeButton !== null, "flyoutCardCloseButton not found")

            mouseClick(closeButton)
            verify(!sheetId.shown, "the close button closed it")
        }

        // The hamburger menu keeps its own copy of this; both write the same
        // value, so this is the one you reach for while already looking at the
        // work it governs.
        function test_theSheetCarriesAutomaticUpdate() {
            addJob("Saving")
            sheetId.toggle()
            waitForRendering(sheetId)

            let toggle = find("taskSheetAutoUpdateSwitch")
            verify(toggle !== null, "taskSheetAutoUpdateSwitch not found")

            // Driven from both ends, since false is also the switch's own
            // default — asserting only that would agree with a sheet that
            // ignored the setting the hamburger menu shares with it.
            RootData.updateCoordinator.automaticUpdate = true
            verify(toggle.checked, "it reports the shared setting")
            RootData.updateCoordinator.automaticUpdate = false
            verify(!toggle.checked, "and follows it back")

            mouseClick(toggle)
            verify(RootData.updateCoordinator.automaticUpdate,
                   "and toggling it turns automatic update on")
        }
    }
}

import QtQuick
import QtTest
import QmlTestRecorder
import cavewherelib

Item {
    id: rootId
    objectName: "rootId"

    // As wide as the wide sidebar, so the footer lays out the way it really does.
    width: Theme.sidebarWidthFull
    height: 200

    SideBarUpdateFooter {
        id: footerId
        objectName: "updateFooter"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        running: false
        needsUpdate: false
        automaticUpdate: false
    }

    SignalSpy {
        id: runSpy
        target: footerId
        signalName: "runRequested"
    }

    SignalSpy {
        id: toggleSpy
        target: footerId
        signalName: "automaticUpdateToggled"
    }

    TestCase {
        name: "SideBarUpdateFooter"
        when: windowShown

        function init() {
            footerId.running = false
            footerId.needsUpdate = false
            footerId.automaticUpdate = false
            footerId.compact = false
            // Pinned rather than left bound to the live task/future models, so a
            // task the rest of the suite happens to be running can't move it.
            footerId.taskCount = 0
            runSpy.clear()
            toggleSpy.clear()
        }

        function find(name) {
            return ObjectFinder.findObjectByChain(rootId, "rootId->updateFooter->" + name)
        }

        function test_idleShowsTheAutomaticUpdateToggle() {
            verify(find("autoUpdateCheckbox") !== null, "the checkbox is shown when idle")
            verify(find("updateRunButton") === null, "no Run button when idle")
            verify(find("updateRunningIndicator") === null, "no busy indicator when idle")
        }

        function test_togglingAutomaticUpdateReportsTheNewValue() {
            let checkbox = find("autoUpdateCheckbox")
            verify(checkbox !== null, "autoUpdateCheckbox not found")

            // The click needs the layout polished, so its coordinates are real.
            waitForRendering(footerId)
            mouseClick(checkbox)
            compare(toggleSpy.count, 1, "checking the box reports once")
            compare(toggleSpy.signalArguments[0][0], true,
                    "checking the box asks for automatic update on")
        }

        function test_staleShowsRunAndAsksForTheUpdate() {
            footerId.needsUpdate = true
            waitForRendering(footerId)

            verify(find("autoUpdateCheckbox") === null, "the idle toggle gives way to Run")
            let runButton = find("updateRunButton")
            verify(runButton !== null, "updateRunButton not found")

            mouseClick(runButton)
            compare(runSpy.count, 1, "pressing Run asks for the update once")
        }

        // The whole reason the coordinator publishes running separately: a
        // pipeline re-edited mid-run reports Dirty while its task is still
        // churning, so both aggregates are true and the footer has to pick one.
        function test_runningWinsOverNeedsUpdate() {
            footerId.needsUpdate = true
            footerId.running = true

            verify(find("updateRunningIndicator") !== null, "the busy indicator is shown")
            verify(find("updateRunButton") === null, "Run is not offered while running")
        }

        // Jobs that never reach the update coordinator still have to read as busy;
        // see the component header for which ones and why.
        function test_aJobOutsideTheCoordinatorStillReadsAsBusy() {
            footerId.taskCount = 1

            verify(!footerId.running, "the coordinator reports nothing running")
            verify(footerId.busy, "a tracked job counts as busy")
            verify(find("updateRunningIndicator") !== null, "the busy indicator is shown")
        }

        // The count can be zero while work really is in flight, so the label has
        // to survive that; see the component header for how it happens.
        function test_runningLabelDropsTheCountWhenNoTaskIsListed() {
            footerId.running = true

            let label = find("updateRunningLabel")
            verify(label !== null, "updateRunningLabel not found")
            compare(label.text, "Running…")

            footerId.taskCount = 1
            compare(label.text, "Running 1 Task")

            footerId.taskCount = 3
            compare(label.text, "Running 3 Tasks")
        }

        function test_compactHidesTheLabelsAndSwapsInTheIconToggle() {
            footerId.compact = true

            let label = find("autoUpdateLabel")
            verify(label !== null, "autoUpdateLabel not found")
            verify(!label.visible, "the caption is hidden when compact")
            verify(find("autoUpdateToggle").visible, "the icon toggle takes over when compact")

            footerId.needsUpdate = true
            verify(!find("updateNeededLabel").visible, "the stale caption is hidden when compact")
            verify(find("updateRunButton").visible, "Run stays reachable when compact")
        }
    }
}

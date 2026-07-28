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

    SignalSpy {
        id: tasksSpy
        target: footerId
        signalName: "tasksRequested"
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
            footerId.tasksShown = false
            footerId.busyRowHovered = false
            runSpy.clear()
            toggleSpy.clear()
            tasksSpy.clear()
            // Park the pointer clear of the footer, so a test that hovered the
            // busy row can't leave it hovered for whatever runs next.
            mouseMove(rootId, rootId.width - 1, 0)
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

        // The busy row is the way into the task flyout; the footer only reports
        // the tap, since the flyout is the host's to own.
        function test_tappingTheBusyRowAsksForTheTaskList() {
            footerId.running = true
            waitForRendering(footerId)

            let indicator = find("updateRunningIndicator")
            verify(indicator !== null, "updateRunningIndicator not found")

            mouseClick(indicator)
            compare(tasksSpy.count, 1, "tapping the busy row asks for the tasks once")
        }

        // The chevron is the row's only cue that there is more behind it, and it
        // points at the flyout, so it lights up while that flyout is on screen.
        function test_busyRowShowsAnExpandChevron() {
            footerId.running = true
            footerId.taskCount = 1

            let chevron = find("updateRunningExpandIcon")
            verify(chevron !== null, "updateRunningExpandIcon not found")
            verify(chevron.visible, "the chevron is shown when there are jobs to list")

            compare(chevron.colorizationColor, Theme.icon, "at rest it is the plain icon color")

            footerId.tasksShown = true
            compare(chevron.colorizationColor, Theme.accent,
                    "it lights up while the flyout is shown")
        }

        // A cascade between pipelines is busy with nothing listed yet, and the
        // flyout cannot show an empty list — so the chevron must not promise one.
        function test_chevronHidesWhenThereIsNothingToExpand() {
            footerId.running = true
            footerId.taskCount = 0

            let chevron = find("updateRunningExpandIcon")
            verify(chevron !== null, "updateRunningExpandIcon not found")
            verify(!chevron.visible, "no chevron while the count is zero")

            footerId.taskCount = 2
            verify(chevron.visible, "it appears once there is something to list")
        }

        // Hovering the busy row is what peeks the task flyout open, so the footer
        // has to publish the hover — the host has no other way to see it.
        function test_hoveringTheBusyRowReportsIt() {
            footerId.running = true
            waitForRendering(footerId)

            let indicator = find("updateRunningIndicator")
            verify(indicator !== null, "updateRunningIndicator not found")

            mouseMove(indicator, indicator.width / 2, indicator.height / 2)
            tryVerify(function() { return footerId.busyRowHovered },
                      1000, "hovering the busy row is reported to the host")

            mouseMove(rootId, rootId.width - 1, 0)
            tryVerify(function() { return !footerId.busyRowHovered },
                      1000, "and the hover ending is reported too")
        }

        // The row is destroyed when work finishes, taking its HoverHandler with
        // it, so nothing would ever report the hover ending.
        function test_leavingBusyClearsTheHoverOutput() {
            footerId.running = true
            footerId.busyRowHovered = true

            footerId.running = false
            verify(!footerId.busy, "no longer busy")
            verify(!footerId.busyRowHovered, "the hover output cleared with the row")
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

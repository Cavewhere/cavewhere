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

        activity: ActiveTasks.Activity.Idle
    }

    SignalSpy {
        id: runSpy
        target: footerId
        signalName: "runRequested"
    }

    SignalSpy {
        id: tasksSpy
        target: footerId
        signalName: "tasksRequested"
    }

    TestCase {
        name: "SideBarUpdateFooter"
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
            // The activity and the count are pinned rather than left bound to
            // the live coordinator and task models, so a job the rest of the
            // suite happens to be running can't move them. tst_ActiveTasks
            // covers the defaults.
            footerId.activity = ActiveTasks.Activity.Idle
            RootData.updateCoordinator.automaticUpdate = false
            footerId.compact = false
            footerId.taskCount = 0
            footerId.progress = -1
            footerId.tasksShown = false
            footerId.busyRowHovered = false
            runSpy.clear()
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

        function test_togglingAutomaticUpdateWritesTheSharedSetting() {
            let checkbox = find("autoUpdateCheckbox")
            verify(checkbox !== null, "autoUpdateCheckbox not found")
            verify(!checkbox.checked, "starts off, matching the setting")

            // The click needs the layout polished, so its coordinates are real.
            waitForRendering(footerId)
            mouseClick(checkbox)

            verify(RootData.updateCoordinator.automaticUpdate,
                   "checking the box turns automatic update on")

            // Driven from the other end as well, which is the half that keeps
            // the four UIs of this one setting from drifting: the box has to
            // follow a change it did not make, the way it must when the phone
            // sheet or the hamburger menu is the one that wrote it.
            RootData.updateCoordinator.automaticUpdate = false
            verify(!checkbox.checked, "and follows the setting back")
        }

        function test_staleShowsRunAndAsksForTheUpdate() {
            footerId.activity = ActiveTasks.Activity.Stale
            waitForRendering(footerId)

            verify(find("autoUpdateCheckbox") === null, "the idle toggle gives way to Run")
            let runButton = find("updateRunButton")
            verify(runButton !== null, "updateRunButton not found")

            mouseClick(runButton)
            compare(runSpy.count, 1, "pressing Run asks for the update once")
        }

        // Busy is one state, not two overlaid: a pipeline re-edited mid-run is
        // both dirty and churning, and the footer shows the run rather than
        // offering a second one. The ranking that decides that now lives in
        // ActiveTasks.activity, so this asserts only what the footer does with
        // the answer — tst_ActiveTasks::test_busyOutranksStale has the ranking.
        function test_busyLeavesNoRunOnOffer() {
            footerId.activity = ActiveTasks.Activity.Busy

            verify(find("updateRunningIndicator") !== null, "the busy indicator is shown")
            verify(find("updateRunButton") === null, "Run is not offered while running")
        }

        // The count can be zero while work really is in flight, so the label has
        // to survive that; see the component header for how it happens.
        function test_runningLabelDropsTheCountWhenNoTaskIsListed() {
            footerId.activity = ActiveTasks.Activity.Busy

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
            footerId.activity = ActiveTasks.Activity.Busy
            waitForRendering(footerId)

            let indicator = find("updateRunningIndicator")
            verify(indicator !== null, "updateRunningIndicator not found")

            mouseClick(indicator)
            compare(tasksSpy.count, 1, "tapping the busy row asks for the tasks once")
        }

        // The chevron is the row's only cue that there is more behind it, and it
        // points at the flyout, so it lights up while that flyout is on screen.
        function test_busyRowShowsAnExpandChevron() {
            footerId.activity = ActiveTasks.Activity.Busy
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
            footerId.activity = ActiveTasks.Activity.Busy
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
            footerId.activity = ActiveTasks.Activity.Busy
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
            footerId.activity = ActiveTasks.Activity.Busy
            footerId.busyRowHovered = true

            footerId.activity = ActiveTasks.Activity.Idle
            verify(!footerId.busyRowHovered, "the hover output cleared with the row")
        }

        // The count has to live somewhere at every width: the label carries it at
        // Wide, and compact hides that label, so the ring takes it over.
        function test_compactMovesTheCountIntoTheRing() {
            footerId.activity = ActiveTasks.Activity.Busy
            footerId.taskCount = 3

            let ringCount = ObjectFinder.findObjectByChain(
                rootId, "rootId->updateFooter->updateRunningIndicator->progressRingCount")
            verify(ringCount !== null, "progressRingCount not found")
            verify(!ringCount.visible, "the label carries the count at full width")
            verify(find("updateRunningLabel").visible, "and that label is shown")

            footerId.compact = true
            verify(ringCount.visible, "compact moves the count into the ring")
            compare(ringCount.text, "3")
            verify(!find("updateRunningLabel").visible, "the label it replaced is hidden")
        }

        function test_compactHidesTheLabelsAndSwapsInTheIconToggle() {
            footerId.compact = true

            let label = find("autoUpdateLabel")
            verify(label !== null, "autoUpdateLabel not found")
            verify(!label.visible, "the caption is hidden when compact")
            verify(find("autoUpdateToggle").visible, "the icon toggle takes over when compact")

            footerId.activity = ActiveTasks.Activity.Stale
            verify(!find("updateNeededLabel").visible, "the stale caption is hidden when compact")
            verify(find("updateRunButton").visible, "Run stays reachable when compact")
        }
    }
}

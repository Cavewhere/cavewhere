import QtQuick as QQ
import QtTest
import QmlTestRecorder
import cavewherelib

QQ.Item {
    id: rootId
    objectName: "rootId"

    width: 300
    height: 60

    TaskStatusChip {
        id: chipId
        objectName: "chip"
        anchors.centerIn: parent

        activity: ActiveTasks.Activity.Idle
    }

    // Left entirely alone, since init() pins the chip above and an assignment is
    // what destroys the default binding this has to check.
    TaskStatusChip {
        id: defaultChipId
        objectName: "defaultChip"
    }

    SignalSpy {
        id: runSpy
        target: chipId
        signalName: "runRequested"
    }

    SignalSpy {
        id: tasksSpy
        target: chipId
        signalName: "tasksRequested"
    }

    TestCase {
        name: "TaskStatusChip"
        when: windowShown

        function init() {
            // Pinned rather than left bound to the live coordinator and task
            // models, so a job the rest of the suite happens to be running can't
            // move them.
            chipId.activity = ActiveTasks.Activity.Idle
            chipId.taskCount = 0
            chipId.progress = -1
            chipId.hostVisible = true
            runSpy.clear()
            tasksSpy.clear()
        }

        function find(name) {
            return ObjectFinder.findObjectByChain(rootId, "rootId->chip->" + name)
        }

        // Automatic Update already lives in the hamburger menu, so an idle chip
        // would be a second home for a setting that has one.
        function test_idleShowsNothingAtAll() {
            verify(!chipId.visible, "the chip is not on screen when idle")
            verify(find("chipRunningIndicator") === null, "no ring is built when idle")
            verify(find("chipPendingLabel") === null, "and no Update button either")
        }

        function test_staleOffersTheUpdate() {
            chipId.activity = ActiveTasks.Activity.Stale
            waitForRendering(chipId)

            verify(chipId.visible, "the chip appears")
            let label = find("chipPendingLabel")
            verify(label !== null, "chipPendingLabel not found")
            verify(find("chipRunningIndicator") === null, "the ring stays out of the stale state")

            mouseClick(chipId)
            compare(runSpy.count, 1, "tapping the chip runs the update once")
        }

        // The chip's ring is 14px, which cannot hold two digits inside it, so
        // the count sits beside the ring rather than in it.
        function test_busyShowsTheRingAndTheCountBesideIt() {
            chipId.activity = ActiveTasks.Activity.Busy
            chipId.taskCount = 12
            waitForRendering(chipId)

            let ring = find("chipRunningIndicator")
            verify(ring !== null, "chipRunningIndicator not found")
            verify(!ring.showCount, "the ring does not carry the count here")

            let count = find("chipRunningCount")
            verify(count !== null, "chipRunningCount not found")
            verify(count.visible, "the count is beside the ring")
            compare(count.text, "12")

            mouseClick(chipId)
            compare(tasksSpy.count, 1, "tapping the chip asks for the task list once")
        }

        // A cascade between pipelines is running with nothing registered yet.
        function test_aZeroCountIsNotShown() {
            chipId.activity = ActiveTasks.Activity.Busy
            chipId.taskCount = 0

            verify(find("chipRunningIndicator") !== null, "the ring still reports the work")
            verify(!find("chipRunningCount").visible, "but there is no number to show")
        }

        // The chip belongs to Narrow; the sidebar footer says all this at the
        // widths where there is a sidebar.
        function test_theHostDecidesWhetherTheChipBelongsHere() {
            chipId.activity = ActiveTasks.Activity.Busy
            verify(chipId.visible, "shown while the host wants it")

            chipId.hostVisible = false
            verify(!chipId.visible, "and gone when the host does not")

            // Not merely hidden: the ring is a QQuickRhiItem whose spin runs
            // whether or not anyone can see it, so a chip that is not on screen
            // must not be holding one.
            verify(find("chipRunningIndicator") === null,
                   "and it is not still running a ring behind the host's back")
        }

        // Busy with nothing registered yet is a real state, and there is no list
        // to open onto in it — an affordance that answers nothing is worse than
        // none.
        function test_theBusyChipIsNotTappableWithNothingToList() {
            chipId.activity = ActiveTasks.Activity.Busy
            chipId.taskCount = 0
            waitForRendering(chipId)

            mouseClick(chipId)
            compare(tasksSpy.count, 0, "nothing was asked for")

            chipId.taskCount = 2
            waitForRendering(chipId)

            mouseClick(chipId)
            compare(tasksSpy.count, 1, "and it answers again once there is a list")
        }

        // Nothing else checks that the chip is wired to the shared state at all;
        // every test above drives these inputs directly, and a chip that had
        // lost the defaults would stay blank on a phone — the gap CP7 exists to
        // close — while still passing all of them.
        function test_theChipDefaultsToTheSharedTaskState() {
            compare(defaultChipId.activity, ActiveTasks.activity, "activity follows ActiveTasks")
            compare(defaultChipId.taskCount, ActiveTasks.count, "as does the count")
            compare(defaultChipId.progress, ActiveTasks.progress, "and the progress")
        }
    }
}

import QtQuick as QQ
import QtTest
import QmlTestRecorder
import cavewherelib

QQ.Item {
    id: rootId
    objectName: "rootId"

    width: 400
    height: 400

    QQ.ListModel {
        id: testModelId
    }

    TaskFlyout {
        id: flyoutId
        objectName: "taskFlyout"
        anchors.left: parent.left
        anchors.bottom: parent.bottom

        model: testModelId
    }

    TestCase {
        name: "TaskFlyout"
        when: windowShown

        function addTask(name, progress, steps) {
            testModelId.append({nameRole: name, progressRole: progress, numberOfStepsRole: steps})
        }

        function init() {
            testModelId.clear()
            flyoutId.pinned = false
            flyoutId.dismissed = false
            flyoutId.previewHovered = false
            flyoutId.openLatched = false
            flyoutId.hostVisible = true
            // Park the pointer clear of the card, so a test that moved it onto
            // the panel can't leave it hovered for whatever runs next.
            mouseMove(rootId, rootId.width - 1, 0)
        }

        function find(name) {
            return ObjectFinder.findObjectByChain(rootId, "rootId->taskFlyout->" + name)
        }

        function test_closedFlyoutStaysHiddenEvenWithTasks() {
            addTask("Loading cave.laz", 0, 0)

            verify(!flyoutId.shown, "a closed flyout is not shown")
            verify(!flyoutId.visible, "a closed flyout is not visible")
        }

        // Hovering the control that owns the flyout peeks it open without a click.
        function test_hoveringThePreviewControlOpensIt() {
            addTask("Loading cave.laz", 0, 0)

            flyoutId.previewHovered = true
            verify(flyoutId.shown, "hovering peeks the flyout open")
            verify(!flyoutId.pinned, "peeking does not pin it")
        }

        // ...and it closes again when the pointer leaves, but only after the delay
        // that lets the pointer cross the gap onto the card.
        function test_unhoveringClosesItAfterTheDelay() {
            addTask("Loading cave.laz", 0, 0)
            flyoutId.previewHovered = true
            verify(flyoutId.shown, "open while hovered")

            flyoutId.previewHovered = false
            verify(flyoutId.shown, "still open immediately after the pointer leaves")
            tryVerify(function() { return !flyoutId.shown },
                      Theme.taskFlyoutHoverCloseDelay * 4,
                      "closes once the delay expires")
        }

        // The gap between the sidebar and the card is dead space, so the card has
        // to count its own hover — otherwise it would vanish mid-reach and could
        // never be read, scrolled, or closed.
        function test_theCardsOwnHoverHoldsItOpen() {
            addTask("Loading cave.laz", 0, 0)
            flyoutId.previewHovered = true
            waitForRendering(flyoutId)

            mouseMove(flyoutId, flyoutId.width / 2, flyoutId.height / 2)
            flyoutId.previewHovered = false

            wait(Theme.taskFlyoutHoverCloseDelay * 2)
            verify(flyoutId.shown, "the pointer resting on the card keeps it open")
        }

        // A click pins it, so it survives the pointer leaving entirely.
        function test_pinnedFlyoutSurvivesTheHoverEnding() {
            addTask("Loading cave.laz", 0, 0)
            flyoutId.previewHovered = true
            flyoutId.togglePin()
            verify(flyoutId.pinned, "clicking pins it")

            flyoutId.previewHovered = false
            wait(Theme.taskFlyoutHoverCloseDelay * 2)
            verify(flyoutId.shown, "a pinned flyout stays open with no hover")
        }

        // The click that unpins comes from a pointer that is still on the row, so
        // clearing the pin alone would leave the card sitting there — the whole
        // reason dismiss() suppresses the hover terms too.
        function test_clickingAgainClosesItEvenWhileStillHovered() {
            addTask("Loading cave.laz", 0, 0)
            flyoutId.previewHovered = true
            flyoutId.togglePin()

            flyoutId.togglePin()
            verify(!flyoutId.pinned, "the second click unpins it")
            verify(!flyoutId.shown, "and it closes at once, despite the pointer still being there")
        }

        // ...and it stays shut while the pointer stays put, rather than springing
        // back open on the hover that never ended.
        function test_aClosedFlyoutStaysShutUntilThePointerLeaves() {
            addTask("Loading cave.laz", 0, 0)
            flyoutId.previewHovered = true
            flyoutId.togglePin()
            flyoutId.togglePin()

            wait(Theme.taskFlyoutHoverCloseDelay * 2)
            verify(!flyoutId.shown, "still shut while the pointer has not moved")

            flyoutId.previewHovered = false
            flyoutId.previewHovered = true
            verify(flyoutId.shown, "leaving and coming back peeks it open again")
        }

        // The footer's busy row shows before any job registers, so the row is
        // clickable in a state the flyout cannot show. The click must not bank a
        // pin that fires on an unrelated job later.
        function test_clickingWithNoTasksDoesNotBankAPin() {
            flyoutId.togglePin()
            verify(!flyoutId.pinned, "nothing to pin open with no jobs listed")

            addTask("Saving", 0, 0)
            verify(!flyoutId.shown, "a later job does not open it unasked")
        }

        // The footer's busy row covers jobs the update coordinator never sees, so
        // the flyout it opens has to list them — this is the only place in the
        // main window that names a LAZ load or a save.
        function test_openWithTasksListsThem() {
            addTask("Loading cave.laz", 0, 0)
            addTask("Saving", 3, 10)
            flyoutId.togglePin()

            verify(flyoutId.shown, "an open flyout with tasks is shown")
            compare(flyoutId.taskCount, 2, "both jobs are counted")

            let list = find("taskFlyoutList")
            verify(list !== null, "taskFlyoutList not found")
            tryCompare(list, "count", 2)
        }

        // An empty list has nothing to say, so the card gets out of the way
        // rather than sitting on screen as an empty box.
        function test_flyoutFallsAwayWhenTheLastTaskFinishes() {
            addTask("Loading cave.laz", 0, 0)
            flyoutId.togglePin()
            verify(flyoutId.shown, "shown while a task is listed")

            testModelId.clear()

            compare(flyoutId.taskCount, 0, "no tasks remain")
            verify(!flyoutId.shown, "the flyout falls away with the last task")
        }

        // ...and it drops the pin on the way out, so an unrelated job minutes
        // later doesn't put the card back over the view with the pointer
        // nowhere near it.
        function test_theLastTaskFinishingDropsThePin() {
            addTask("Loading cave.laz", 0, 0)
            flyoutId.togglePin()
            testModelId.clear()

            verify(!flyoutId.pinned, "the pin goes with the work it was opened for")

            addTask("Saving", 0, 0)
            verify(!flyoutId.shown, "the next job does not re-open it unasked")
        }

        function test_titleCountsTheTasks() {
            addTask("Saving", 0, 0)
            flyoutId.togglePin()

            let title = find("flyoutCardTitle")
            verify(title !== null, "flyoutCardTitle not found")
            compare(title.text, "1 Task")

            addTask("Loading cave.laz", 0, 0)
            compare(title.text, "2 Tasks")
        }

        // Pressing × has to close the card there and then. The pointer that
        // pressed it is on the card, so it is one of the terms holding the card
        // open — clearing the pin alone leaves it exactly where it was.
        function test_closeButtonClosesTheCard() {
            addTask("Saving", 0, 0)
            flyoutId.togglePin()
            waitForRendering(flyoutId)

            let closeButton = find("flyoutCardCloseButton")
            verify(closeButton !== null, "flyoutCardCloseButton not found")

            mouseClick(closeButton)
            verify(!flyoutId.shown, "× closes the card immediately")
            verify(!flyoutId.visible, "and it leaves the screen")
            verify(!flyoutId.pinned, "closing drops the pin")
        }

        // The flyout hinges to the sidebar, so it goes away with it on narrow
        // layouts rather than floating against the page view.
        function test_hostCanHideItWithTheSidebar() {
            addTask("Saving", 0, 0)
            flyoutId.togglePin()
            flyoutId.hostVisible = false

            verify(flyoutId.shown, "still logically open")
            verify(!flyoutId.visible, "but not visible where the sidebar isn't")
        }
    }
}

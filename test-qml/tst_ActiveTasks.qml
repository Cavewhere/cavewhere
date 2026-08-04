import QtQuick as QQ
import QtTest
import QmlTestRecorder
import cavewherelib

QQ.Item {
    id: rootId
    objectName: "rootId"

    width: Theme.sidebarWidthFull
    height: 200

    // Stands in for the future manager, which cannot be given a job from QML.
    // ActiveTasks reads whatever model it is pointed at, so the only way to
    // drive it is to swap that model out and put it back.
    QQ.ListModel {
        id: fakeJobsId
    }

    // A stand-in that does report progress, so the fallback below can be watched
    // displacing a real number rather than the one it happened to start at.
    QQ.QtObject {
        id: measuredJobsId

        property int count: 1
        property real progress: 0.5
    }

    // No activity or taskCount of its own, so it reports whatever the shared
    // state says — which is the binding this file exists to hold in place.
    SideBarUpdateFooter {
        id: footerId
        objectName: "updateFooter"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
    }

    TestCase {
        name: "ActiveTasks"
        when: windowShown

        function init() {
            ActiveTasks.model = fakeJobsId
        }

        // ActiveTasks is a singleton, so the real manager has to go back before
        // anything else in the suite reads it.
        function cleanup() {
            ActiveTasks.model = Qt.binding(
                function() { return RootData.futureManagerModel })
            ActiveTasks.needsUpdate = Qt.binding(
                function() { return RootData.updateCoordinator.needsUpdate })
            fakeJobsId.clear()
        }

        function find(name) {
            return ObjectFinder.findObjectByChain(rootId, "rootId->updateFooter->" + name)
        }

        // Most jobs never reach the update coordinator — a save, a point-cloud
        // load, an import — so the coordinator alone cannot answer whether the
        // app is working.
        function test_aJobOutsideTheCoordinatorReadsAsBusy() {
            verify(!RootData.updateCoordinator.running, "the coordinator is idle")
            compare(ActiveTasks.count, 0, "and nothing is tracked yet")
            verify(!ActiveTasks.busy, "so nothing is happening")

            fakeJobsId.append({"nameRole": "Loading entrance.laz"})

            compare(ActiveTasks.count, 1, "the job is tracked")
            verify(ActiveTasks.busy, "a tracked job counts as busy")
        }

        // The footer takes the activity as an input so a test can drive it, which
        // only works because it defaults to the shared state. Nothing else checks
        // that default is still wired.
        function test_theUpdateFooterFollowsTheSharedActivity() {
            verify(find("updateRunningIndicator") === null, "idle to begin with")

            fakeJobsId.append({"nameRole": "Saving"})

            compare(footerId.activity, ActiveTasks.Activity.Busy,
                    "the footer picked up the shared state")
            verify(find("updateRunningIndicator") !== null, "and shows the busy row")
        }

        // A model with nothing to say about progress has to read as
        // indeterminate. QML leaves a real property holding its old value when a
        // binding evaluates to undefined, so without the fallback the ring would
        // keep drawing the last job's number over work nobody is measuring.
        function test_aModelThatCannotMeasureReadsAsIndeterminate() {
            ActiveTasks.model = measuredJobsId

            compare(ActiveTasks.progress, measuredJobsId.progress,
                    "a model that measures is read straight through")
            verify(ActiveTasks.progressKnown, "and drawn as a number")

            ActiveTasks.model = fakeJobsId
            fakeJobsId.append({"nameRole": "Saving"})

            compare(ActiveTasks.progress, ActiveTasks.indeterminateProgress,
                    "and a model that cannot measure falls back")
            verify(!ActiveTasks.progressKnown, "so there is nothing to draw")
        }

        // Busy outranks stale, and both the sidebar footer and the top bar chip
        // render that ranking — so it is decided once, here.
        function test_busyOutranksStale() {
            compare(ActiveTasks.activity, ActiveTasks.Activity.Idle,
                    "nothing to report to begin with")

            ActiveTasks.needsUpdate = true
            compare(ActiveTasks.activity, ActiveTasks.Activity.Stale,
                    "stale derived data is worth saying")

            fakeJobsId.append({"nameRole": "Saving"})
            compare(ActiveTasks.activity, ActiveTasks.Activity.Busy,
                    "but running wins while both are true")

            fakeJobsId.clear()
            compare(ActiveTasks.activity, ActiveTasks.Activity.Stale,
                    "and the stale state comes back when the work lands")
        }
    }
}

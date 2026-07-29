import QtQuick as QQ
import QtTest
import QmlTestRecorder
import cavewherelib

QQ.Item {
    id: rootId
    objectName: "rootId"

    width: Theme.sidebarWidthFull
    height: 200

    // Stands in for the task and future managers, neither of which can be given
    // a job from QML. ActiveTasks reads its own model, so the only way to drive
    // it is to swap that model's sources out and put them back.
    QQ.ListModel {
        id: fakeJobsId
    }

    // No activity or taskCount of its own, so it reports whatever the shared
    // state says — which is the binding this file exists to hold in place.
    SideBarUpdateFooter {
        id: footerId
        objectName: "updateFooter"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        automaticUpdate: false
    }

    TestCase {
        name: "ActiveTasks"
        when: windowShown

        function init() {
            ActiveTasks.model.models = [fakeJobsId]
        }

        // ActiveTasks is a singleton, so the real managers have to go back before
        // anything else in the suite reads it.
        function cleanup() {
            ActiveTasks.model.models = [RootData.taskManagerModel,
                                        RootData.futureManagerModel]
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

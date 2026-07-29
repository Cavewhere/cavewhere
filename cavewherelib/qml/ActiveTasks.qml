/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

pragma Singleton

import QtQuick as QQ
import cavewherelib

// Every tracked job in the app, as one model. The task manager and the future
// manager are separate sources; a QConcatenateTablesProxyModel joins them
// without copying rows. Resolved once here because three unrelated consumers
// need the same list — the update footer's busy count, the task flyout it
// opens, and the shutdown screen's TaskListView — and each instantiating its
// own proxy would let their idea of "what's running" drift apart.
QQ.QtObject {
    id: activeTasks

    // What the app has to say about background work, in the order it says it.
    enum Activity { Idle, Stale, Busy }

    readonly property TaskFutureCombineModel model: TaskFutureCombineModel {
        models: [RootData.taskManagerModel, RootData.futureManagerModel]
    }

    readonly property int count: activeTasks.model.count

    // Whether the app is working, in the widest sense: the update coordinator's
    // three derived-data pipelines, or any tracked job at all. It lives here
    // rather than in the sidebar footer that reads it because the footer is not
    // the only surface that has to answer this — at narrow widths there is no
    // sidebar at all. A cascade between pipelines is running with no future yet,
    // so this is true while the count is still zero.
    readonly property bool busy: RootData.updateCoordinator.running || activeTasks.count > 0

    // How far along, in [0, 1]. Only meaningful when progressKnown — read alone
    // it is negative whenever no job reports steps, and a progress bar handed
    // that will clamp it to empty and claim the work has not started.
    readonly property real progress: activeTasks.model.progress

    // Whether any job can say how far along it is. False is the signal to spin
    // rather than draw a number nothing measured.
    readonly property bool progressKnown: activeTasks.progress >= 0

    // Whether derived data is out of date — the three pipelines the update
    // coordinator owns, and nothing else. Defaulted rather than readonly, unlike
    // everything else here: nothing else in the ranking below can be made stale
    // from QML, so this is the only way the ranking itself can be exercised.
    property bool needsUpdate: RootData.updateCoordinator.needsUpdate

    // Busy outranks stale. A pipeline re-edited mid-run is both, and running is
    // the more useful thing to say: the update it needs is already under way, or
    // else something unrelated is, and either way pressing Run now would only
    // queue behind it. The accepted cost is that a long save masks the Run
    // button until it lands.
    //
    // The ranking lives here rather than in the sidebar footer that renders it,
    // because the footer is not the only surface that renders it — the top bar
    // chip shows the same three states at widths where there is no sidebar at
    // all, and two copies of a ranking is two places for it to drift.
    readonly property int activity: {
        if (activeTasks.busy) {
            return ActiveTasks.Activity.Busy
        }
        if (activeTasks.needsUpdate) {
            return ActiveTasks.Activity.Stale
        }
        return ActiveTasks.Activity.Idle
    }
}

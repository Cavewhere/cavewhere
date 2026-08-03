/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

pragma Singleton

import QtQuick as QQ
import cavewherelib

// Every tracked job in the app, under one name. Three unrelated consumers need
// the same list — the update footer's busy count, the task flyout it opens, and
// the shutdown screen's TaskListView — and naming it once here is what keeps
// their idea of "what's running" from drifting apart from the ranking below.
QQ.QtObject {
    id: activeTasks

    // What the app has to say about background work, in the order it says it.
    enum Activity { Idle, Stale, Busy }

    // Defaulted rather than readonly, like needsUpdate below, and typed QtObject
    // rather than FutureManagerModel for the same reason: the future manager can
    // only be given a job by starting real work, so standing a plain ListModel in
    // here is the only way a test can drive the count and the ranking off it.
    property QQ.QtObject model: RootData.futureManagerModel

    readonly property int count: activeTasks.model ? activeTasks.model.count : 0

    // Whether the app is working, in the widest sense: the update coordinator's
    // three derived-data pipelines, or any tracked job at all. It lives here
    // rather than in the sidebar footer that reads it because the footer is not
    // the only surface that has to answer this — at narrow widths there is no
    // sidebar at all. A cascade between pipelines is running with no future yet,
    // so this is true while the count is still zero.
    readonly property bool busy: RootData.updateCoordinator.running || activeTasks.count > 0

    // The sentinel cwFutureManagerModel::progress() reports when nothing running
    // can say how far along it is. Also what a stand-in model reads as, having
    // no progress of its own to give.
    readonly property real indeterminateProgress: -1

    // How far along, in [0, 1]. Only meaningful when progressKnown — read alone
    // it is negative whenever no job reports steps, and a progress bar handed
    // that will clamp it to empty and claim the work has not started.
    readonly property real progress: activeTasks.model && activeTasks.model.progress !== undefined
                                     ? activeTasks.model.progress
                                     : activeTasks.indeterminateProgress

    // Whether any job can say how far along it is. False is the signal to spin
    // rather than draw a number nothing measured.
    readonly property bool progressKnown: activeTasks.progress >= 0

    // Whether derived data is out of date — the three pipelines the update
    // coordinator owns, and nothing else. Defaulted rather than readonly, for
    // the same reason model is: nothing can be made stale from QML, so this is
    // the only way the stale half of the ranking can be exercised.
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

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

    readonly property TaskFutureCombineModel model: TaskFutureCombineModel {
        models: [RootData.taskManagerModel, RootData.futureManagerModel]
    }

    readonly property int count: activeTasks.model.count
}

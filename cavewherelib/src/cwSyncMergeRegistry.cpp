#include "cwSyncMergeRegistry.h"

#include "cwCaveSyncMergeHandler.h"
#include "cwCavingRegionSyncMergeHandler.h"
#include "cwNoteLiDARSyncMergeHandler.h"
#include "cwNoteSyncMergeHandler.h"
#include "cwTripSyncMergeHandler.h"

#include <QSet>

cwSyncMergeRegistry::cwSyncMergeRegistry()
{
    m_handlers.emplace_back(std::make_unique<cwCavingRegionSyncMergeHandler>());
    m_handlers.emplace_back(std::make_unique<cwCaveSyncMergeHandler>());
    m_handlers.emplace_back(std::make_unique<cwTripSyncMergeHandler>());
    m_handlers.emplace_back(std::make_unique<cwNoteLiDARSyncMergeHandler>());
    m_handlers.emplace_back(std::make_unique<cwNoteSyncMergeHandler>());
}

const cwSyncMergeRegistry& cwSyncMergeRegistry::instance()
{
    static const cwSyncMergeRegistry registry;
    return registry;
}

cwReconcileMergeResult cwSyncMergeRegistry::reconcile(const cwReconcileMergeContext& context) const
{
    cwReconcileMergeResult aggregateResult;
    QSet<QObject*> objectPathReadySet;
    QStringList appliedHandlers;

    // diskAlreadySynchronized starts optimistic: true.
    // Any applied handler that says diskAlreadySynchronized=false flips it to false.
    bool allHandlersDiskSynchronized = true;

    for (const auto& handler : m_handlers) {
        cwReconcileMergeResult result = handler->reconcile(context);

        // When the region handler detected a git-level project move, all other handlers'
        // files were also placed correctly by git — override their disk sync flag here
        // rather than repeating the check in every handler.
        if (context.gitProjectDirMoved) {
            result.diskAlreadySynchronized = true;
        }

        if (result.outcome == cwReconcileMergeResult::Outcome::NotApplicable) {
            continue;
        }

        if (result.handlerName.isEmpty()) {
            result.handlerName = handler->name();
        }

        if (result.outcome == cwReconcileMergeResult::Outcome::RequiresFullReload) {
            return result;
        }

        aggregateResult.outcome = cwReconcileMergeResult::Outcome::Applied;
        aggregateResult.modelMutated = aggregateResult.modelMutated || result.modelMutated;
        aggregateResult.pendingConflictCleanup =
            aggregateResult.pendingConflictCleanup || result.pendingConflictCleanup;
        aggregateResult.diagnostics.append(result.diagnostics);
        appliedHandlers.append(result.handlerName);
        // If a mutating handler did NOT synchronize disk, persistence is required.
        if (result.modelMutated && !result.diskAlreadySynchronized) {
            allHandlersDiskSynchronized = false;
        }
        for (QObject* object : std::as_const(result.objectsPathReady)) {
            if (object != nullptr) {
                objectPathReadySet.insert(object);
            }
        }
    }

    if (aggregateResult.outcome == cwReconcileMergeResult::Outcome::NotApplicable) {
        return aggregateResult;
    }

    aggregateResult.diskAlreadySynchronized =
        aggregateResult.modelMutated ? allHandlersDiskSynchronized : true;
    aggregateResult.handlerName = appliedHandlers.join(QStringLiteral(", "));
    aggregateResult.objectsPathReady.reserve(objectPathReadySet.size());
    for (QObject* object : std::as_const(objectPathReadySet)) {
        aggregateResult.objectsPathReady.append(object);
    }

    return aggregateResult;
}

#include "cwSyncMergeRegistry.h"

#include "cwNoteLiDARSyncMergeHandler.h"
#include "cwNoteSyncMergeHandler.h"

#include <QSet>

cwSyncMergeRegistry::cwSyncMergeRegistry()
{
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

    for (const auto& handler : m_handlers) {
        cwReconcileMergeResult result = handler->reconcile(context);
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
        aggregateResult.diagnostics.append(result.diagnostics);
        appliedHandlers.append(result.handlerName);
        for (QObject* object : std::as_const(result.objectsPathReady)) {
            if (object != nullptr) {
                objectPathReadySet.insert(object);
            }
        }
    }

    if (aggregateResult.outcome == cwReconcileMergeResult::Outcome::NotApplicable) {
        return aggregateResult;
    }

    aggregateResult.handlerName = appliedHandlers.join(QStringLiteral(", "));
    aggregateResult.objectsPathReady.reserve(objectPathReadySet.size());
    for (QObject* object : std::as_const(objectPathReadySet)) {
        aggregateResult.objectsPathReady.append(object);
    }

    return aggregateResult;
}

#include "cwSyncMergeRegistry.h"

#include "cwNoteSyncMergeHandler.h"

cwSyncMergeRegistry::cwSyncMergeRegistry()
{
    m_handlers.emplace_back(std::make_unique<cwNoteSyncMergeHandler>());
}

const cwSyncMergeRegistry& cwSyncMergeRegistry::instance()
{
    static const cwSyncMergeRegistry registry;
    return registry;
}

cwReconcileMergeResult cwSyncMergeRegistry::reconcile(const cwReconcileMergeContext& context) const
{
    for (const auto& handler : m_handlers) {
        cwReconcileMergeResult result = handler->reconcile(context);
        if (result.outcome == cwReconcileMergeResult::Outcome::NotApplicable) {
            continue;
        }

        if (result.handlerName.isEmpty()) {
            result.handlerName = handler->name();
        }

        return result;
    }

    return {};
}

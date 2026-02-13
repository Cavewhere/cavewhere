#include "cwCaveMergeApplier.h"

#include "cwCave.h"
#include "cwSyncMergeApplyUtils.h"

#include <optional>

Monad::ResultBase cwCaveMergeApplier::applyCaveMergePlan(const cwCaveMergePlan& plan)
{
    if (plan.currentCave == nullptr || plan.loadedCaveData == nullptr) {
        return Monad::ResultBase(QStringLiteral("Cave merge plan is missing required objects."));
    }

    if (plan.currentCave->id().isNull()
        || plan.loadedCaveData->id.isNull()
        || plan.currentCave->id() != plan.loadedCaveData->id) {
        return Monad::ResultBase(QStringLiteral("Cave merge plan requires matching non-null cave ids."));
    }

    const std::optional<QString> baseName = plan.baseCaveData.has_value()
        ? std::optional<QString>(plan.baseCaveData->name)
        : std::nullopt;

    plan.currentCave->setName(cwSyncMergeApplyUtils::chooseBundleValue(
        plan.currentCave->name(),
        plan.loadedCaveData->name,
        baseName,
        [](const QString& lhs, const QString& rhs) { return lhs == rhs; }));

    return Monad::ResultBase();
}

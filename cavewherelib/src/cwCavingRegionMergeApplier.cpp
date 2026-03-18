#include "cwCavingRegionMergeApplier.h"

#include "cwCavingRegion.h"
#include "cwSyncMergeApplyUtils.h"

Monad::ResultBase cwCavingRegionMergeApplier::applyRegionMergePlan(const cwCavingRegionMergePlan& plan)
{
    if (plan.currentRegion == nullptr || plan.loadedRegionData == nullptr) {
        return Monad::ResultBase(QStringLiteral("CavingRegion merge plan is missing required objects."));
    }

    plan.currentRegion->setName(cwSyncMergeApplyUtils::chooseBundleValue(
        plan.currentRegion->name(),
        plan.loadedRegionData->name,
        plan.baseName,
        [](const QString& lhs, const QString& rhs) { return lhs == rhs; }));

    return Monad::ResultBase();
}

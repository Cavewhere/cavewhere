#include "cwCavingRegionMergePlanBuilder.h"

#include "cwCavingRegion.h"

Monad::Result<cwCavingRegionMergePlan> cwCavingRegionMergePlanBuilder::build(
    cwCavingRegion* currentRegion,
    const cwCavingRegionData* loadedRegionData,
    std::optional<QString> baseName,
    std::optional<QList<cwExternalSource>> theirExternalSources,
    std::optional<QList<cwExternalSource>> baseExternalSources)
{
    if (currentRegion == nullptr) {
        return Monad::Result<cwCavingRegionMergePlan>(
            QStringLiteral("CavingRegion merge plan requires a non-null current region."));
    }

    if (loadedRegionData == nullptr) {
        return Monad::Result<cwCavingRegionMergePlan>(
            QStringLiteral("CavingRegion merge plan requires non-null loaded region data."));
    }

    cwCavingRegionMergePlan plan;
    plan.currentRegion = currentRegion;
    plan.loadedRegionData = loadedRegionData;
    plan.baseName = baseName;
    plan.theirExternalSources = std::move(theirExternalSources);
    plan.baseExternalSources = std::move(baseExternalSources);

    return Monad::Result<cwCavingRegionMergePlan>(plan);
}

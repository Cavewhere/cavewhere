#include "cwCavingRegionMergeApplier.h"

#include "cwCavingRegion.h"
#include "cwSyncMergeApplyUtils.h"

namespace {

const cwExternalSource* findSource(const QList<cwExternalSource>& sources, const QUuid& id)
{
    for (const cwExternalSource& source : sources) {
        if (source.id() == id) {
            return &source;
        }
    }
    return nullptr;
}

cwExternalSource mergeSource(const cwExternalSource& ours,
                             const cwExternalSource& theirs,
                             const cwExternalSource* base)
{
    cwExternalSource merged = theirs;
    const std::optional<QUuid> baseRoot = base != nullptr
            ? std::optional<QUuid>(base->rootNodeId())
            : std::nullopt;
    merged.setRootNodeId(cwSyncMergeApplyUtils::chooseBundleValue(
        ours.rootNodeId(),
        theirs.rootNodeId(),
        baseRoot,
        [](const QUuid& lhs, const QUuid& rhs) { return lhs == rhs; }));
    return merged;
}

} // namespace

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

    cwExternalSourceModel* sourceModel = plan.currentRegion->externalSources();
    const QList<cwExternalSource> merged = mergeExternalSources(
        sourceModel->sources(),
        plan.theirExternalSources.value_or(plan.loadedRegionData->externalSources),
        plan.baseExternalSources);
    if (merged != sourceModel->sources()) {
        sourceModel->setSources(merged);
    }

    return Monad::ResultBase();
}

QList<cwExternalSource> cwCavingRegionMergeApplier::mergeExternalSources(
    const QList<cwExternalSource>& ours,
    const QList<cwExternalSource>& theirs,
    const std::optional<QList<cwExternalSource>>& base)
{
    const QList<cwExternalSource> baseSources = base.value_or(QList<cwExternalSource>());

    QList<cwExternalSource> merged;
    for (const cwExternalSource& theirSource : theirs) {
        const cwExternalSource* ourSource = findSource(ours, theirSource.id());
        const cwExternalSource* baseSource = findSource(baseSources, theirSource.id());
        if (ourSource != nullptr) {
            merged.append(mergeSource(*ourSource, theirSource, baseSource));
        } else if (baseSource == nullptr) {
            merged.append(theirSource);
        }
    }

    for (const cwExternalSource& ourSource : ours) {
        const bool theirSide = findSource(theirs, ourSource.id()) != nullptr;
        const bool inBase = findSource(baseSources, ourSource.id()) != nullptr;
        if (!theirSide && !inBase) {
            merged.append(ourSource);
        }
    }

    return merged;
}

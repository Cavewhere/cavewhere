#include "cwCaveMergeApplier.h"

#include "cwCave.h"
#include "cwSyncMergeApplyUtils.h"

#include <optional>

namespace {

//! Three-way merges one scalar field of the node descriptor: the peer's value wins only
//! where the local side still holds the merge base's. \a currentValue reads the field off
//! the live node, \a field names the same field in the loaded and base descriptors.
template <class T, class CurrentFn>
T mergeScalar(const cwCaveMergePlan& plan, CurrentFn currentValue, T cwCaveData::*field)
{
    const std::optional<T> baseValue = plan.baseCaveData.has_value()
        ? std::optional<T>(plan.baseCaveData.value().*field)
        : std::nullopt;

    return cwSyncMergeApplyUtils::chooseBundleValue(
        currentValue(*plan.currentCave),
        plan.loadedCaveData->*field,
        baseValue,
        [](const T& lhs, const T& rhs) { return lhs == rhs; });
}

} // namespace

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

    plan.currentCave->setName(mergeScalar(
        plan,
        [](const cwCave& cave) { return cave.name(); },
        &cwCaveData::name));

    plan.currentCave->setKind(mergeScalar(
        plan,
        [](const cwCave& cave) { return cave.kind(); },
        &cwCaveData::kind));

    plan.currentCave->setReadOnly(mergeScalar(
        plan,
        [](const cwCave& cave) { return cave.isReadOnly(); },
        &cwCaveData::readOnly));

    plan.currentCave->setSourceId(mergeScalar(
        plan,
        [](const cwCave& cave) { return cave.sourceId(); },
        &cwCaveData::sourceId));

    plan.currentCave->setSourcePath(mergeScalar(
        plan,
        [](const cwCave& cave) { return cave.sourcePath(); },
        &cwCaveData::sourcePath));

    return Monad::ResultBase();
}

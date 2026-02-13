#include "cwTripMergeApplier.h"

#include "cwSaveLoad.h"
#include "cwTrip.h"
#include "cavewhere.pb.h"

#include <optional>

namespace {

template<typename T, typename IsEqualFn>
T chooseBundleValue(const T& currentValue,
                    const T& loadedValue,
                    const std::optional<T>& baseValue,
                    IsEqualFn isEqual)
{
    if (!baseValue.has_value()) {
        return loadedValue;
    }

    const bool currentMatchesBase = isEqual(currentValue, *baseValue);
    const bool loadedMatchesBase = isEqual(loadedValue, *baseValue);

    if (currentMatchesBase && !loadedMatchesBase) {
        return loadedValue;
    }
    if (!currentMatchesBase && loadedMatchesBase) {
        return currentValue;
    }
    if (!currentMatchesBase && !loadedMatchesBase) {
        return currentValue;
    }

    return currentValue;
}

std::unique_ptr<CavewhereProto::Trip> normalizedTripProtoForObject(const cwTrip* trip)
{
    auto protoTrip = cwSaveLoad::toProtoTrip(trip);
    protoTrip->clear_name();
    protoTrip->clear_date();
    return protoTrip;
}

std::unique_ptr<CavewhereProto::Trip> normalizedTripProtoForData(const cwTripData& tripData)
{
    cwTrip tempTrip;
    tempTrip.setData(tripData);
    auto protoTrip = cwSaveLoad::toProtoTrip(&tempTrip);
    protoTrip->clear_name();
    protoTrip->clear_date();
    return protoTrip;
}

bool hasOnlyNameAndDateDiff(const cwTrip* currentTrip, const cwTripData& loadedTripData)
{
    const auto currentNormalized = normalizedTripProtoForObject(currentTrip);
    const auto loadedNormalized = normalizedTripProtoForData(loadedTripData);
    return currentNormalized->SerializeAsString() == loadedNormalized->SerializeAsString();
}

} // namespace

bool cwTripMergeApplier::applyTripMergePlan(const cwTripMergePlan& plan, QString* failureReason)
{
    Q_ASSERT(plan.currentTrip != nullptr);
    Q_ASSERT(plan.loadedTripData != nullptr);
    if (plan.currentTrip == nullptr || plan.loadedTripData == nullptr) {
        if (failureReason != nullptr) {
            *failureReason = QStringLiteral("Trip merge plan is missing required objects.");
        }
        return false;
    }

    cwTrip* const currentTrip = plan.currentTrip;
    const cwTripData& loadedTripData = *plan.loadedTripData;

    if (!hasOnlyNameAndDateDiff(currentTrip, loadedTripData)) {
        if (failureReason != nullptr) {
            *failureReason = QStringLiteral("Trip subobject merge is not implemented for incremental trip merge.");
        }
        return false;
    }

    const std::optional<QString> baseName = plan.baseTripData.has_value()
        ? std::optional<QString>(plan.baseTripData->name)
        : std::nullopt;
    const std::optional<QDateTime> baseDate = plan.baseTripData.has_value()
        ? std::optional<QDateTime>(plan.baseTripData->date)
        : std::nullopt;

    currentTrip->setName(chooseBundleValue(
        currentTrip->name(),
        loadedTripData.name,
        baseName,
        [](const QString& lhs, const QString& rhs) { return lhs == rhs; }));

    currentTrip->setDate(chooseBundleValue(
        currentTrip->date(),
        loadedTripData.date,
        baseDate,
        [](const QDateTime& lhs, const QDateTime& rhs) { return lhs == rhs; }));

    return true;
}


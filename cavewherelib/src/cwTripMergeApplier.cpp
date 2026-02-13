#include "cwTripMergeApplier.h"

#include "cwSaveLoad.h"
#include "cwSyncMergeApplyUtils.h"
#include "cwTrip.h"
#include "cwTripCalibrationMergeApplier.h"
#include "cwTripCalibrationMergePlanBuilder.h"
#include "cavewhere.pb.h"

#include <optional>

namespace {

std::unique_ptr<CavewhereProto::Trip> normalizedTripProtoForObject(const cwTrip* trip)
{
    auto protoTrip = cwSaveLoad::toProtoTrip(trip);
    protoTrip->clear_name();
    protoTrip->clear_date();
    protoTrip->clear_tripcalibration();
    return protoTrip;
}

std::unique_ptr<CavewhereProto::Trip> normalizedTripProtoForData(const cwTripData& tripData)
{
    cwTrip tempTrip;
    tempTrip.setData(tripData);
    auto protoTrip = cwSaveLoad::toProtoTrip(&tempTrip);
    protoTrip->clear_name();
    protoTrip->clear_date();
    protoTrip->clear_tripcalibration();
    return protoTrip;
}

bool hasOnlyNameAndDateDiff(const cwTrip* currentTrip, const cwTripData& loadedTripData)
{
    const auto currentNormalized = normalizedTripProtoForObject(currentTrip);
    const auto loadedNormalized = normalizedTripProtoForData(loadedTripData);
    return currentNormalized->SerializeAsString() == loadedNormalized->SerializeAsString();
}

} // namespace

Monad::ResultBase cwTripMergeApplier::applyTripMergePlan(const cwTripMergePlan& plan)
{
    Q_ASSERT(plan.currentTrip != nullptr);
    Q_ASSERT(plan.loadedTripData != nullptr);
    if (plan.currentTrip == nullptr || plan.loadedTripData == nullptr) {
        return Monad::ResultBase(QStringLiteral("Trip merge plan is missing required objects."));
    }

    cwTrip* const currentTrip = plan.currentTrip;
    const cwTripData& loadedTripData = *plan.loadedTripData;

    if (!hasOnlyNameAndDateDiff(currentTrip, loadedTripData)) {
        return Monad::ResultBase(
            QStringLiteral("Trip subobject merge is not implemented for incremental trip merge."));
    }

    const std::optional<QString> baseName = plan.baseTripData.has_value()
        ? std::optional<QString>(plan.baseTripData->name)
        : std::nullopt;
    const std::optional<QDateTime> baseDate = plan.baseTripData.has_value()
        ? std::optional<QDateTime>(plan.baseTripData->date)
        : std::nullopt;
    const std::optional<cwTripCalibrationData> baseCalibration = plan.baseTripData.has_value()
        ? std::optional<cwTripCalibrationData>(plan.baseTripData->calibrations)
        : std::nullopt;

    currentTrip->setName(cwSyncMergeApplyUtils::chooseBundleValue(
        currentTrip->name(),
        loadedTripData.name,
        baseName,
        [](const QString& lhs, const QString& rhs) { return lhs == rhs; }));

    currentTrip->setDate(cwSyncMergeApplyUtils::chooseBundleValue(
        currentTrip->date(),
        loadedTripData.date,
        baseDate,
        [](const QDateTime& lhs, const QDateTime& rhs) { return lhs == rhs; }));

    const auto calibrationMergePlan = cwTripCalibrationMergePlanBuilder::build(
        currentTrip->calibrations(),
        &loadedTripData.calibrations,
        baseCalibration);
    if (calibrationMergePlan.hasError()) {
        return Monad::ResultBase(calibrationMergePlan.errorMessage());
    }

    const auto applyCalibrationResult =
        cwTripCalibrationMergeApplier::applyTripCalibrationMergePlan(calibrationMergePlan.value());
    if (applyCalibrationResult.hasError()) {
        return Monad::ResultBase(applyCalibrationResult.errorMessage());
    }

    return Monad::ResultBase();
}

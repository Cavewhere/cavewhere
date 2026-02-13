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

    QString calibrationPlanFailureReason;
    const auto calibrationMergePlan = cwTripCalibrationMergePlanBuilder::build(
        currentTrip->calibrations(),
        &loadedTripData.calibrations,
        baseCalibration,
        &calibrationPlanFailureReason);
    if (!calibrationMergePlan.has_value()) {
        if (failureReason != nullptr) {
            *failureReason = calibrationPlanFailureReason.isEmpty()
                                 ? QStringLiteral("Trip calibration merge plan is missing required objects.")
                                 : calibrationPlanFailureReason;
        }
        return false;
    }

    if (!cwTripCalibrationMergeApplier::applyTripCalibrationMergePlan(*calibrationMergePlan)) {
        if (failureReason != nullptr) {
            *failureReason = QStringLiteral("Trip calibration merge apply failed.");
        }
        return false;
    }

    return true;
}

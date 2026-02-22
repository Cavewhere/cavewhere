#include "cwTripCalibrationMergeApplier.h"

#include "cwSyncMergeApplyUtils.h"

namespace {

bool doublesEqual(double lhs, double rhs)
{
    return qFuzzyCompare(1.0 + lhs, 1.0 + rhs);
}

} // namespace

Monad::ResultBase cwTripCalibrationMergeApplier::applyTripCalibrationMergePlan(const cwTripCalibrationMergePlan& plan)
{
    if (plan.currentCalibration == nullptr || plan.loadedCalibrationData == nullptr) {
        return Monad::ResultBase(QStringLiteral("Trip calibration merge plan is missing required objects."));
    }

    const cwTripCalibrationData currentCalibrationData = plan.currentCalibration->data();
    const cwTripCalibrationData& loadedCalibrationData = *plan.loadedCalibrationData;
    const std::optional<cwTripCalibrationData> baseCalibrationData = plan.baseCalibrationData;

    cwTripCalibrationData mergedData = currentCalibrationData;

    mergedData.setCorrectedCompassBacksight(cwSyncMergeApplyUtils::chooseBundleValue(
        currentCalibrationData.hasCorrectedCompassBacksight(),
        loadedCalibrationData.hasCorrectedCompassBacksight(),
        baseCalibrationData.has_value()
            ? std::optional<bool>(baseCalibrationData->hasCorrectedCompassBacksight())
            : std::nullopt,
        [](bool lhs, bool rhs) { return lhs == rhs; },
        plan.applyMode));

    mergedData.setCorrectedClinoBacksight(cwSyncMergeApplyUtils::chooseBundleValue(
        currentCalibrationData.hasCorrectedClinoBacksight(),
        loadedCalibrationData.hasCorrectedClinoBacksight(),
        baseCalibrationData.has_value()
            ? std::optional<bool>(baseCalibrationData->hasCorrectedClinoBacksight())
            : std::nullopt,
        [](bool lhs, bool rhs) { return lhs == rhs; },
        plan.applyMode));

    mergedData.setCorrectedCompassFrontsight(cwSyncMergeApplyUtils::chooseBundleValue(
        currentCalibrationData.hasCorrectedCompassFrontsight(),
        loadedCalibrationData.hasCorrectedCompassFrontsight(),
        baseCalibrationData.has_value()
            ? std::optional<bool>(baseCalibrationData->hasCorrectedCompassFrontsight())
            : std::nullopt,
        [](bool lhs, bool rhs) { return lhs == rhs; },
        plan.applyMode));

    mergedData.setCorrectedClinoFrontsight(cwSyncMergeApplyUtils::chooseBundleValue(
        currentCalibrationData.hasCorrectedClinoFrontsight(),
        loadedCalibrationData.hasCorrectedClinoFrontsight(),
        baseCalibrationData.has_value()
            ? std::optional<bool>(baseCalibrationData->hasCorrectedClinoFrontsight())
            : std::nullopt,
        [](bool lhs, bool rhs) { return lhs == rhs; },
        plan.applyMode));

    mergedData.setTapeCalibration(cwSyncMergeApplyUtils::chooseBundleValue(
        currentCalibrationData.tapeCalibration(),
        loadedCalibrationData.tapeCalibration(),
        baseCalibrationData.has_value()
            ? std::optional<double>(baseCalibrationData->tapeCalibration())
            : std::nullopt,
        doublesEqual,
        plan.applyMode));

    mergedData.setFrontCompassCalibration(cwSyncMergeApplyUtils::chooseBundleValue(
        currentCalibrationData.frontCompassCalibration(),
        loadedCalibrationData.frontCompassCalibration(),
        baseCalibrationData.has_value()
            ? std::optional<double>(baseCalibrationData->frontCompassCalibration())
            : std::nullopt,
        doublesEqual,
        plan.applyMode));

    mergedData.setFrontClinoCalibration(cwSyncMergeApplyUtils::chooseBundleValue(
        currentCalibrationData.frontClinoCalibration(),
        loadedCalibrationData.frontClinoCalibration(),
        baseCalibrationData.has_value()
            ? std::optional<double>(baseCalibrationData->frontClinoCalibration())
            : std::nullopt,
        doublesEqual,
        plan.applyMode));

    mergedData.setBackCompassCalibration(cwSyncMergeApplyUtils::chooseBundleValue(
        currentCalibrationData.backCompassCalibration(),
        loadedCalibrationData.backCompassCalibration(),
        baseCalibrationData.has_value()
            ? std::optional<double>(baseCalibrationData->backCompassCalibration())
            : std::nullopt,
        doublesEqual,
        plan.applyMode));

    mergedData.setBackClinoCalibration(cwSyncMergeApplyUtils::chooseBundleValue(
        currentCalibrationData.backClinoCalibration(),
        loadedCalibrationData.backClinoCalibration(),
        baseCalibrationData.has_value()
            ? std::optional<double>(baseCalibrationData->backClinoCalibration())
            : std::nullopt,
        doublesEqual,
        plan.applyMode));

    mergedData.setDeclination(cwSyncMergeApplyUtils::chooseBundleValue(
        currentCalibrationData.declination(),
        loadedCalibrationData.declination(),
        baseCalibrationData.has_value()
            ? std::optional<double>(baseCalibrationData->declination())
            : std::nullopt,
        doublesEqual,
        plan.applyMode));

    mergedData.setDistanceUnit(cwSyncMergeApplyUtils::chooseBundleValue(
        currentCalibrationData.distanceUnit(),
        loadedCalibrationData.distanceUnit(),
        baseCalibrationData.has_value()
            ? std::optional<cwUnits::LengthUnit>(baseCalibrationData->distanceUnit())
            : std::nullopt,
        [](cwUnits::LengthUnit lhs, cwUnits::LengthUnit rhs) { return lhs == rhs; },
        plan.applyMode));

    mergedData.setFrontSights(cwSyncMergeApplyUtils::chooseBundleValue(
        currentCalibrationData.hasFrontSights(),
        loadedCalibrationData.hasFrontSights(),
        baseCalibrationData.has_value()
            ? std::optional<bool>(baseCalibrationData->hasFrontSights())
            : std::nullopt,
        [](bool lhs, bool rhs) { return lhs == rhs; },
        plan.applyMode));

    mergedData.setBackSights(cwSyncMergeApplyUtils::chooseBundleValue(
        currentCalibrationData.hasBackSights(),
        loadedCalibrationData.hasBackSights(),
        baseCalibrationData.has_value()
            ? std::optional<bool>(baseCalibrationData->hasBackSights())
            : std::nullopt,
        [](bool lhs, bool rhs) { return lhs == rhs; },
        plan.applyMode));

    plan.currentCalibration->setData(mergedData);
    return Monad::ResultBase();
}

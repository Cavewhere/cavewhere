#include "cwNoteMergeApplier.h"

#include "cwImage.h"
#include "cwImageResolution.h"
#include "cwNote.h"
#include "cwSyncMergeApplyUtils.h"

#include <optional>

namespace {

bool imageResolutionDataEqual(const cwImageResolution::Data& lhs, const cwImageResolution::Data& rhs)
{
    return lhs.unit == rhs.unit
           && lhs.value == rhs.value
           && lhs.updateValueWhenUnitChanged == rhs.updateValueWhenUnitChanged;
}

} // namespace

Monad::ResultBase cwNoteMergeApplier::applyNoteMergePlan(const cwNoteMergePlan& plan)
{
    Q_ASSERT(plan.currentNote != nullptr);
    Q_ASSERT(plan.loadedNoteData != nullptr);
    if (plan.currentNote == nullptr || plan.loadedNoteData == nullptr) {
        return Monad::ResultBase(QStringLiteral("Note merge plan is missing required objects."));
    }

    cwNote* const currentNote = plan.currentNote;
    const cwNoteData& loadedNoteData = *plan.loadedNoteData;

    const std::optional<cwImage> baseImage = plan.baseNoteData.has_value()
        ? std::optional<cwImage>(plan.baseNoteData->image)
        : std::nullopt;
    const std::optional<cwImageResolution::Data> baseImageResolution = plan.baseNoteData.has_value()
        ? std::optional<cwImageResolution::Data>(plan.baseNoteData->imageResolution)
        : std::nullopt;
    const std::optional<double> baseRotate = plan.baseNoteData.has_value()
        ? std::optional<double>(plan.baseNoteData->rotate)
        : std::nullopt;

    currentNote->setName(loadedNoteData.name);
    currentNote->setId(loadedNoteData.id);

    currentNote->setImage(cwSyncMergeApplyUtils::chooseBundleValue(
        currentNote->image(),
        loadedNoteData.image,
        baseImage,
        [](const cwImage& lhs, const cwImage& rhs) { return lhs == rhs; }));

    currentNote->imageResolution()->setData(cwSyncMergeApplyUtils::chooseBundleValue(
        currentNote->imageResolution()->data(),
        loadedNoteData.imageResolution,
        baseImageResolution,
        imageResolutionDataEqual));

    currentNote->setRotate(cwSyncMergeApplyUtils::chooseBundleValue(
        currentNote->rotate(),
        loadedNoteData.rotate,
        baseRotate,
        [](double lhs, double rhs) { return lhs == rhs; }));

    return Monad::ResultBase();
}

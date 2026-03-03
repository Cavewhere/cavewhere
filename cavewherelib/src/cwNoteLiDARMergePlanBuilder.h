#pragma once

#include "CaveWhereLibExport.h"
#include "cwNoteLiDARData.h"
#include "cwSurveyNoteLiDARModelData.h"
#include "cwSyncMergeApplyUtils.h"
#include "Monad/Result.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QUuid>
#include <QString>

#include <optional>

class cwNoteLiDAR;
class cwSurveyNoteLiDARModel;

enum class cwNoteLiDARDescriptorApplyMode {
    FullModelReplace,
    IncrementalMerge,
    Ambiguous
};

struct cwNoteLiDARMergePlan {
    cwNoteLiDAR* currentNote = nullptr;
    const cwNoteLiDARData* loadedNoteData = nullptr;
    std::optional<cwNoteLiDARData> baseNoteData;
    cwSyncMergeApplyUtils::ApplyMode applyMode = cwSyncMergeApplyUtils::ApplyMode::ThreeWayMerge;
};

struct cwNoteLiDARMergePreparation {
    QList<cwNoteLiDARMergePlan> plans;
    QList<QObject*> orderedNotes;
};

class CAVEWHERE_LIB_EXPORT cwNoteLiDARMergePlanBuilder
{
public:
    static cwNoteLiDARDescriptorApplyMode determineApplyMode(
        cwSurveyNoteLiDARModel* noteLiDARModel,
        const cwSurveyNoteLiDARModelData& loadedNoteLiDARModelData);

    static Monad::Result<cwNoteLiDARMergePreparation> build(
        cwSurveyNoteLiDARModel* noteLiDARModel,
        const cwSurveyNoteLiDARModelData& loadedNoteLiDARModelData,
        const QHash<QUuid, cwNoteLiDARData>& baseNoteLiDARByNoteId,
        cwSyncMergeApplyUtils::ApplyMode applyMode = cwSyncMergeApplyUtils::ApplyMode::ThreeWayMerge);
};

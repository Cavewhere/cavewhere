#pragma once

#include "cwNoteLiDARData.h"
#include "cwSurveyNoteLiDARModelData.h"
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
};

struct cwNoteLiDARMergePreparation {
    QList<cwNoteLiDARMergePlan> plans;
    QList<QObject*> orderedNotes;
};

class cwNoteLiDARMergePlanBuilder
{
public:
    static cwNoteLiDARDescriptorApplyMode determineApplyMode(
        cwSurveyNoteLiDARModel* noteLiDARModel,
        const cwSurveyNoteLiDARModelData& loadedNoteLiDARModelData);

    static Monad::Result<cwNoteLiDARMergePreparation> build(
        cwSurveyNoteLiDARModel* noteLiDARModel,
        const cwSurveyNoteLiDARModelData& loadedNoteLiDARModelData,
        const QHash<QUuid, cwNoteLiDARData>& baseNoteLiDARByNoteId);
};

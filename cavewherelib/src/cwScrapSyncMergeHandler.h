#pragma once

#include <QList>
#include <QHash>
#include <QSet>
#include <QtCore/quuid.h>

#include "cwLead.h"
#include "cwNoteStation.h"

#include <optional>
#include <vector>

class cwNote;
class cwSurveyNoteModel;
struct cwNoteData;
struct cwSurveyNoteModelData;

struct cwScrapBaseIdentityData {
    QSet<QUuid> stationIds;
    QSet<QUuid> leadIds;
    QHash<QUuid, cwNoteStation> stationsById;
    QHash<QUuid, cwLead> leadsById;
};

struct cwNoteStructuralMergePlan {
    cwNote* note = nullptr;
    const cwNoteData* loadedNoteData = nullptr;
    QHash<QUuid, cwScrapBaseIdentityData> baseScrapIdentityByScrapId;
    std::vector<QUuid> mergedScrapOrder;
};

struct cwNoteStructuralMergePreparation {
    QList<cwNoteStructuralMergePlan> plans;
    QList<cwNote*> orderedNotes;
};

class cwScrapSyncMergeHandler
{
public:
    static std::optional<cwNoteStructuralMergePreparation> buildNoteStructuralMergePreparation(
        cwSurveyNoteModel* noteModel,
        const cwSurveyNoteModelData& loadedNoteModelData);

    static void applyNoteStructuralMergePlan(const cwNoteStructuralMergePlan& plan);
};

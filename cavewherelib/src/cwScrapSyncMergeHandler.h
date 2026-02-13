#pragma once

#include <QList>
#include <QHash>
#include <QSet>
#include <QPolygonF>
#include <QtCore/quuid.h>

#include "cwLead.h"
#include "cwNoteTransformationData.h"
#include "cwNoteStation.h"
#include "cwScrapType.h"

#include <optional>
#include <vector>

class cwNote;
class cwSurveyNoteModel;
struct cwNoteData;
struct cwSurveyNoteModelData;

struct cwScrapBaseIdentityData {
    struct GeometryData {
        QPolygonF outlinePoints;
        struct TransformBundle {
            cwNoteTransformationData noteTransformation;
            bool calculateNoteTransform = false;
            cwScrapType::Type viewType = cwScrapType::Plan;
            bool hasProjectedProfileView = false;
            double projectedAzimuth = 0.0;
            int projectedDirection = 0;
        } transform;
    };

    QSet<QUuid> stationIds;
    QSet<QUuid> leadIds;
    QHash<QUuid, cwNoteStation> stationsById;
    QHash<QUuid, cwLead> leadsById;
    bool hasGeometryData = false;
    GeometryData geometry;
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

    static bool applyNoteStructuralMergePlan(const cwNoteStructuralMergePlan& plan);
};

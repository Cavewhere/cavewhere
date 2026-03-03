#pragma once

#include "CaveWhereLibExport.h"
#include "Monad/Result.h"
#include "cwCaveData.h"

#include <QHash>
#include <QList>
#include <QUuid>

#include <optional>

class cwCave;

struct cwCaveMergePlan {
    cwCave* currentCave = nullptr;
    const cwCaveData* loadedCaveData = nullptr;
    std::optional<cwCaveData> baseCaveData;
};

struct cwCaveMergePreparation {
    QList<cwCaveMergePlan> plans;
};

class CAVEWHERE_LIB_EXPORT cwCaveMergePlanBuilder
{
public:
    static Monad::Result<cwCaveMergePreparation> build(
        const QList<cwCave*>& currentCaves,
        const QList<const cwCaveData*>& loadedCaves,
        const QHash<QUuid, cwCaveData>& baseCaveById);
};

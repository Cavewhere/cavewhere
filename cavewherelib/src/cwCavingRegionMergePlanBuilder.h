#pragma once

#include "CaveWhereLibExport.h"
#include "Monad/Result.h"
#include "cwCavingRegionData.h"

#include <optional>

class cwCavingRegion;

struct cwCavingRegionMergePlan {
    cwCavingRegion* currentRegion = nullptr;
    const cwCavingRegionData* loadedRegionData = nullptr;
    std::optional<QString> baseName;
};

class CAVEWHERE_LIB_EXPORT cwCavingRegionMergePlanBuilder
{
public:
    static Monad::Result<cwCavingRegionMergePlan> build(
        cwCavingRegion* currentRegion,
        const cwCavingRegionData* loadedRegionData,
        std::optional<QString> baseName = std::nullopt);
};

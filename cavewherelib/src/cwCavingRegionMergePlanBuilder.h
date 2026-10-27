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
    //! The peer's sources, when they differ from loadedRegionData's: a merge
    //! commit that resolved the project file with our lines holds ours on disk.
    std::optional<QList<cwExternalSource>> theirExternalSources;
    //! The merge base's sources; absent when the base is unknown.
    std::optional<QList<cwExternalSource>> baseExternalSources;
};

class CAVEWHERE_LIB_EXPORT cwCavingRegionMergePlanBuilder
{
public:
    static Monad::Result<cwCavingRegionMergePlan> build(
        cwCavingRegion* currentRegion,
        const cwCavingRegionData* loadedRegionData,
        std::optional<QString> baseName = std::nullopt,
        std::optional<QList<cwExternalSource>> theirExternalSources = std::nullopt,
        std::optional<QList<cwExternalSource>> baseExternalSources = std::nullopt);
};

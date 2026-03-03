#pragma once

#include "CaveWhereLibExport.h"
#include "cwTeamMergeApplier.h"
#include "cwTeamMergePlanBuilder.h"

class CAVEWHERE_LIB_EXPORT cwTeamSyncMergeHandler
{
public:
    static Monad::Result<cwTeamMergePlan> buildTeamMergePlan(
        cwTeam* currentTeam,
        const cwTeamData* loadedTeamData,
        const std::optional<cwTeamData>& baseTeamData,
        cwSyncMergeApplyUtils::ApplyMode applyMode = cwSyncMergeApplyUtils::ApplyMode::ThreeWayMerge);

    static Monad::ResultBase applyTeamMergePlan(const cwTeamMergePlan& plan);
};

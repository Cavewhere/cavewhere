#pragma once

#include "cwTeamMergeApplier.h"
#include "cwTeamMergePlanBuilder.h"

class cwTeamSyncMergeHandler
{
public:
    static Monad::Result<cwTeamMergePlan> buildTeamMergePlan(
        cwTeam* currentTeam,
        const cwTeamData* loadedTeamData,
        const std::optional<cwTeamData>& baseTeamData);

    static Monad::ResultBase applyTeamMergePlan(const cwTeamMergePlan& plan);
};


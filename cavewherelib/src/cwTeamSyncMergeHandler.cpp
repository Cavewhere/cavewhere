#include "cwTeamSyncMergeHandler.h"

Monad::Result<cwTeamMergePlan> cwTeamSyncMergeHandler::buildTeamMergePlan(
    cwTeam* currentTeam,
    const cwTeamData* loadedTeamData,
    const std::optional<cwTeamData>& baseTeamData)
{
    return cwTeamMergePlanBuilder::build(currentTeam, loadedTeamData, baseTeamData);
}

Monad::ResultBase cwTeamSyncMergeHandler::applyTeamMergePlan(const cwTeamMergePlan& plan)
{
    return cwTeamMergeApplier::applyTeamMergePlan(plan);
}


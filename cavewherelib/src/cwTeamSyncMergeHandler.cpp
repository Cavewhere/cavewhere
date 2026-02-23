#include "cwTeamSyncMergeHandler.h"

Monad::Result<cwTeamMergePlan> cwTeamSyncMergeHandler::buildTeamMergePlan(
    cwTeam* currentTeam,
    const cwTeamData* loadedTeamData,
    const std::optional<cwTeamData>& baseTeamData,
    cwSyncMergeApplyUtils::ApplyMode applyMode)
{
    return cwTeamMergePlanBuilder::build(currentTeam, loadedTeamData, baseTeamData, applyMode);
}

Monad::ResultBase cwTeamSyncMergeHandler::applyTeamMergePlan(const cwTeamMergePlan& plan)
{
    return cwTeamMergeApplier::applyTeamMergePlan(plan);
}

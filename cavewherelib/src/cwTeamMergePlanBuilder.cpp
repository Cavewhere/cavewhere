#include "cwTeamMergePlanBuilder.h"

Monad::Result<cwTeamMergePlan> cwTeamMergePlanBuilder::build(
    cwTeam* currentTeam,
    const cwTeamData* loadedTeamData,
    const std::optional<cwTeamData>& baseTeamData)
{
    if (currentTeam == nullptr || loadedTeamData == nullptr) {
        return Monad::Result<cwTeamMergePlan>(QStringLiteral("Team merge plan is missing required objects."));
    }

    cwTeamMergePlan plan;
    plan.currentTeam = currentTeam;
    plan.loadedTeamData = loadedTeamData;
    plan.baseTeamData = baseTeamData;
    return Monad::Result<cwTeamMergePlan>(plan);
}


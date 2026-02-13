#pragma once

#include "cwTeamData.h"
#include "Monad/Result.h"

#include <optional>

class cwTeam;

struct cwTeamMergePlan {
    cwTeam* currentTeam = nullptr;
    const cwTeamData* loadedTeamData = nullptr;
    std::optional<cwTeamData> baseTeamData;
};

class cwTeamMergePlanBuilder
{
public:
    static Monad::Result<cwTeamMergePlan> build(
        cwTeam* currentTeam,
        const cwTeamData* loadedTeamData,
        const std::optional<cwTeamData>& baseTeamData);
};


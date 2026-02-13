#include "cwTeamMergeApplier.h"

#include "cwSyncIdUtils.h"
#include "cwSyncMergeApplyUtils.h"
#include "cwTeam.h"

namespace {

cwTeamMember mergeSharedMember(const cwTeamMember& currentMember,
                               const cwTeamMember& loadedMember,
                               const std::optional<cwTeamMember>& baseMember)
{
    cwTeamMember mergedMember = currentMember;
    mergedMember.setName(cwSyncMergeApplyUtils::chooseBundleValue(
        currentMember.name(),
        loadedMember.name(),
        baseMember.has_value() ? std::optional<QString>(baseMember->name()) : std::nullopt,
        [](const QString& lhs, const QString& rhs) { return lhs == rhs; }));
    mergedMember.setJobs(cwSyncMergeApplyUtils::chooseBundleValue(
        currentMember.jobs(),
        loadedMember.jobs(),
        baseMember.has_value() ? std::optional<QStringList>(baseMember->jobs()) : std::nullopt,
        [](const QStringList& lhs, const QStringList& rhs) { return lhs == rhs; }));
    return mergedMember;
}

} // namespace

Monad::ResultBase cwTeamMergeApplier::applyTeamMergePlan(const cwTeamMergePlan& plan)
{
    if (plan.currentTeam == nullptr || plan.loadedTeamData == nullptr) {
        return Monad::ResultBase(QStringLiteral("Team merge plan is missing required objects."));
    }

    const QList<cwTeamMember> currentMembers = plan.currentTeam->teamMembers();
    const QList<cwTeamMember>& loadedMembers = plan.loadedTeamData->members;
    const std::optional<QList<cwTeamMember>> baseMembers =
        plan.baseTeamData.has_value() ? std::optional<QList<cwTeamMember>>(plan.baseTeamData->members)
                                      : std::nullopt;

    const auto mergedMembersResult = cwSyncIdUtils::buildMergedOrderedList<cwTeamMember>(
        currentMembers,
        loadedMembers,
        baseMembers,
        [](const cwTeamMember& member) { return member.id(); },
        [](const cwTeamMember& currentMember,
           const cwTeamMember& loadedMember,
           const std::optional<cwTeamMember>& baseMember) {
            return mergeSharedMember(currentMember, loadedMember, baseMember);
        },
        [](const std::vector<QUuid>& /*currentIds*/,
           const std::vector<QUuid>& loadedIds,
           const std::optional<std::vector<QUuid>>& /*baseIds*/) {
            return loadedIds;
        },
        QStringLiteral("team member list"));
    if (mergedMembersResult.hasError()) {
        const QString errorMessage = mergedMembersResult.errorMessage();
        if (errorMessage == QStringLiteral("Ambiguous ids in team member list.")) {
            return Monad::ResultBase(QStringLiteral("Ambiguous team member ids."));
        }
        if (errorMessage == QStringLiteral("Ambiguous base ids in team member list.")) {
            return Monad::ResultBase(QStringLiteral("Ambiguous base team member ids."));
        }
        if (errorMessage == QStringLiteral("Ambiguous ordered ids in team member list.")) {
            return Monad::ResultBase(QStringLiteral("Ambiguous ordered team member ids."));
        }
        if (errorMessage == QStringLiteral("Failed to build deterministic merged order for team member list.")) {
            return Monad::ResultBase(QStringLiteral("Failed to build deterministic team member order."));
        }
        return Monad::ResultBase(errorMessage);
    }

    plan.currentTeam->setTeamMembers(mergedMembersResult.value());
    return Monad::ResultBase();
}

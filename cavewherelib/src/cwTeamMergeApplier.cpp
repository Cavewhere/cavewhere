#include "cwTeamMergeApplier.h"

#include "cwSyncMergeApplyUtils.h"
#include "cwTeam.h"

#include <QHash>
#include <QSet>

namespace {

std::optional<QHash<QUuid, cwTeamMember>> keyedMembersById(const QList<cwTeamMember>& members)
{
    QHash<QUuid, cwTeamMember> membersById;
    membersById.reserve(members.size());
    for (const cwTeamMember& member : members) {
        const QUuid id = member.id();
        if (id.isNull() || membersById.contains(id)) {
            return std::nullopt;
        }
        membersById.insert(id, member);
    }
    return membersById;
}

std::optional<QList<QUuid>> orderedUniqueIds(const QList<cwTeamMember>& members)
{
    QList<QUuid> ids;
    ids.reserve(members.size());
    QSet<QUuid> seenIds;
    for (const cwTeamMember& member : members) {
        const QUuid id = member.id();
        if (id.isNull() || seenIds.contains(id)) {
            return std::nullopt;
        }
        seenIds.insert(id);
        ids.append(id);
    }
    return ids;
}

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

    const auto currentById = keyedMembersById(currentMembers);
    const auto loadedById = keyedMembersById(loadedMembers);
    if (!currentById.has_value() || !loadedById.has_value()) {
        return Monad::ResultBase(QStringLiteral("Ambiguous team member ids."));
    }

    std::optional<QHash<QUuid, cwTeamMember>> baseById;
    if (baseMembers.has_value()) {
        baseById = keyedMembersById(*baseMembers);
        if (!baseById.has_value()) {
            return Monad::ResultBase(QStringLiteral("Ambiguous base team member ids."));
        }
    }

    const auto currentIds = orderedUniqueIds(currentMembers);
    const auto loadedIds = orderedUniqueIds(loadedMembers);
    if (!currentIds.has_value() || !loadedIds.has_value()) {
        return Monad::ResultBase(QStringLiteral("Ambiguous ordered team member ids."));
    }

    QHash<QUuid, cwTeamMember> mergedById;
    mergedById.reserve(currentById->size() + loadedById->size());

    QSet<QUuid> allIds;
    for (auto it = currentById->cbegin(); it != currentById->cend(); ++it) {
        allIds.insert(it.key());
    }
    for (auto it = loadedById->cbegin(); it != loadedById->cend(); ++it) {
        allIds.insert(it.key());
    }

    for (const QUuid& id : allIds) {
        const auto currentIt = currentById->constFind(id);
        const auto loadedIt = loadedById->constFind(id);
        const bool hasCurrent = currentIt != currentById->cend();
        const bool hasLoaded = loadedIt != loadedById->cend();
        const bool hasBase = baseById.has_value() && baseById->contains(id);

        if (hasCurrent && hasLoaded) {
            const std::optional<cwTeamMember> baseMember = hasBase
                                                               ? std::optional<cwTeamMember>(baseById->value(id))
                                                               : std::nullopt;
            mergedById.insert(id, mergeSharedMember(*currentIt, *loadedIt, baseMember));
            continue;
        }

        if (hasCurrent) {
            // Preserve local-only members, and local delete-vs-remote-modify conflicts.
            mergedById.insert(id, *currentIt);
            continue;
        }

        if (hasLoaded && !hasBase) {
            // Remote-only add.
            mergedById.insert(id, *loadedIt);
        }
    }

    QList<QUuid> mergedOrder;
    mergedOrder.reserve(mergedById.size());
    QSet<QUuid> seenOrderIds;
    for (const QUuid& id : *loadedIds) {
        if (mergedById.contains(id) && !seenOrderIds.contains(id)) {
            mergedOrder.append(id);
            seenOrderIds.insert(id);
        }
    }
    for (const QUuid& id : *currentIds) {
        if (mergedById.contains(id) && !seenOrderIds.contains(id)) {
            mergedOrder.append(id);
            seenOrderIds.insert(id);
        }
    }

    if (mergedOrder.size() != mergedById.size()) {
        return Monad::ResultBase(QStringLiteral("Failed to build deterministic team member order."));
    }

    QList<cwTeamMember> mergedMembers;
    mergedMembers.reserve(mergedOrder.size());
    for (const QUuid& id : mergedOrder) {
        mergedMembers.append(mergedById.value(id));
    }

    plan.currentTeam->setTeamMembers(mergedMembers);
    return Monad::ResultBase();
}


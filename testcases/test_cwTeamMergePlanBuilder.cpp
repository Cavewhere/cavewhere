// Catch includes
#include <catch2/catch_test_macros.hpp>
using namespace Catch;

// Our includes
#include "cwTeam.h"
#include "cwTeamData.h"
#include "cwTeamMember.h"
#include "cwTeamSyncMergeHandler.h"

namespace {

cwTeamMember makeMember(const QUuid& id, const QString& name, const QStringList& jobs)
{
    cwTeamMember member;
    member.setId(id);
    member.setName(name);
    member.setJobs(jobs);
    return member;
}

} // namespace

TEST_CASE("cwTeam merge plan builder validates required objects", "[cwTeamMerge][sync]")
{
    cwTeam team;
    cwTeamData loadedData;

    const auto missingCurrent = cwTeamSyncMergeHandler::buildTeamMergePlan(nullptr, &loadedData, {});
    CHECK(missingCurrent.hasError());

    const auto missingLoaded = cwTeamSyncMergeHandler::buildTeamMergePlan(&team, nullptr, {});
    CHECK(missingLoaded.hasError());
}

TEST_CASE("cwTeam merge applier merges team members by stable id with ordered jobs", "[cwTeamMerge][sync]")
{
    cwTeam currentTeam;

    const QUuid aliceId = QUuid::createUuid();
    const QUuid bobId = QUuid::createUuid();
    const QUuid carolId = QUuid::createUuid();
    const QUuid daveId = QUuid::createUuid();

    const cwTeamMember aliceBase = makeMember(aliceId, QStringLiteral("Alice"), {QStringLiteral("Sketch"), QStringLiteral("Survey")});
    const cwTeamMember bobBase = makeMember(bobId, QStringLiteral("Bob"), {QStringLiteral("Lead")});
    currentTeam.setTeamMembers({aliceBase, bobBase});
    const cwTeamData baseData = currentTeam.data();

    cwTeamMember aliceCurrent = aliceBase;
    cwTeamMember bobCurrent = bobBase;
    bobCurrent.setJobs({QStringLiteral("Lead"), QStringLiteral("Draft")}); // Local-only change from base.
    const cwTeamMember carolCurrent = makeMember(carolId, QStringLiteral("Carol"), {QStringLiteral("Photos")}); // Local add.
    currentTeam.setTeamMembers({carolCurrent, aliceCurrent, bobCurrent});

    cwTeamMember aliceLoaded = aliceBase;
    aliceLoaded.setName(QStringLiteral("Alice Remote")); // Remote-only change from base.
    aliceLoaded.setJobs({QStringLiteral("Survey"), QStringLiteral("Sketch")}); // Remote-only order change from base.
    cwTeamMember bobLoaded = bobBase;
    bobLoaded.setJobs({QStringLiteral("Lead"), QStringLiteral("Remote Role")}); // Conflict with local.
    const cwTeamMember daveLoaded = makeMember(daveId, QStringLiteral("Dave"), {QStringLiteral("Cartography")}); // Remote add.

    cwTeamData loadedData;
    loadedData.members = {bobLoaded, aliceLoaded, daveLoaded};

    const auto planResult = cwTeamSyncMergeHandler::buildTeamMergePlan(&currentTeam, &loadedData, baseData);
    REQUIRE_FALSE(planResult.hasError());
    REQUIRE_FALSE(cwTeamSyncMergeHandler::applyTeamMergePlan(planResult.value()).hasError());

    const QList<cwTeamMember> mergedMembers = currentTeam.teamMembers();
    REQUIRE(mergedMembers.size() == 4);

    CHECK(mergedMembers[0].id() == bobId);
    CHECK(mergedMembers[1].id() == aliceId);
    CHECK(mergedMembers[2].id() == daveId);
    CHECK(mergedMembers[3].id() == carolId);

    CHECK(mergedMembers[0].jobs() == QStringList({QStringLiteral("Lead"), QStringLiteral("Draft")}));
    CHECK(mergedMembers[1].name() == QStringLiteral("Alice Remote"));
    CHECK(mergedMembers[1].jobs() == QStringList({QStringLiteral("Survey"), QStringLiteral("Sketch")}));
    CHECK(mergedMembers[2].name() == QStringLiteral("Dave"));
    CHECK(mergedMembers[3].name() == QStringLiteral("Carol"));
}

TEST_CASE("cwTeam merge applier rejects ambiguous member ids", "[cwTeamMerge][sync]")
{
    cwTeam currentTeam;
    const QUuid sharedId = QUuid::createUuid();
    currentTeam.setTeamMembers({makeMember(sharedId, QStringLiteral("Current"), {QStringLiteral("Lead")})});

    cwTeamData loadedData;
    loadedData.members = {
        makeMember(sharedId, QStringLiteral("Loaded A"), {QStringLiteral("Sketch")}),
        makeMember(sharedId, QStringLiteral("Loaded B"), {QStringLiteral("Survey")})
    };

    const auto planResult = cwTeamSyncMergeHandler::buildTeamMergePlan(&currentTeam, &loadedData, {});
    REQUIRE_FALSE(planResult.hasError());

    const auto applyResult = cwTeamSyncMergeHandler::applyTeamMergePlan(planResult.value());
    CHECK(applyResult.hasError());
    CHECK(applyResult.errorMessage() == QStringLiteral("Ambiguous team member ids."));
}

